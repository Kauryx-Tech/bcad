// Calculs des outils de mesure (I-01) : purs, sans Qt, partages entre
// l'execution et l'apercu sous le curseur. Rien n'est cree dans le dessin.

#pragma once

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Point.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace bcad::app::detail {

using geom::Point2;

// Distance entre deux points, ses composantes et l'angle du segment par
// rapport a l'axe X (degres, sens trigonometrique, dans [0, 360)).
struct DistanceMeasure {
    double distance;
    double dx, dy;
    double angleDeg;
};

inline DistanceMeasure measureDistance(const Point2& a, const Point2& b) {
    DistanceMeasure m;
    m.dx = b.x_ - a.x_;
    m.dy = b.y_ - a.y_;
    m.distance = std::hypot(m.dx, m.dy);
    double deg = std::atan2(m.dy, m.dx) * 180.0 / std::numbers::pi;
    if (deg < 0.0) deg += 360.0;
    if (deg >= 360.0) deg -= 360.0;
    m.angleDeg = m.distance > 0.0 ? deg : 0.0;
    return m;
}

// Angle au sommet v entre les cotes v->a et v->b, dans [0, 180] degres.
// Nul si un cote est degenere.
inline double measureAngleDeg(const Point2& v, const Point2& a, const Point2& b) {
    double ux = a.x_ - v.x_, uy = a.y_ - v.y_;
    double wx = b.x_ - v.x_, wy = b.y_ - v.y_;
    if (std::hypot(ux, uy) == 0.0 || std::hypot(wx, wy) == 0.0) return 0.0;
    // atan2 du produit vectoriel et du produit scalaire : stable pres de 0
    // et de 180, contrairement a acos.
    double ang = std::atan2(std::abs(ux * wy - uy * wx), ux * wx + uy * wy);
    return ang * 180.0 / std::numbers::pi;
}

// Degres -> grades (gon), l'unite d'angle des geometres francophones.
inline double degToGon(double deg) { return deg * 400.0 / 360.0; }

// Surface (formule du lacet, valeur absolue) et perimetre d'un anneau ferme.
// Le dernier sommet peut repeter le premier : le cote nul n'ajoute rien.
inline double ringArea(const std::vector<Point2>& ring) {
    const std::size_t n = ring.size();
    if (n < 3) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const Point2& a = ring[i];
        const Point2& b = ring[(i + 1) % n];
        sum += a.x_ * b.y_ - b.x_ * a.y_;
    }
    return std::abs(sum) / 2.0;
}

inline double ringPerimeter(const std::vector<Point2>& ring) {
    const std::size_t n = ring.size();
    if (n < 2) return 0.0;
    double len = 0.0;
    for (std::size_t i = 0; i < n; ++i) len += geom::distance(ring[i], ring[(i + 1) % n]);
    return len;
}

} // namespace bcad::app::detail
