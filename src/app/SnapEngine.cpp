#include "bcad/app/SnapEngine.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/SnapGeometry.h"
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

namespace bcad::app {

namespace {

struct Candidate {
    geom::Point2 point;
    SnapType type;
    double dist;
};

void addPointCandidates(const geom::Entity& e, std::vector<Candidate>& out, const geom::Point2& cursor) {
    auto add = [&](const geom::Point2& p, SnapType type) {
        out.push_back({ p, type, geom::distance(cursor, p) });
    };
    auto midpoint = [](const geom::Point2& a, const geom::Point2& b) {
        return geom::Point2((a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0);
    };

    switch (e.type()) {
        case geom::EntityType::Line: {
            const auto& l = static_cast<const geom::LineEntity&>(e);
            add(l.start(), SnapType::Endpoint);
            add(l.end(), SnapType::Endpoint);
            add(midpoint(l.start(), l.end()), SnapType::Midpoint);
            break;
        }
        case geom::EntityType::Circle: {
            const auto& c = static_cast<const geom::CircleEntity&>(e);
            add(c.center(), SnapType::Center);
            double cx = CGAL::to_double(c.center().x()), cy = CGAL::to_double(c.center().y());
            for (double a : { 0.0, std::numbers::pi / 2, std::numbers::pi, 3 * std::numbers::pi / 2 }) {
                add(geom::Point2(cx + c.radius() * std::cos(a), cy + c.radius() * std::sin(a)), SnapType::Quadrant);
            }
            break;
        }
        case geom::EntityType::Arc: {
            const auto& a = static_cast<const geom::ArcEntity&>(e);
            add(a.center(), SnapType::Center);
            add(a.startPoint(), SnapType::Endpoint);
            add(a.endPoint(), SnapType::Endpoint);
            double cx = CGAL::to_double(a.center().x()), cy = CGAL::to_double(a.center().y());
            for (double ang : { 0.0, std::numbers::pi / 2, std::numbers::pi, 3 * std::numbers::pi / 2 }) {
                double rel = geom::normalizeAngle(ang - a.startAngle());
                if (rel <= a.sweep()) {
                    add(geom::Point2(cx + a.radius() * std::cos(ang), cy + a.radius() * std::sin(ang)),
                        SnapType::Quadrant);
                }
            }
            break;
        }
        case geom::EntityType::Polyline: {
            const auto& p = static_cast<const geom::PolylineEntity&>(e);
            const auto& verts = p.vertices();
            std::size_t n = verts.size();
            for (std::size_t i = 0; i < n; ++i) add(verts[i], SnapType::Endpoint);
            std::size_t segCount = p.closed() ? n : (n == 0 ? 0 : n - 1);
            for (std::size_t i = 0; i < segCount; ++i) {
                add(midpoint(verts[i], verts[(i + 1) % n]), SnapType::Midpoint);
            }
            break;
        }
        case geom::EntityType::Point: {
            add(static_cast<const geom::PointEntity&>(e).position(), SnapType::Endpoint);
            break;
        }
    }
}

std::optional<Candidate> bestInTier(const std::vector<Candidate>& candidates, double tolerance,
                                     std::initializer_list<SnapType> tierTypes) {
    const Candidate* best = nullptr;
    for (const auto& c : candidates) {
        if (c.dist > tolerance) continue;
        for (SnapType t : tierTypes) {
            if (c.type == t && (!best || c.dist < best->dist)) best = &c;
        }
    }
    if (best) return *best;
    return std::nullopt;
}

} // namespace

SnapResult SnapEngine::findSnap(const core::Document& doc, const geom::Point2& cursor, double worldTolerance,
                                 std::optional<geom::Point2> referencePoint) const {
    geom::BoundingBox region = geom::BoundingBox::fromCenterHalfExtent(cursor, worldTolerance);
    std::vector<geom::Entity*> hits = doc.entitiesInRegion(region);

    std::vector<Candidate> candidates;
    for (geom::Entity* e : hits) addPointCandidates(*e, candidates, cursor);

    for (std::size_t i = 0; i < hits.size(); ++i) {
        for (std::size_t j = i + 1; j < hits.size(); ++j) {
            for (const geom::Point2& p : geom::entityIntersections(*hits[i], *hits[j])) {
                candidates.push_back({ p, SnapType::Intersection, geom::distance(cursor, p) });
            }
        }
    }

    if (auto c = bestInTier(candidates, worldTolerance,
                             { SnapType::Endpoint, SnapType::Center, SnapType::Intersection, SnapType::Quadrant })) {
        return { c->type, c->point };
    }
    if (auto c = bestInTier(candidates, worldTolerance, { SnapType::Midpoint })) {
        return { c->type, c->point };
    }

    // Perpendicular only makes sense relative to an active reference point
    // (the tool's last placed point), and it activates on proximity to the
    // *entity* rather than to the resulting foot point, which can sit far
    // from the cursor along that entity — matches how AutoCAD's PER snap
    // behaves: hover anywhere on the qualifying line/circle/arc.
    if (referencePoint) {
        geom::Entity* nearest = nullptr;
        double nearestDist = std::numeric_limits<double>::infinity();
        for (geom::Entity* e : hits) {
            double d = e->distanceTo(cursor);
            if (d <= worldTolerance && d < nearestDist) {
                nearestDist = d;
                nearest = e;
            }
        }
        if (nearest) {
            if (auto foot = geom::perpendicularFoot(*nearest, *referencePoint, cursor)) {
                return { SnapType::Perpendicular, *foot };
            }
        }
    }

    // Nearest: absolute last resort, "snap to whatever curve is closest".
    // The nearest point on an entity to a query point and the perpendicular
    // foot from that same point are the same construction (for a line,
    // both are the segment projection; for a circle/arc, both sit along
    // the center-to-point radius) — so this reuses perpendicularFoot with
    // the cursor standing in as its own reference point.
    {
        const geom::Entity* bestEntity = nullptr;
        geom::Point2 bestPoint(0, 0);
        double bestDist = worldTolerance;
        for (geom::Entity* e : hits) {
            if (auto foot = geom::perpendicularFoot(*e, cursor, cursor)) {
                double d = geom::distance(cursor, *foot);
                if (d < bestDist) {
                    bestDist = d;
                    bestPoint = *foot;
                    bestEntity = e;
                }
            }
        }
        if (bestEntity) return { SnapType::Nearest, bestPoint };
    }

    return {};
}

} // namespace bcad::app
