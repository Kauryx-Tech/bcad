#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/layers/LayerManager.h"
#include "bcad/render/Quadtree.h"
#include "bcad/render/TessellationTypes.h"
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace bcad::core {

// The in-memory model of one drawing: entities, their layers, and the
// spatial index kept in sync with both. Everything else (renderer, tools,
// DXF/SQLite I/O) operates on a Document.
class Document {
public:
    Document();

    geom::Entity* addEntity(std::unique_ptr<geom::Entity> entity);
    void removeEntity(int id);
    geom::Entity* findEntity(int id) const;

    // Call after mutating an entity already owned by the document (move,
    // rotate, vertex edit, ...) so the spatial index stays correct.
    void notifyEntityChanged(geom::Entity* entity);

    const std::vector<std::unique_ptr<geom::Entity>>& entities() const { return entities_; }

    std::vector<geom::Entity*> entitiesInRegion(const geom::BoundingBox& region) const;

    // Nearest entity to `p` within `tolerance` world units, or nullptr.
    // Locked/hidden layers are skipped.
    geom::Entity* pickEntity(const geom::Point2& p, double tolerance) const;

    geom::BoundingBox extents() const;

    layers::LayerManager& layerManager() { return layers_; }
    const layers::LayerManager& layerManager() const { return layers_; }

    const render::Quadtree& spatialIndex() const { return *index_; }

    void clear();

    // Builds GPU-ready vertex batches for every visible entity intersecting
    // `region`, grouped by resolved color. Safe to call from a background
    // tessellation thread while the GUI thread edits the document: reads
    // take a shared lock, mutations (add/remove/notifyEntityChanged) take
    // an exclusive one.
    render::TessellationResult buildTessellation(const geom::BoundingBox& region, double tolerance) const;

    // Fired on any structural change (add/remove/transform); GUI hooks in
    // here to trigger a repaint instead of polling every frame.
    std::function<void()> onChanged;

private:
    void notifyChanged();

    std::vector<std::unique_ptr<geom::Entity>> entities_;
    std::unordered_map<int, geom::Entity*> byId_;
    layers::LayerManager layers_;
    std::unique_ptr<render::Quadtree> index_;
    int nextId_ = 1;
    mutable std::shared_mutex mutex_;
};

} // namespace bcad::core
