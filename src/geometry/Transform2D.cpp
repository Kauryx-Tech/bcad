#include "bcad/geometry/Transform2D.h"
#include "bcad/geometry/BoundingBox.h"

namespace bcad::geom {

BoundingBox Transform2D::transform(const BoundingBox& bbox) const {
    if (!bbox.isValid()) return bbox;
    Point2 corners[4] = {
        Point2(bbox.minX, bbox.minY),
        Point2(bbox.maxX, bbox.minY),
        Point2(bbox.maxX, bbox.maxY),
        Point2(bbox.minX, bbox.maxY)
    };
    BoundingBox result;
    for (const auto& c : corners) result.expand(transform(c));
    return result;
}

} // namespace bcad::geom