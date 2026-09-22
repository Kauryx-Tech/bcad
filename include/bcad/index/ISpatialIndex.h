#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Entity.h"
#include <memory>
#include <vector>

namespace bcad::index {

// Abstraction d'index spatial consommée par le Core. Les implémentations
// concrètes (quadtree, octree, grille, backend accéléré) restent substituables.
class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;

    virtual void insert(geom::Entity* entity) = 0;
    virtual void remove(geom::Entity* entity) = 0;
    virtual void update(geom::Entity* entity) = 0;
    virtual void clear() = 0;

    virtual std::vector<geom::Entity*> query(const geom::BoundingBox& region) const = 0;
};

// Factory function to create the default spatial index implementation.
// This allows the Core to create an index without depending on concrete implementations.
std::unique_ptr<ISpatialIndex> createDefaultSpatialIndex(const geom::BoundingBox& worldBounds);

} // namespace bcad::index
