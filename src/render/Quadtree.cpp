#include "bcad/render/Quadtree.h"

#include <algorithm>
#include <array>
#include <unordered_map>

namespace bcad::render {

using geom::BoundingBox;
using geom::Entity;

struct Quadtree::Node {
    BoundingBox bounds;
    int depth = 0;
    std::array<std::unique_ptr<Node>, 4> children;
    std::vector<std::pair<Entity*, BoundingBox>> items;

    bool isLeaf() const { return children[0] == nullptr; }
};

namespace {

// Which quadrant (0..3) fully contains `box`, or -1 if it straddles the
// split lines and must be kept at the parent level.
int quadrantFor(const BoundingBox& nodeBounds, const BoundingBox& box) {
    double midX = (nodeBounds.minX + nodeBounds.maxX) / 2.0;
    double midY = (nodeBounds.minY + nodeBounds.maxY) / 2.0;
    bool fitsLeft = box.maxX <= midX;
    bool fitsRight = box.minX >= midX;
    bool fitsBottom = box.maxY <= midY;
    bool fitsTop = box.minY >= midY;
    if (fitsLeft && fitsBottom) return 0;
    if (fitsRight && fitsBottom) return 1;
    if (fitsLeft && fitsTop) return 2;
    if (fitsRight && fitsTop) return 3;
    return -1;
}

BoundingBox childBounds(const BoundingBox& nodeBounds, int quadrant) {
    double midX = (nodeBounds.minX + nodeBounds.maxX) / 2.0;
    double midY = (nodeBounds.minY + nodeBounds.maxY) / 2.0;
    switch (quadrant) {
        case 0: return { nodeBounds.minX, nodeBounds.minY, midX, midY };
        case 1: return { midX, nodeBounds.minY, nodeBounds.maxX, midY };
        case 2: return { nodeBounds.minX, midY, midX, nodeBounds.maxY };
        default: return { midX, midY, nodeBounds.maxX, nodeBounds.maxY };
    }
}

} // namespace

Quadtree::Quadtree(BoundingBox worldBounds, int maxItemsPerNode, int maxDepth)
    : worldBounds_(worldBounds), maxItemsPerNode_(maxItemsPerNode), maxDepth_(maxDepth) {
    root_ = std::make_unique<Node>();
    root_->bounds = worldBounds_;
    root_->depth = 0;
}

Quadtree::~Quadtree() = default;

namespace {

void insertInto(Quadtree::Node* node, Entity* entity, const BoundingBox& box,
                 int maxItemsPerNode, int maxDepth) {
    if (!node->isLeaf()) {
        int q = quadrantFor(node->bounds, box);
        if (q >= 0) {
            insertInto(node->children[q].get(), entity, box, maxItemsPerNode, maxDepth);
            return;
        }
        node->items.emplace_back(entity, box);
        return;
    }

    node->items.emplace_back(entity, box);

    if (static_cast<int>(node->items.size()) > maxItemsPerNode && node->depth < maxDepth) {
        for (int q = 0; q < 4; ++q) {
            node->children[q] = std::make_unique<Quadtree::Node>();
            node->children[q]->bounds = childBounds(node->bounds, q);
            node->children[q]->depth = node->depth + 1;
        }
        std::vector<std::pair<Entity*, BoundingBox>> remaining;
        for (auto& [ent, b] : node->items) {
            int q = quadrantFor(node->bounds, b);
            if (q >= 0) {
                insertInto(node->children[q].get(), ent, b, maxItemsPerNode, maxDepth);
            } else {
                remaining.emplace_back(ent, b);
            }
        }
        node->items = std::move(remaining);
    }
}

bool removeFrom(Quadtree::Node* node, Entity* entity) {
    auto it = std::find_if(node->items.begin(), node->items.end(),
                            [&](const auto& pair) { return pair.first == entity; });
    if (it != node->items.end()) {
        node->items.erase(it);
        return true;
    }
    if (!node->isLeaf()) {
        for (auto& child : node->children) {
            if (removeFrom(child.get(), entity)) return true;
        }
    }
    return false;
}

void queryInto(const Quadtree::Node* node, const BoundingBox& region, std::vector<Entity*>& out) {
    if (!node->bounds.intersects(region)) return;
    for (const auto& [entity, box] : node->items) {
        if (box.intersects(region)) out.push_back(entity);
    }
    if (!node->isLeaf()) {
        for (const auto& child : node->children) queryInto(child.get(), region, out);
    }
}

} // namespace

void Quadtree::insert(Entity* entity) {
    BoundingBox box = entity->boundingBox();
    // Entities that fall (partially) outside the tracked world bounds still
    // need to be findable, so grow the root rather than dropping them.
    if (!worldBounds_.contains(box)) {
        worldBounds_.expand(box);
        root_->bounds = worldBounds_;
    }
    insertInto(root_.get(), entity, box, maxItemsPerNode_, maxDepth_);
    ++entityCount_;
}

void Quadtree::remove(Entity* entity) {
    if (removeFrom(root_.get(), entity)) {
        --entityCount_;
    }
}

void Quadtree::update(Entity* entity) {
    remove(entity);
    insert(entity);
}

void Quadtree::clear() {
    root_ = std::make_unique<Node>();
    root_->bounds = worldBounds_;
    entityCount_ = 0;
}

void Quadtree::rebuild(const std::vector<Entity*>& entities) {
    clear();
    for (Entity* e : entities) insert(e);
}

std::vector<Entity*> Quadtree::query(const BoundingBox& region) const {
    std::vector<Entity*> out;
    queryInto(root_.get(), region, out);
    return out;
}

} // namespace bcad::render
