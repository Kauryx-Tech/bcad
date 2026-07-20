#pragma once

#include "bcad/geometry/Tolerance.h"
#include "bcad/geometry/Types.h"
#include <cmath>
#include <numbers>

namespace bcad::geom {

inline double distance(const Point2& a, const Point2& b) {
    return std::sqrt(CGAL::to_double(CGAL::squared_distance(a, b)));
}

inline double squaredDistance(const Point2& a, const Point2& b) {
    return CGAL::to_double(CGAL::squared_distance(a, b));
}

// Angle of vector (a->b) in radians, in [0, 2*pi).
inline double angleOf(const Point2& a, const Point2& b) {
    double ang = std::atan2(CGAL::to_double(b.y() - a.y()), CGAL::to_double(b.x() - a.x()));
    if (ang < 0) ang += 2.0 * std::numbers::pi;
    return ang;
}

// Unsigned angle between two vectors, in [0, pi].
inline double angleBetween(const Vector2& u, const Vector2& v) {
    double dot = CGAL::to_double(u * v);
    double lu = std::sqrt(CGAL::to_double(u.squared_length()));
    double lv = std::sqrt(CGAL::to_double(v.squared_length()));
    if (lu == 0.0 || lv == 0.0) return 0.0;
    double c = std::clamp(dot / (lu * lv), -1.0, 1.0);
    return std::acos(c);
}

inline double toDegrees(double radians) { return radians * 180.0 / std::numbers::pi; }
inline double toRadians(double degrees) { return degrees * std::numbers::pi / 180.0; }

inline double normalizeAngle(double radians) {
    double twoPi = 2.0 * std::numbers::pi;
    double a = std::fmod(radians, twoPi);
    if (a < 0) a += twoPi;
    return a;
}

// Perpendicular (signed) distance from point p to the infinite line through a-b.
inline double distancePointToLine(const Point2& p, const Point2& a, const Point2& b) {
    Vector2 ab = b - a;
    double len = std::sqrt(CGAL::to_double(ab.squared_length()));
    if (len < Tolerance::kDegenerateLength) return distance(p, a);
    Vector2 ap = p - a;
    double cross = CGAL::to_double(ab.x() * ap.y() - ab.y() * ap.x());
    return std::abs(cross) / len;
}

// Closest point on segment a-b to p, clamped to the segment.
inline Point2 closestPointOnSegment(const Point2& p, const Point2& a, const Point2& b) {
    Vector2 ab = b - a;
    double lenSq = CGAL::to_double(ab.squared_length());
    if (lenSq < Tolerance::kDegenerateLength) return a;
    Vector2 ap = p - a;
    double t = CGAL::to_double(ap * ab) / lenSq;
    t = std::clamp(t, 0.0, 1.0);
    return a + t * ab;
}

} // namespace bcad::geom
