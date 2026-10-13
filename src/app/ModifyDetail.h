// Geometrie partagee entre l'execution (ViewportModifyTools) et l'apercu
// sous curseur (ViewportOverlay) : decalage d'une entite, supports de
// chanfrein (lignes, arcs, polylignes) et test d'inclusion. Header-only :
// aucune unite de traduction ne grossit, et l'apercu montre exactement ce
// que l'outil va poser.

#pragma once

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

namespace bcad::app::detail {

using geom::Point2;

// Clone decale de e, a distance d du cote de side. Lignes (normale),
// cercles/arcs (rayon augmente si side est hors, reduit sinon), polylignes
// (sommets pousses le long des normales moyennes, jointure biseautee).
// nullptr si le type n'est pas decalable.
inline std::unique_ptr<geom::Entity> offsetClone(geom::Entity* e, double d,
                                                const Point2& side) {
    if (e->typeId() == geom::TypeId_Line) {
        const auto& l = static_cast<const geom::LineEntity&>(*e);
        double dx = l.end().x_ - l.start().x_, dy = l.end().y_ - l.start().y_;
        double len = std::hypot(dx, dy);
        if (len < geom::Tolerance::kDegenerateLength) return nullptr;
        double nx = -dy / len, ny = dx / len;
        double s = (side.x_ - l.start().x_) * nx + (side.y_ - l.start().y_) * ny;
        double sign = (s >= 0) ? 1.0 : -1.0;
        return std::make_unique<geom::LineEntity>(
            Point2(l.start().x_ + nx * sign * d, l.start().y_ + ny * sign * d),
            Point2(l.end().x_ + nx * sign * d, l.end().y_ + ny * sign * d));
    }
    if (e->typeId() == geom::TypeId_Circle || e->typeId() == geom::TypeId_Arc) {
        Point2 c = (e->typeId() == geom::TypeId_Circle)
                       ? static_cast<const geom::CircleEntity&>(*e).center()
                       : static_cast<const geom::ArcEntity&>(*e).center();
        double r = (e->typeId() == geom::TypeId_Circle)
                       ? static_cast<const geom::CircleEntity&>(*e).radius()
                       : static_cast<const geom::ArcEntity&>(*e).radius();
        double r2 = (geom::distance(c, side) >= r) ? r + d : r - d;
        if (r2 < geom::Tolerance::kDegenerateLength) return nullptr;
        if (e->typeId() == geom::TypeId_Circle) return std::make_unique<geom::CircleEntity>(c, r2);
        const auto& a = static_cast<const geom::ArcEntity&>(*e);
        return std::make_unique<geom::ArcEntity>(c, r2, a.startAngle(), a.endAngle());
    }
    if (e->typeId() == geom::TypeId_Polyline) {
        const auto& p = static_cast<const geom::PolylineEntity&>(*e);
        const auto& v = p.vertices();
        if (v.size() < 2) return nullptr;
        double dx0 = v[1].x_ - v[0].x_, dy0 = v[1].y_ - v[0].y_;
        double l0 = std::hypot(dx0, dy0);
        if (l0 < geom::Tolerance::kDegenerateLength) return nullptr;
        double s = (side.x_ - v[0].x_) * (-dy0 / l0) + (side.y_ - v[0].y_) * (dx0 / l0);
        double sign = (s >= 0) ? 1.0 : -1.0;
        std::vector<Point2> nv;
        nv.reserve(v.size());
        std::size_t n = v.size();
        for (std::size_t i = 0; i < n; ++i) {
            double nx = 0.0, ny = 0.0;
            auto addEdge = [&](std::size_t a, std::size_t b) {
                double ex = v[b].x_ - v[a].x_, ey = v[b].y_ - v[a].y_;
                double el = std::hypot(ex, ey);
                if (el < geom::Tolerance::kDegenerateLength) return;
                nx += -ey / el;
                ny += ex / el;
            };
            if (i > 0) addEdge(i - 1, i);
            if (i + 1 < n) addEdge(i, i + 1);
            if (p.closed() && (i == 0 || i + 1 == n) && n > 2) addEdge(n - 1, 0);
            double nl = std::hypot(nx, ny);
            if (nl < geom::Tolerance::kDegenerateLength) {
                nv.push_back(v[i]);
            } else {
                nv.emplace_back(v[i].x_ + nx / nl * sign * d, v[i].y_ + ny / nl * sign * d);
            }
        }
        return std::make_unique<geom::PolylineEntity>(nv, p.closed());
    }
    return nullptr;
}

// Support de chanfrein : segment (ligne, ou segment de polyligne le plus
// proche du point designe) ou cercle (arc).
struct ChamferSupport {
    geom::Entity* entity = nullptr;
    bool circle = false;
    bool poly = false;
    Point2 s, t; // segment si !circle
    std::size_t polySeg = 0; // segment retenu si poly
    Point2 c; // centre si circle
    double r = 0.0; // rayon si circle
};

inline ChamferSupport chamferSupport(geom::Entity* e, const Point2& pick) {
    ChamferSupport sup;
    sup.entity = e;
    if (e->typeId() == geom::TypeId_Arc) {
        const auto& a = static_cast<const geom::ArcEntity&>(*e);
        sup.circle = true;
        sup.c = a.center();
        sup.r = a.radius();
        return sup;
    }
    if (e->typeId() == geom::TypeId_Line) {
        const auto& l = static_cast<const geom::LineEntity&>(*e);
        sup.s = l.start();
        sup.t = l.end();
        return sup;
    }
    const auto& p = static_cast<const geom::PolylineEntity&>(*e);
    const auto& v = p.vertices();
    std::size_t n = v.size();
    std::size_t segs = p.closed() ? n : (n > 0 ? n - 1 : 0);
    std::size_t best = 0;
    double bd = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < segs; ++i) {
        Point2 q = geom::closestPointOnSegment(pick, v[i], v[(i + 1) % n]);
        double d = geom::distance(pick, q);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    sup.poly = true;
    sup.polySeg = best;
    sup.s = v[best];
    sup.t = v[(best + 1) % n];
    return sup;
}

// Projection d'un point designe sur un support.
inline Point2 projectOnSupport(const ChamferSupport& sup, const Point2& pick) {
    if (sup.circle) {
        double ang = geom::angleOf(sup.c, pick);
        return Point2(sup.c.x_ + sup.r * std::cos(ang), sup.c.y_ + sup.r * std::sin(ang));
    }
    return geom::closestPointOnSegment(pick, sup.s, sup.t);
}

// Intersections des supports (droites infinies et cercles).
inline std::vector<Point2> supportIntersections(const ChamferSupport& a,
                                               const ChamferSupport& b) {
    std::vector<Point2> out;
    if (!a.circle && !b.circle) {
        double dx1 = a.t.x_ - a.s.x_, dy1 = a.t.y_ - a.s.y_;
        double dx2 = b.t.x_ - b.s.x_, dy2 = b.t.y_ - b.s.y_;
        double denom = dx1 * dy2 - dy1 * dx2;
        if (std::abs(denom) < geom::Tolerance::kDegenerateLength) return out;
        double t = ((b.s.x_ - a.s.x_) * dy2 - (b.s.y_ - a.s.y_) * dx2) / denom;
        out.emplace_back(a.s.x_ + t * dx1, a.s.y_ + t * dy1);
        return out;
    }
    if (a.circle != b.circle) {
        const ChamferSupport& ln = a.circle ? b : a;
        const ChamferSupport& ci = a.circle ? a : b;
        double dx = ln.t.x_ - ln.s.x_, dy = ln.t.y_ - ln.s.y_;
        double len = std::hypot(dx, dy);
        if (len < geom::Tolerance::kDegenerateLength) return out;
        double ux = dx / len, uy = dy / len;
        double fx = ln.s.x_ - ci.c.x_, fy = ln.s.y_ - ci.c.y_;
        double bb = 2.0 * (fx * ux + fy * uy);
        double cc = fx * fx + fy * fy - ci.r * ci.r;
        double disc = bb * bb - 4.0 * cc;
        if (disc < 0.0) return out;
        double sq = std::sqrt(disc);
        for (double sgn : {-1.0, 1.0}) {
            double tt = (-bb + sgn * sq) / 2.0;
            out.emplace_back(ln.s.x_ + ux * tt, ln.s.y_ + uy * tt);
        }
        return out;
    }
    double dd = geom::distance(a.c, b.c);
    if (dd < geom::Tolerance::kDegenerateLength || dd > a.r + b.r + geom::Tolerance::kDegenerateLength ||
        dd + std::min(a.r, b.r) < std::max(a.r, b.r) - geom::Tolerance::kDegenerateLength) {
        return out;
    }
    double ka = (a.r * a.r - b.r * b.r + dd * dd) / (2.0 * dd);
    double h = std::sqrt(std::max(0.0, a.r * a.r - ka * ka));
    double xm = a.c.x_ + ka * (b.c.x_ - a.c.x_) / dd;
    double ym = a.c.y_ + ka * (b.c.y_ - a.c.y_) / dd;
    double rx = -(b.c.y_ - a.c.y_) * (h / dd);
    double ry = (b.c.x_ - a.c.x_) * (h / dd);
    out.emplace_back(xm + rx, ym + ry);
    out.emplace_back(xm - rx, ym - ry);
    return out;
}

// Point de raccord a distance d de I sur le support, du cote du point q.
inline Point2 trimOnSupport(const ChamferSupport& sup, const Point2& i, const Point2& q, double d) {
    if (!sup.circle) {
        double vx = q.x_ - i.x_, vy = q.y_ - i.y_;
        double vl = std::hypot(vx, vy);
        if (vl < geom::Tolerance::kDegenerateLength) return i;
        return Point2(i.x_ + vx / vl * d, i.y_ + vy / vl * d);
    }
    double aI = geom::angleOf(sup.c, i);
    double aq = geom::angleOf(sup.c, q);
    double dir = (geom::normalizeAngle(aq - aI) <= std::numbers::pi) ? 1.0 : -1.0;
    double aT = aI + dir * (d / sup.r);
    return Point2(sup.c.x_ + sup.r * std::cos(aT), sup.c.y_ + sup.r * std::sin(aT));
}

// Entite raccourcie au point t : ligne (extremite la plus proche remplacee),
// arc (angle de l'extremite la plus proche recale), polyligne (sommet du
// segment retenu le plus proche de t deplace).
inline std::unique_ptr<geom::Entity> shortenToPoint(geom::Entity* e, const ChamferSupport& sup,
                                                   const Point2& t) {
    if (e->typeId() == geom::TypeId_Line) {
        const auto& l = static_cast<const geom::LineEntity&>(*e);
        bool nearStart = geom::distance(l.start(), t) < geom::distance(l.end(), t);
        return std::make_unique<geom::LineEntity>(nearStart ? t : l.start(),
                                                 nearStart ? l.end() : t);
    }
    if (e->typeId() == geom::TypeId_Arc) {
        auto out = e->clone();
        auto& a = static_cast<geom::ArcEntity&>(*out);
        double angT = geom::angleOf(a.center(), t);
        if (geom::distance(a.startPoint(), t) < geom::distance(a.endPoint(), t)) {
            a.setStartAngle(angT);
        } else {
            a.setEndAngle(angT);
        }
        return out;
    }
    auto out = e->clone();
    auto& p = static_cast<geom::PolylineEntity&>(*out);
    const auto& v = p.vertices();
    std::size_t n = v.size();
    std::size_t i0 = sup.polySeg % n;
    std::size_t i1 = (sup.polySeg + 1) % n;
    std::size_t idx = (geom::distance(v[i0], t) < geom::distance(v[i1], t)) ? i0 : i1;
    p.vertices()[idx] = t;
    return out;
}

// Inclusion (regle pair-impair) pour la designation de contour au point.
inline bool pointInPolygon(const Point2& p, const std::vector<Point2>& v, bool closed) {
    if (!closed || v.size() < 3) return false;
    bool inside = false;
    std::size_t n = v.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point2& a = v[i];
        const Point2& b = v[j];
        if ((a.y_ > p.y_) != (b.y_ > p.y_)) {
            double x = a.x_ + (p.y_ - a.y_) * (b.x_ - a.x_) / (b.y_ - a.y_);
            if (p.x_ < x) inside = !inside;
        }
    }
    return inside;
}

} // namespace bcad::app::detail
