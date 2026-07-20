#pragma once

#include "bcad/geometry/Entity.h"
#include <optional>
#include <vector>

namespace bcad::geom {

// All true-geometry intersection points between two entities (computed from
// their exact representation, not their tessellation). Supports any
// combination of Line/Circle/Arc/Polyline; unsupported pairs (e.g. anything
// involving PointEntity) return an empty vector rather than erroring, since
// "no intersections" is a legitimate answer for a snap query.
std::vector<Point2> entityIntersections(const Entity& a, const Entity& b);

// Foot of the perpendicular from `reference` onto entity `e`. `cursorHint`
// disambiguates which segment/branch of a multi-part entity (a polyline) the
// user is actually pointing at. Returns nullopt if the entity type has no
// well-defined perpendicular, or the foot would land outside an arc's sweep.
std::optional<Point2> perpendicularFoot(const Entity& e, const Point2& reference, const Point2& cursorHint);

} // namespace bcad::geom
