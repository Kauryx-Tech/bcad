#pragma once

#include "bcad/geometry/Point.h"
#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Transform2D.h"
#include <vector>

namespace bcad::geom {

using Point2 = geom::Point2;
using Vector2 = geom::Vector2;
using AffTransform2 = geom::Transform2D;

using Polygon2 = std::vector<Point2>;
struct PolygonWithHoles2Impl {
    Polygon2 outer;
    std::vector<Polygon2> holes;
};
using PolygonWithHoles2 = PolygonWithHoles2Impl;

} // namespace bcad::geom