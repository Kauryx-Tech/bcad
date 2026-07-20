#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Entity.h"
#include <memory>
#include <vector>

namespace bcad::render {

// Loose quadtree over entity bounding boxes. Backbone of both viewport
// culling (query the visible-area bbox) and pick/selection (query a small
// bbox around the cursor) — see architecture doc's "Spatial Indexing".
class Quadtree {
public:
    explicit Quadtree(geom::BoundingBox worldBounds, int maxItemsPerNode = 8, int maxDepth = 10);
    ~Quadtree();

    void insert(geom::Entity* entity);
    void remove(geom::Entity* entity);
    // Call after an entity already in the tree moved/resized.
    void update(geom::Entity* entity);

    void clear();
    void rebuild(const std::vector<geom::Entity*>& entities);

    // All entities whose bbox intersects the query region (superset —
    // exact hit-testing is the caller's job via Entity::distanceTo).
    std::vector<geom::Entity*> query(const geom::BoundingBox& region) const;

    std::size_t size() const { return entityCount_; }
    const geom::BoundingBox& bounds() const { return worldBounds_; }

    // Opaque node type: forward-declared here, fully defined only in
    // Quadtree.cpp (its free-function tree-walking helpers need access, and
    // C++ has no "friend the whole translation unit" — public+opaque is
    // simpler than befriending each helper individually).
    struct Node;

private:
    geom::BoundingBox worldBounds_;
    int maxItemsPerNode_;
    int maxDepth_;
    std::unique_ptr<Node> root_;
    std::size_t entityCount_ = 0;
};

} // namespace bcad::render
