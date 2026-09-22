#pragma once

#include "bcad/geometry/Tolerance.h"
#include "bcad/geometry/Point.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace bcad::geom {

// Forward declarations for entity types (not the structs)
class LineEntity;
class CircleEntity;
class ArcEntity;
class PolylineEntity;
class Entity;

// ==================== Basic Geometry Functions ====================

// Angle du vecteur (a->b) en radians, dans [0, 2*pi).
inline double angleOf(const Point2& a, const Point2& b) {
    double ang = std::atan2(b.y_ - a.y_, b.x_ - a.x_);
    if (ang < 0) ang += 2.0 * std::numbers::pi;
    return ang;
}

// Angle non signé entre deux vecteurs, dans [0, pi].
inline double angleBetween(const Vector2& u, const Vector2& v) {
    double dot = u.x_ * v.x_ + u.y_ * v.y_;
    double lu = std::sqrt(u.x_ * u.x_ + u.y_ * u.y_);
    double lv = std::sqrt(v.x_ * v.x_ + v.y_ * v.y_);
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

// Distance perpendiculaire (signée) du point p à la droite infinie passant par a-b.
inline double distancePointToLine(const Point2& p, const Point2& a, const Point2& b) {
    Vector2 ab = {b.x_ - a.x_, b.y_ - a.y_};
    double len = std::sqrt(ab.x_ * ab.x_ + ab.y_ * ab.y_);
    if (len < Tolerance::kDegenerateLength) return distance(p, a);
    Vector2 ap = {p.x_ - a.x_, p.y_ - a.y_};
    double cross = ab.x_ * ap.y_ - ab.y_ * ap.x_;
    return std::abs(cross) / len;
}

// Point du segment a-b le plus proche de p, contraint à rester sur le segment.
inline Point2 closestPointOnSegment(const Point2& p, const Point2& a, const Point2& b) {
    Vector2 ab = {b.x_ - a.x_, b.y_ - a.y_};
    double lenSq = ab.x_ * ab.x_ + ab.y_ * ab.y_;
    if (lenSq < Tolerance::kDegenerateLength) return a;
    Vector2 ap = {p.x_ - a.x_, p.y_ - a.y_};
    double t = (ap.x_ * ab.x_ + ap.y_ * ab.y_) / lenSq;
    t = std::clamp(t, 0.0, 1.0);
    return Point2(a.x_ + t * ab.x_, a.y_ + t * ab.y_);
}

} // namespace bcad::geom