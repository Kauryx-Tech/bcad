// Geometrie des variantes de Cercle ▾ et Arc ▾ (comme AutoCAD) : cercle par
// 2 points (diametre) ou 3 points, arc par 3 points. Pure, partagee entre
// l'execution et l'apercu sous le curseur.

#pragma once

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/Tolerance.h"

#include <cmath>
#include <numbers>
#include <optional>

namespace bcad::app::detail {

using geom::Point2;

struct CircleDef {
    Point2 center;
    double radius;
};

inline std::optional<CircleDef> circleFrom2P(const Point2& a, const Point2& b) {
    const double d = geom::distance(a, b);
    if (d < geom::Tolerance::kDegenerateLength) return std::nullopt;
    return CircleDef{Point2((a.x_ + b.x_) / 2.0, (a.y_ + b.y_) / 2.0), d / 2.0};
}

// Cercle circonscrit ; rien si les points sont alignes ou confondus.
inline std::optional<CircleDef> circleFrom3P(const Point2& a, const Point2& b, const Point2& c) {
    const double d = 2.0 * (a.x_ * (b.y_ - c.y_) + b.x_ * (c.y_ - a.y_) + c.x_ * (a.y_ - b.y_));
    const double scale = std::max({geom::distance(a, b), geom::distance(b, c), geom::distance(a, c), 1.0});
    if (std::abs(d) < 1e-12 * scale * scale) return std::nullopt;
    const double a2 = a.x_ * a.x_ + a.y_ * a.y_, b2 = b.x_ * b.x_ + b.y_ * b.y_, c2 = c.x_ * c.x_ + c.y_ * c.y_;
    const Point2 center((a2 * (b.y_ - c.y_) + b2 * (c.y_ - a.y_) + c2 * (a.y_ - b.y_)) / d,
                        (a2 * (c.x_ - b.x_) + b2 * (a.x_ - c.x_) + c2 * (b.x_ - a.x_)) / d);
    return CircleDef{center, geom::distance(center, a)};
}

// Arc de `start` a `end` passant par `mid` : angles de depart et de fin dans
// le sens trigonometrique (convention d'ArcEntity), inverses si l'arc
// passant par `mid` tourne dans l'autre sens.
struct ArcDef {
    Point2 center;
    double radius, startAngle, endAngle;
};

inline std::optional<ArcDef> arcFrom3P(const Point2& start, const Point2& mid, const Point2& end) {
    const auto circle = circleFrom3P(start, mid, end);
    if (!circle) return std::nullopt;
    const double a0 = geom::angleOf(circle->center, start);
    const double am = geom::angleOf(circle->center, mid);
    const double a1 = geom::angleOf(circle->center, end);
    const bool ccw = geom::normalizeAngle(am - a0) <= geom::normalizeAngle(a1 - a0);
    return ArcDef{circle->center, circle->radius, ccw ? a0 : a1, ccw ? a1 : a0};
}

} // namespace bcad::app::detail
