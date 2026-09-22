#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Entity.h"
#include "bcad/index/ISpatialIndex.h"
#include <memory>
#include <vector>

namespace bcad::index {

// Quadtree "lâche" (loose) sur les boîtes englobantes des entités. Il sert à
// la fois aux requêtes de région et au picking CPU, sans dépendre du renderer.
class QuadtreeIndex final : public ISpatialIndex {
public:
    explicit QuadtreeIndex(geom::BoundingBox worldBounds, int maxItemsPerNode = 8, int maxDepth = 10);
    ~QuadtreeIndex() override;

    void insert(geom::Entity* entity) override;
    void remove(geom::Entity* entity) override;
    void update(geom::Entity* entity) override;

    void clear() override;
    void rebuild(const std::vector<geom::Entity*>& entities);

    std::vector<geom::Entity*> query(const geom::BoundingBox& region) const override;

    std::size_t size() const { return entityCount_; }
    const geom::BoundingBox& bounds() const { return worldBounds_; }

    struct Node;

private:
    geom::BoundingBox worldBounds_;
    int maxItemsPerNode_;
    int maxDepth_;
    std::unique_ptr<Node> root_;
    std::size_t entityCount_ = 0;
};

} // namespace bcad::index
