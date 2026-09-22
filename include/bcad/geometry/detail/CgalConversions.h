#pragma once

#include "bcad/geometry/Point.h"
#include "bcad/geometry/Transform2D.h"

// CGAL includes MUST be outside any namespace to avoid namespace pollution
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Aff_transformation_2.h>
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Polygon_2_algorithms.h>

namespace bcad::geom::detail {

using Kernel = ::CGAL::Exact_predicates_inexact_constructions_kernel;
using CgalPoint2 = Kernel::Point_2;
using CgalVector2 = Kernel::Vector_2;
using CgalAffTransform2 = ::CGAL::Aff_transformation_2<Kernel>;
using CgalPolygon2 = ::CGAL::Polygon_2<Kernel>;
using CgalPolygonWithHoles2 = ::CGAL::Polygon_with_holes_2<Kernel>;

inline CgalPoint2 toCgal(const Point2& p) {
    return CgalPoint2(p.x_, p.y_);
}

inline Point2 fromCgal(const CgalPoint2& p) {
    return Point2(::CGAL::to_double(p.x()), ::CGAL::to_double(p.y()));
}

inline CgalVector2 toCgal(const Vector2& v) {
    return CgalVector2(v.x_, v.y_);
}

inline Vector2 fromCgal(const CgalVector2& v) {
    return Vector2(::CGAL::to_double(v.x()), ::CGAL::to_double(v.y()));
}

inline CgalAffTransform2 toCgal(const Transform2D& t) {
    const auto& m = t.matrix();
    return CgalAffTransform2(m.m00, m.m01, m.m02, m.m10, m.m11, m.m12);
}

inline Transform2D fromCgal(const CgalAffTransform2& t) {
    return Transform2D(Transform2D::Matrix{
        ::CGAL::to_double(t.m(0,0)), ::CGAL::to_double(t.m(0,1)), ::CGAL::to_double(t.m(0,2)),
        ::CGAL::to_double(t.m(1,0)), ::CGAL::to_double(t.m(1,1)), ::CGAL::to_double(t.m(1,2))
    });
}

} // namespace bcad::geom::detail