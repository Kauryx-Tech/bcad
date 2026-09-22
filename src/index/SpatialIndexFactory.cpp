#include "bcad/index/ISpatialIndex.h"
#include "bcad/index/QuadtreeIndex.h"

namespace bcad::index {

std::unique_ptr<ISpatialIndex> createDefaultSpatialIndex(const geom::BoundingBox& worldBounds) {
    return std::make_unique<QuadtreeIndex>(worldBounds);
}

} // namespace bcad::index