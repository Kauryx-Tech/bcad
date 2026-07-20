#pragma once

#include "bcad/geometry/Types.h"
#include <array>
#include <vector>

namespace bcad::geom {

struct Triangle {
    std::array<Point2, 3> points;
};

// Plain Delaunay triangulation of a point set (convex hull domain).
std::vector<Triangle> delaunayTriangulate(const std::vector<Point2>& points);

// Constrained Delaunay triangulation of a (possibly non-convex) polygon,
// used to fill closed polylines/polygons for rendering or export.
// `holes` are optional inner boundaries (e.g. from a boolean-difference result).
std::vector<Triangle> triangulatePolygon(const std::vector<Point2>& outerBoundary,
                                          const std::vector<std::vector<Point2>>& holes = {});

} // namespace bcad::geom
