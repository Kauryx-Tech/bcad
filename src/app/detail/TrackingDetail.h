// Repérage polaire (F10, POLARMODE d'AutoCAD) : calcul pur, partage entre
// l'accrochage du curseur et son dessin. Depuis un point de reference, le
// curseur proche d'une direction multiple de l'increment (90°, 45°, 30°…)
// est ramene sur cette direction.

#pragma once

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Point.h"

#include <cmath>
#include <numbers>
#include <optional>

namespace bcad::app::detail {

using geom::Point2;

struct PolarHit {
    Point2 point;     // curseur ramene sur la direction
    double distance;  // depuis la reference
    double angleDeg;  // direction, multiple de l'increment, dans [0, 360)
};

// `toleranceWorld` : ecart maximal (unites du dessin) entre le curseur et la
// direction pour qu'elle l'attire.
inline std::optional<PolarHit> polarTrack(const Point2& ref, const Point2& cursor, double incrementDeg,
                                          double toleranceWorld) {
    if (incrementDeg <= 0.0) return std::nullopt;
    const double dx = cursor.x_ - ref.x_, dy = cursor.y_ - ref.y_;
    const double length = std::hypot(dx, dy);
    if (length <= toleranceWorld) return std::nullopt;
    double deg = std::atan2(dy, dx) * 180.0 / std::numbers::pi;
    if (deg < 0.0) deg += 360.0;
    double snapped = std::round(deg / incrementDeg) * incrementDeg;
    if (snapped >= 360.0) snapped -= 360.0;
    const double rad = snapped * std::numbers::pi / 180.0;
    const double ux = std::cos(rad), uy = std::sin(rad);
    const double along = dx * ux + dy * uy;
    const double off = std::abs(-dx * uy + dy * ux);
    if (along <= 0.0 || off > toleranceWorld) return std::nullopt;
    return PolarHit{Point2(ref.x_ + ux * along, ref.y_ + uy * along), along, snapped};
}

// Reperage d'objet (F11, AUTOSNAP/OTRACK d'AutoCAD) : alignement du curseur
// sur l'horizontale ou la verticale d'un point acquis ; a l'intersection de
// deux alignements (un horizontal, un vertical), le croisement.
struct AlignHit {
    Point2 point;
    std::optional<Point2> horizontalFrom;  // point acquis dont on suit l'horizontale
    std::optional<Point2> verticalFrom;    // point acquis dont on suit la verticale
};

template <class Points>
std::optional<AlignHit> trackAlign(const Points& acquired, const Point2& cursor, double toleranceWorld) {
    std::optional<Point2> horizontal, vertical;
    double bestH = toleranceWorld, bestV = toleranceWorld;
    for (const Point2& p : acquired) {
        if (const double dy = std::abs(cursor.y_ - p.y_); dy <= bestH) {
            bestH = dy;
            horizontal = p;
        }
        if (const double dx = std::abs(cursor.x_ - p.x_); dx <= bestV) {
            bestV = dx;
            vertical = p;
        }
    }
    if (!horizontal && !vertical) return std::nullopt;
    AlignHit hit{cursor, horizontal, vertical};
    if (horizontal) hit.point.y_ = horizontal->y_;
    if (vertical) hit.point.x_ = vertical->x_;
    return hit;
}

} // namespace bcad::app::detail
