#include "bcad/geometry/SnapGeometry.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Tolerance.h"
#include <algorithm>
#include <limits>

namespace bcad::geom {

namespace {

using Segment = std::pair<Point2, Point2>;

std::vector<Segment> segmentsOf(const Entity& e) {
    std::vector<Segment> segs;
    if (e.type() == EntityType::Line) {
        const auto& l = static_cast<const LineEntity&>(e);
        segs.emplace_back(l.start(), l.end());
    } else if (e.type() == EntityType::Polyline) {
        const auto& p = static_cast<const PolylineEntity&>(e);
        const auto& v = p.vertices();
        std::size_t n = v.size();
        std::size_t segCount = p.closed() ? n : (n == 0 ? 0 : n - 1);
        for (std::size_t i = 0; i < segCount; ++i) segs.emplace_back(v[i], v[(i + 1) % n]);
    }
    return segs;
}

bool isCircular(const Entity& e) { return e.type() == EntityType::Circle || e.type() == EntityType::Arc; }

Point2 centerOf(const Entity& e) {
    return e.type() == EntityType::Circle ? static_cast<const CircleEntity&>(e).center()
                                           : static_cast<const ArcEntity&>(e).center();
}

double radiusOf(const Entity& e) {
    return e.type() == EntityType::Circle ? static_cast<const CircleEntity&>(e).radius()
                                           : static_cast<const ArcEntity&>(e).radius();
}

// Vrai pour tout point d'un cercle complet ; pour un arc, uniquement dans son balayage.
bool onCircularEntity(const Entity& e, const Point2& p) {
    if (e.type() == EntityType::Circle) return true;
    const auto& a = static_cast<const ArcEntity&>(e);
    double rel = normalizeAngle(angleOf(a.center(), p) - a.startAngle());
    return rel <= a.sweep() + Tolerance::kAngular;
}

std::optional<Point2> segSegIntersection(const Point2& a1, const Point2& a2, const Point2& b1, const Point2& b2) {
    // Une résolution paramétrique manuelle (plutôt que l'API à variant de
    // CGAL::intersection) garde ce site d'appel simple : on ne veut jamais que
    // le cas d'un point unique, jamais le cas dégénéré de segments superposés.
    double dax = CGAL::to_double(a2.x() - a1.x()), day = CGAL::to_double(a2.y() - a1.y());
    double dbx = CGAL::to_double(b2.x() - b1.x()), dby = CGAL::to_double(b2.y() - b1.y());
    double denom = dax * dby - day * dbx;
    if (std::abs(denom) < Tolerance::kDegenerateLength) return std::nullopt; // parallèles/colinéaires

    double ex = CGAL::to_double(b1.x() - a1.x()), ey = CGAL::to_double(b1.y() - a1.y());
    double t = (ex * dby - ey * dbx) / denom;
    double u = (ex * day - ey * dax) / denom;
    constexpr double kEdge = 1e-9;
    if (t < -kEdge || t > 1.0 + kEdge || u < -kEdge || u > 1.0 + kEdge) return std::nullopt;

    return Point2(CGAL::to_double(a1.x()) + t * dax, CGAL::to_double(a1.y()) + t * day);
}

std::vector<Point2> segCircleIntersection(const Point2& a, const Point2& b, const Point2& center, double radius) {
    std::vector<Point2> out;
    double dx = CGAL::to_double(b.x() - a.x()), dy = CGAL::to_double(b.y() - a.y());
    double fx = CGAL::to_double(a.x() - center.x()), fy = CGAL::to_double(a.y() - center.y());

    double A = dx * dx + dy * dy;
    if (A < Tolerance::kDegenerateLength) return out;
    double B = 2.0 * (fx * dx + fy * dy);
    double C = fx * fx + fy * fy - radius * radius;

    double disc = B * B - 4.0 * A * C;
    if (disc < 0.0) return out;
    double sq = std::sqrt(disc);
    constexpr double kEdge = 1e-9;

    for (double t : { (-B - sq) / (2.0 * A), (-B + sq) / (2.0 * A) }) {
        if (t >= -kEdge && t <= 1.0 + kEdge) {
            double tc = std::clamp(t, 0.0, 1.0);
            out.emplace_back(CGAL::to_double(a.x()) + tc * dx, CGAL::to_double(a.y()) + tc * dy);
        }
    }
    if (out.size() == 2 && distance(out[0], out[1]) < Tolerance::kLinear) out.pop_back();
    return out;
}

std::vector<Point2> circleCircleIntersection(const Point2& c1, double r1, const Point2& c2, double r2) {
    std::vector<Point2> out;
    double dx = CGAL::to_double(c2.x() - c1.x()), dy = CGAL::to_double(c2.y() - c1.y());
    double d = std::sqrt(dx * dx + dy * dy);
    if (d < Tolerance::kDegenerateLength) return out; // concentriques : pas d'intersection bien définie
    if (d > r1 + r2 + Tolerance::kLinear || d < std::abs(r1 - r2) - Tolerance::kLinear) return out;

    double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    double hSq = std::max(0.0, r1 * r1 - a * a);
    double h = std::sqrt(hSq);
    double mx = CGAL::to_double(c1.x()) + a * dx / d;
    double my = CGAL::to_double(c1.y()) + a * dy / d;
    double rx = -dy * (h / d), ry = dx * (h / d);

    out.emplace_back(mx + rx, my + ry);
    if (h > Tolerance::kLinear) out.emplace_back(mx - rx, my - ry);
    return out;
}

} // namespace

std::vector<Point2> entityIntersections(const Entity& a, const Entity& b) {
    std::vector<Point2> results;
    bool aCirc = isCircular(a), bCirc = isCircular(b);

    if (!aCirc && !bCirc) {
        for (const auto& [a1, a2] : segmentsOf(a)) {
            for (const auto& [b1, b2] : segmentsOf(b)) {
                if (auto p = segSegIntersection(a1, a2, b1, b2)) results.push_back(*p);
            }
        }
    } else if (aCirc && bCirc) {
        for (const Point2& p : circleCircleIntersection(centerOf(a), radiusOf(a), centerOf(b), radiusOf(b))) {
            if (onCircularEntity(a, p) && onCircularEntity(b, p)) results.push_back(p);
        }
    } else {
        const Entity& circ = aCirc ? a : b;
        const auto& segs = aCirc ? segmentsOf(b) : segmentsOf(a);
        for (const auto& [s1, s2] : segs) {
            for (const Point2& p : segCircleIntersection(s1, s2, centerOf(circ), radiusOf(circ))) {
                if (onCircularEntity(circ, p)) results.push_back(p);
            }
        }
    }
    return results;
}

std::optional<Point2> perpendicularFoot(const Entity& e, const Point2& reference, const Point2& cursorHint) {
    if (isCircular(e)) {
        Point2 center = centerOf(e);
        double radius = radiusOf(e);
        Vector2 dir = reference - center;
        double len = std::sqrt(CGAL::to_double(dir.squared_length()));
        if (len < Tolerance::kDegenerateLength) return std::nullopt;
        Point2 foot(CGAL::to_double(center.x()) + radius * CGAL::to_double(dir.x()) / len,
                    CGAL::to_double(center.y()) + radius * CGAL::to_double(dir.y()) / len);
        if (!onCircularEntity(e, foot)) return std::nullopt;
        return foot;
    }

    auto segs = segmentsOf(e);
    if (segs.empty()) return std::nullopt;

    // Sélectionne le segment que le curseur survole réellement — pertinent
    // pour les polylignes, qui peuvent avoir plusieurs segments candidats à portée.
    const Segment* best = nullptr;
    double bestDist = std::numeric_limits<double>::infinity();
    for (const auto& seg : segs) {
        double d = distance(cursorHint, closestPointOnSegment(cursorHint, seg.first, seg.second));
        if (d < bestDist) {
            bestDist = d;
            best = &seg;
        }
    }
    if (!best) return std::nullopt;
    return closestPointOnSegment(reference, best->first, best->second);
}

} // namespace bcad::geom
