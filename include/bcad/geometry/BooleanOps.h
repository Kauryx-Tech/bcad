#pragma once

#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Types.h"
#include <vector>

namespace bcad::geom {

enum class BooleanOp { Union, Intersection, Difference, SymmetricDifference };

// Polygon boolean operations backed by CGAL::Boolean_set_operations_2.
// Inputs must be closed, simple (non-self-intersecting) polylines; the
// result may contain holes and/or multiple disjoint pieces, so it is
// returned as one PolylineEntity per outer boundary (holes are flattened
// into that boundary's vertex list is NOT done — callers that need holes
// should use booleanOpWithHoles instead).
std::vector<PolygonWithHoles2> booleanOpWithHoles(const PolylineEntity& a,
                                                   const PolylineEntity& b,
                                                   BooleanOp op);

// Convenience wrapper: outer boundaries only, as ready-to-draw polylines.
std::vector<PolylineEntity> booleanOp(const PolylineEntity& a,
                                       const PolylineEntity& b,
                                       BooleanOp op);

double polygonArea(const PolylineEntity& polyline);

bool isSimplePolygon(const PolylineEntity& polyline);

} // namespace bcad::geom
