#include "bcad/core/Document.h"

#include <algorithm>
#include <limits>
#include <map>
#include <tuple>

namespace bcad::core {

using geom::BoundingBox;
using geom::Entity;
using geom::Point2;

namespace {
// Valeur par défaut généreuse afin que les premières insertions, avant que
// les vraies dimensions ne soient connues, ne déclenchent pas immédiatement
// un redimensionnement de la racine ; le Quadtree grandit de toute façon à
// la demande.
BoundingBox defaultWorldBounds() {
    return BoundingBox{ -1000.0, -1000.0, 1000.0, 1000.0 };
}
} // namespace

Document::Document() : index_(std::make_unique<render::Quadtree>(defaultWorldBounds())) {}

Entity* Document::addEntity(std::unique_ptr<Entity> entity) {
    Entity* raw = nullptr;
    {
        std::unique_lock lock(mutex_);
        entity->setId(nextId_++);
        if (entity->layer().empty() || layers_.find(entity->layer()) == nullptr) {
            entity->setLayer(layers_.currentLayerName());
        }
        raw = entity.get();
        byId_[raw->id()] = raw;
        entities_.push_back(std::move(entity));
        index_->insert(raw);
    }
    notifyChanged();
    return raw;
}

void Document::removeEntity(int id) {
    bool removed = false;
    {
        std::unique_lock lock(mutex_);
        auto it = byId_.find(id);
        if (it != byId_.end()) {
            Entity* raw = it->second;
            index_->remove(raw);
            byId_.erase(it);
            entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                            [&](const auto& e) { return e->id() == id; }),
                             entities_.end());
            removed = true;
        }
    }
    if (removed) notifyChanged();
}

Entity* Document::findEntity(int id) const {
    std::shared_lock lock(mutex_);
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

void Document::notifyEntityChanged(Entity* entity) {
    {
        std::unique_lock lock(mutex_);
        index_->update(entity);
    }
    notifyChanged();
}

std::vector<Entity*> Document::entitiesInRegion(const BoundingBox& region) const {
    std::shared_lock lock(mutex_);
    auto hits = index_->query(region);
    std::vector<Entity*> visible;
    visible.reserve(hits.size());
    for (Entity* e : hits) {
        const layers::Layer* layer = layers_.find(e->layer());
        if (layer && layer->visible) visible.push_back(e);
    }
    return visible;
}

Entity* Document::pickEntity(const Point2& p, double tolerance) const {
    std::shared_lock lock(mutex_);
    BoundingBox region = BoundingBox::fromCenterHalfExtent(p, tolerance);
    auto candidates = index_->query(region);

    Entity* best = nullptr;
    double bestDist = std::numeric_limits<double>::infinity();
    for (Entity* e : candidates) {
        const layers::Layer* layer = layers_.find(e->layer());
        if (!layer || !layer->visible || layer->locked) continue;
        double d = e->distanceTo(p);
        if (d <= tolerance && d < bestDist) {
            bestDist = d;
            best = e;
        }
    }
    return best;
}

BoundingBox Document::extents() const {
    std::shared_lock lock(mutex_);
    BoundingBox bb;
    for (const auto& e : entities_) bb.expand(e->boundingBox());
    return bb;
}

void Document::clear() {
    {
        std::unique_lock lock(mutex_);
        entities_.clear();
        byId_.clear();
        index_->clear();
        nextId_ = 1;
    }
    notifyChanged();
}

render::TessellationResult Document::buildTessellation(const BoundingBox& region, double tolerance) const {
    std::shared_lock lock(mutex_);

    render::TessellationResult result;
    result.region = region;
    result.toleranceUsed = tolerance;

    // Regrouper d'abord par couleur résolue pour que le moteur de rendu
    // puisse émettre un seul appel glMultiDrawArrays par couleur au lieu
    // d'un appel de dessin par entité.
    struct ColorKey {
        float r, g, b, a;
        bool operator<(const ColorKey& o) const {
            return std::tie(r, g, b, a) < std::tie(o.r, o.g, o.b, o.a);
        }
    };
    std::map<ColorKey, render::ColorBatch> batches;

    for (Entity* e : index_->query(region)) {
        const layers::Layer* layer = layers_.find(e->layer());
        if (!layer || !layer->visible) continue;

        geom::Color color = e->colorOverride().value_or(layer->color);
        ColorKey key{ color.r, color.g, color.b, color.a };
        auto& batch = batches[key];
        batch.color = color;

        std::vector<Point2> pts = e->tessellate(tolerance);
        if (pts.size() < 2) continue;

        batch.firsts.push_back(static_cast<std::int32_t>(batch.vertices.size() / 2));
        batch.counts.push_back(static_cast<std::int32_t>(pts.size()));
        for (const auto& p : pts) {
            batch.vertices.push_back(static_cast<float>(CGAL::to_double(p.x())));
            batch.vertices.push_back(static_cast<float>(CGAL::to_double(p.y())));
        }
    }

    result.batches.reserve(batches.size());
    for (auto& [key, batch] : batches) result.batches.push_back(std::move(batch));
    return result;
}

void Document::notifyChanged() {
    if (onChanged) onChanged();
}

} // namespace bcad::core
