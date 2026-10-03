#include "bcad/geometry/GeometryUtils2.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace bcad::geom {

// Helper: convert entity to primitive for intersection
struct Line2;
struct Circle2;
struct Segment2;
struct Arc2;
struct Polyline2;

namespace detail {

Line2 toLine2(const LineEntity& e) {
    return Line2{e.start(), e.end()};
}

Circle2 toCircle2(const CircleEntity& e) {
    return Circle2{e.center(), e.radius()};
}

Circle2 toCircle2(const ArcEntity& e) {
    return Circle2{e.center(), e.radius()};
}

Arc2 toArc2(const ArcEntity& e) {
    return Arc2{e.center(), e.radius(), e.startAngle(), e.endAngle()};
}

Polyline2 toPolyline2(const PolylineEntity& e) {
    Polyline2 p;
    p.vertices = e.vertices();
    p.closed = e.closed();
    return p;
}

} // namespace detail

// ==================== Trim Operations ====================

bool trimLine(LineEntity& line, const Entity& cuttingEntity, const Point2& pickPoint) {
    auto pts = entityIntersections(line, cuttingEntity);
    if (pts.empty()) return false;
    
    double tPick = 0.0;
    double lenSq = (line.end().x_ - line.start().x_) * (line.end().x_ - line.start().x_) + 
                   (line.end().y_ - line.start().y_) * (line.end().y_ - line.start().y_);
    if (lenSq > Tolerance::kDegenerateLength) {
        double dx = line.end().x_ - line.start().x_;
        double dy = line.end().y_ - line.start().y_;
        tPick = (pickPoint.x_ - line.start().x_) * dx + (pickPoint.y_ - line.start().y_) * dy;
        tPick = tPick / lenSq;
        tPick = std::clamp(tPick, 0.0, 1.0);
    }
    
    double bestT = -1;
    Point2 bestPt;
    bool found = false;
    
    for (const auto& pt : pts) {
        double dx = line.end().x_ - line.start().x_;
        double dy = line.end().y_ - line.start().y_;
        double t = (dx * dx + dy * dy) > Tolerance::kDegenerateLength ? 
            ((pt.x_ - line.start().x_) * dx + (pt.y_ - line.start().y_) * dy) / 
            (dx * dx + dy * dy) : -1;
        
        if (t < -Tolerance::kLinear || t > 1.0 + Tolerance::kLinear) continue;
        
        if (!found || std::abs(t - tPick) < std::abs(bestT - tPick)) {
            bestT = t;
            bestPt = pt;
            found = true;
        }
    }
    
    if (!found) return false;
    
    if (bestT < 0.5) {
        line.setStart(bestPt);
    } else {
        line.setEnd(bestPt);
    }
    return true;
}

bool trimPolyline(PolylineEntity& polyline, const Entity& cuttingEntity, const Point2& pickPoint) {
    // Find intersection points between polyline and cutting entity
    auto pts = entityIntersections(polyline, cuttingEntity);
    if (pts.empty()) return false;

    const auto& verts = polyline.vertices();
    if (verts.size() < 2) return false;

    bool closed = polyline.closed();
    size_t n = verts.size();
    size_t segCount = closed ? n : n - 1;

    // Find the segment and intersection closest to pickPoint
    double bestDistSq = std::numeric_limits<double>::infinity();
    size_t bestSeg = 0;
    Point2 bestPt;

    for (size_t i = 0; i < segCount; ++i) {
        const Point2& a = verts[i];
        const Point2& b = verts[(i + 1) % n];
        Segment2 seg{a, b};

        for (const auto& pt : pts) {
            // Check if intersection is on this segment
            double minX = std::min(a.x_, b.x_) - Tolerance::kLinear;
            double maxX = std::max(a.x_, b.x_) + Tolerance::kLinear;
            double minY = std::min(a.y_, b.y_) - Tolerance::kLinear;
            double maxY = std::max(a.y_, b.y_) + Tolerance::kLinear;
            if (pt.x_ < minX || pt.x_ > maxX || pt.y_ < minY || pt.y_ > maxY) continue;

            double dx = pickPoint.x_ - pt.x_;
            double dy = pickPoint.y_ - pt.y_;
            double distSq = dx * dx + dy * dy;
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestSeg = i;
                bestPt = pt;
            }
        }
    }

    if (bestDistSq == std::numeric_limits<double>::infinity()) return false;

    // Insert the intersection point as a new vertex
    // Split the segment at the intersection
    std::vector<Point2> newVerts = verts;
    newVerts.insert(newVerts.begin() + bestSeg + 1, bestPt);

    // Now we need to decide which part to keep based on pickPoint
    // Find which side of the new vertex the pickPoint is closer to
    // For simplicity in v1, we just keep the part containing the pickPoint
    // by checking if pickPoint is closer to start or end of the original polyline

    double distToStart = (pickPoint.x_ - verts[0].x_) * (pickPoint.x_ - verts[0].x_) +
                         (pickPoint.y_ - verts[0].y_) * (pickPoint.y_ - verts[0].y_);
    double distToEnd = (pickPoint.x_ - verts.back().x_) * (pickPoint.x_ - verts.back().x_) +
                       (pickPoint.y_ - verts.back().y_) * (pickPoint.y_ - verts.back().y_);

    if (distToStart < distToEnd) {
        // Keep from start to intersection
        newVerts.resize(bestSeg + 2);
    } else {
        // Keep from intersection to end
        newVerts.erase(newVerts.begin(), newVerts.begin() + bestSeg + 1);
    }

    polyline = PolylineEntity(std::move(newVerts), closed);
    return true;
}

bool extendArc(ArcEntity& arc, const Entity& boundaryEntity, const Point2& pickPoint) {
    // Find intersection of the full circle (arc extended to 360°) with the boundary.
    CircleEntity fullCircle(arc.center(), arc.radius());
    auto pts = entityIntersections(fullCircle, boundaryEntity);
    if (pts.empty()) return false;

    const double cx = arc.center().x_, cy = arc.center().y_;
    // The pick point indicates which end (start or end) the user wants to extend.
    const double pickRel = normalizeAngle(
        std::atan2(pickPoint.y_ - cy, pickPoint.x_ - cx) - arc.startAngle());
    const double sw = arc.sweep();
    // "Near start" if pick is in the first half, "near end" if in the second half.
    const bool nearStart = (pickRel < sw * 0.5);

    double bestRel = -1.0;
    bool found = false;
    for (const auto& p : pts) {
        double rel = normalizeAngle(std::atan2(p.y_ - cy, p.x_ - cx) - arc.startAngle());
        // Candidate must be outside the current arc span.
        if (rel >= -Tolerance::kAngular && rel <= sw + Tolerance::kAngular) continue;
        if (nearStart) {
            // We want the point just before the start (rel near 2π, i.e. large).
            double distToStart = normalizeAngle(-rel); // distance CCW from candidate to start
            if (!found || distToStart < normalizeAngle(-bestRel)) {
                bestRel = rel;
                found = true;
            }
        } else {
            // We want the point just after the end (rel > sw).
            if (!found || rel < bestRel) {
                bestRel = rel;
                found = true;
            }
        }
    }
    if (!found) return false;

    const double bestAngle = arc.startAngle() + bestRel;
    if (nearStart) {
        arc.setStartAngle(bestAngle);
    } else {
        arc.setEndAngle(bestAngle);
    }
    return true;
}

bool trimArc(ArcEntity& arc, const Entity& cuttingEntity, const Point2& pickPoint) {
    auto pts = entityIntersections(arc, cuttingEntity);
    if (pts.empty()) return false;

    // Angle du point de clic par rapport au centre de l'arc (normalisé dans
    // l'intervalle [startAngle, startAngle + sweep]).
    const double cx = arc.center().x_, cy = arc.center().y_;
    const double pickRel = normalizeAngle(
        std::atan2(pickPoint.y_ - cy, pickPoint.x_ - cx) - arc.startAngle());
    const double sw = arc.sweep();

    // Choisir l'intersection dont l'angle relatif est le plus proche de pickRel.
    double bestRel = -1.0;
    bool found = false;
    for (const auto& p : pts) {
        double rel = normalizeAngle(std::atan2(p.y_ - cy, p.x_ - cx) - arc.startAngle());
        if (rel > sw + Tolerance::kAngular) continue; // hors de l'arc
        if (!found || std::abs(rel - pickRel) < std::abs(bestRel - pickRel)) {
            bestRel = rel;
            found = true;
        }
    }
    if (!found) return false;

    // Retirer le bout que le clic désigne : si pickRel < bestRel, raccourcir
    // le début de l'arc (avancer startAngle) ; sinon raccourcir la fin.
    if (pickRel < bestRel) {
        arc.setStartAngle(arc.startAngle() + bestRel);
    } else {
        arc.setEndAngle(arc.startAngle() + bestRel);
    }
    return true;
}

// ==================== Extend Operations ====================

// Helper: intersection of two infinite lines (not segments)
inline std::optional<Point2> intersectInfiniteLines(const Line2& a, const Line2& b) {
    Point2 r = {a.b.x_ - a.a.x_, a.b.y_ - a.a.y_};
    Point2 s = {b.b.x_ - b.a.x_, b.b.y_ - b.a.y_};
    
    double rxs = r.x_ * s.y_ - r.y_ * s.x_;
    if (std::abs(rxs) < Tolerance::kDegenerateLength) 
        return std::nullopt; // Parallel or collinear
    
    Point2 qp = {b.a.x_ - a.a.x_, b.a.y_ - a.a.y_};
    double t = (qp.x_ * s.y_ - qp.y_ * s.x_) / rxs;
    // For infinite lines, t can be any value
    
    return Point2{ a.a.x_ + t * r.x_, a.a.y_ + t * r.y_ };
}

bool extendLine(LineEntity& line, const Entity& boundaryEntity, const Point2& pickPoint) {
    // Find intersection of the line's infinite line with boundaryEntity
    Line2 infiniteLine{line.start(), line.end()};

    std::vector<Point2> intersections;
    if (auto* b = dynamic_cast<const LineEntity*>(&boundaryEntity); b) {
        if (auto pt = intersectInfiniteLines(infiniteLine, detail::toLine2(*b))) {
            intersections.push_back(*pt);
        }
    } else if (auto* c = dynamic_cast<const CircleEntity*>(&boundaryEntity); c) {
        intersections = intersect(infiniteLine, detail::toCircle2(*c));
    } else if (auto* arc = dynamic_cast<const ArcEntity*>(&boundaryEntity); arc) {
        auto pts = intersect(infiniteLine, detail::toCircle2(*arc));
        for (const auto& p : pts) {
            double ang = std::atan2(p.y_ - arc->center().y_, p.x_ - arc->center().x_);
            double rel = std::fmod(ang - arc->startAngle(), 2.0 * std::numbers::pi);
            if (rel < 0) rel += 2.0 * std::numbers::pi;
            if (rel <= arc->sweep() + Tolerance::kAngular) {
                intersections.push_back(p);
            }
        }
    } else if (auto* p = dynamic_cast<const PolylineEntity*>(&boundaryEntity); p) {
        intersections = intersect(infiniteLine, detail::toPolyline2(*p));
    } else {
        return false;
    }

    if (intersections.empty()) return false;

    // Choose the intersection on the side of pickPoint
    double dx = line.end().x_ - line.start().x_;
    double dy = line.end().y_ - line.start().y_;
    double lenSq = dx * dx + dy * dy;
    if (lenSq < Tolerance::kDegenerateLength) return false;

    double pickT = ((pickPoint.x_ - line.start().x_) * dx + (pickPoint.y_ - line.start().y_) * dy) / lenSq;

    Point2 bestPt = intersections[0];
    double bestT = std::numeric_limits<double>::infinity();

    for (const auto& pt : intersections) {
        double t = ((pt.x_ - line.start().x_) * dx + (pt.y_ - line.start().y_) * dy) / lenSq;
        // We want the intersection beyond the line (t < 0 or t > 1)
        // and on the same side as pickPoint
        if ((pickT <= 0.5 && t < 0) || (pickT > 0.5 && t > 1)) {
            if (std::abs(t - pickT) < std::abs(bestT - pickT)) {
                bestT = t;
                bestPt = pt;
            }
        }
    }

    if (bestT == std::numeric_limits<double>::infinity()) return false;

    if (pickT <= 0.5) {
        line.setStart(bestPt);
    } else {
        line.setEnd(bestPt);
    }
    return true;
}

// ==================== Break Operations ====================

void breakPolyline(PolylineEntity& polyline, const Point2& breakPoint) {
    const auto& verts = polyline.vertices();
    if (verts.size() < 2) return;

    bool closed = polyline.closed();
    size_t n = verts.size();
    size_t segCount = closed ? n : n - 1;

    // Find the segment containing breakPoint
    for (size_t i = 0; i < segCount; ++i) {
        const Point2& a = verts[i];
        const Point2& b = verts[(i + 1) % n];
        if (distancePointToLine(breakPoint, a, b) <= Tolerance::kDegenerateLength) {
            // Insert breakPoint as new vertex
            std::vector<Point2> newVerts = verts;
            newVerts.insert(newVerts.begin() + i + 1, breakPoint);
            polyline = PolylineEntity(std::move(newVerts), closed);
            return;
        }
    }
}

// ==================== Offset Operations ====================

std::unique_ptr<PolylineEntity> offsetPolyline(const PolylineEntity& poly, double distance, const Point2& sidePoint) {
    const auto& verts = poly.vertices();
    if (verts.size() < 2) {
        return std::make_unique<PolylineEntity>(std::vector<Point2>(), false);
    }

    bool closed = poly.closed();
    size_t n = verts.size();
    size_t segCount = closed ? n : n - 1;

    // For each vertex, compute the offset along the angle bisector
    std::vector<Point2> offsetVerts;
    offsetVerts.reserve(n);

    // First, compute offset for each segment
    std::vector<Point2> segOffsetA(segCount), segOffsetB(segCount);
    for (size_t i = 0; i < segCount; ++i) {
        const Point2& a = verts[i];
        const Point2& b = verts[(i + 1) % n];
        Point2 dir{b.x_ - a.x_, b.y_ - a.y_};
        double len = std::sqrt(dir.x_ * dir.x_ + dir.y_ * dir.y_);
        if (len < Tolerance::kDegenerateLength) {
            segOffsetA[i] = a;
            segOffsetB[i] = b;
            continue;
        }
        Point2 normal(-dir.y_ / len, dir.x_ / len);

        // Determine which side
        double cross = dir.x_ * (sidePoint.y_ - a.y_) - dir.y_ * (sidePoint.x_ - a.x_);
        double sign = (cross > 0) ? 1.0 : -1.0;

        segOffsetA[i] = {a.x_ + normal.x_ * distance * sign, a.y_ + normal.y_ * distance * sign};
        segOffsetB[i] = {b.x_ + normal.x_ * distance * sign, b.y_ + normal.y_ * distance * sign};
    }

    // Now reconstruct vertices by intersecting adjacent offset segments
    if (closed) {
        for (size_t i = 0; i < n; ++i) {
            size_t prev = (i + n - 1) % n;
            size_t curr = i;

            // Intersect offset segments [prev] and [curr]
            Segment2 s1{segOffsetB[prev], segOffsetA[prev]};
            Segment2 s2{segOffsetA[curr], segOffsetB[curr]};

            if (auto pt = intersect(s1, s2)) {
                offsetVerts.push_back(*pt);
            } else {
                // Parallel - use midpoint
                offsetVerts.push_back({(segOffsetB[prev].x_ + segOffsetA[curr].x_) / 2.0,
                                       (segOffsetB[prev].y_ + segOffsetA[curr].y_) / 2.0});
            }
        }
    } else {
        // Open polyline - first vertex
        offsetVerts.push_back(segOffsetA[0]);

        // Middle vertices - intersect adjacent offset segments
        for (size_t i = 1; i < n - 1; ++i) {
            Segment2 s1{segOffsetB[i - 1], segOffsetA[i - 1]};
            Segment2 s2{segOffsetA[i], segOffsetB[i]};

            if (auto pt = intersect(s1, s2)) {
                offsetVerts.push_back(*pt);
            } else {
                offsetVerts.push_back({(segOffsetB[i - 1].x_ + segOffsetA[i].x_) / 2.0,
                                       (segOffsetB[i - 1].y_ + segOffsetA[i].y_) / 2.0});
            }
        }

        // Last vertex
        offsetVerts.push_back(segOffsetB[n - 2]);
    }

    return std::make_unique<PolylineEntity>(std::move(offsetVerts), closed);
}

// ==================== Break Operations ====================

std::pair<std::unique_ptr<LineEntity>, std::unique_ptr<LineEntity>>
breakLine(const LineEntity& line, const Point2& breakPoint) {
    if (distancePointToLine(breakPoint, line.start(), line.end()) > Tolerance::kDegenerateLength) {
        return {nullptr, nullptr};
    }

    auto l1 = std::make_unique<LineEntity>(line.start(), breakPoint);
    auto l2 = std::make_unique<LineEntity>(breakPoint, line.end());
    return {std::move(l1), std::move(l2)};
}

std::pair<std::unique_ptr<ArcEntity>, std::unique_ptr<ArcEntity>>
breakArc(const ArcEntity& arc, const Point2& breakPoint) {
    // Project breakPoint onto the arc: compute its angle and check it's on the arc.
    const double cx = arc.center().x_, cy = arc.center().y_;
    const double ang = std::atan2(breakPoint.y_ - cy, breakPoint.x_ - cx);
    const double rel = normalizeAngle(ang - arc.startAngle());
    if (rel > arc.sweep() + Tolerance::kAngular) return {nullptr, nullptr};
    // Check the point is actually on the arc (radius match).
    const double dist = std::sqrt((breakPoint.x_ - cx) * (breakPoint.x_ - cx) +
                                  (breakPoint.y_ - cy) * (breakPoint.y_ - cy));
    if (std::abs(dist - arc.radius()) > Tolerance::kLinear) return {nullptr, nullptr};

    const double breakAngle = arc.startAngle() + rel;
    auto a1 = std::make_unique<ArcEntity>(arc.center(), arc.radius(), arc.startAngle(), breakAngle);
    auto a2 = std::make_unique<ArcEntity>(arc.center(), arc.radius(), breakAngle, arc.endAngle());
    return {std::move(a1), std::move(a2)};
}

// ==================== Offset Operations ====================

std::unique_ptr<LineEntity> offsetLine(const LineEntity& line, double distance, const Point2& sidePoint) {
    Point2 dir = {line.end().x_ - line.start().x_, line.end().y_ - line.start().y_};
    double len = std::sqrt(dir.x_ * dir.x_ + dir.y_ * dir.y_);
    if (len < Tolerance::kDegenerateLength) return nullptr;
    
    Point2 normal(-dir.y_ / len, dir.x_ / len);
    
    double cross = (line.end().x_ - line.start().x_) * (sidePoint.y_ - line.start().y_) -
                   (line.end().y_ - line.start().y_) * (sidePoint.x_ - line.start().x_);
    double sign = (cross > 0) ? 1.0 : -1.0;
    
    Point2 newStart = {line.start().x_ + normal.x_ * distance * sign,
                       line.start().y_ + normal.y_ * distance * sign};
    Point2 newEnd = {line.end().x_ + normal.x_ * distance * sign,
                     line.end().y_ + normal.y_ * distance * sign};
    
    return std::make_unique<LineEntity>(newStart, newEnd);
}

std::unique_ptr<CircleEntity> offsetCircle(const CircleEntity& circle, double distance, const Point2& sidePoint) {
    (void)sidePoint;
    double newRadius = circle.radius() + distance;
    if (newRadius < 0) newRadius = 0;
    return std::make_unique<CircleEntity>(circle.center(), newRadius);
}

std::unique_ptr<ArcEntity> offsetArc(const ArcEntity& arc, double distance, const Point2& sidePoint) {
    // Determine offset direction: inward or outward relative to center.
    const double cx = arc.center().x_, cy = arc.center().y_;
    const double dx = sidePoint.x_ - cx, dy = sidePoint.y_ - cy;
    const double sideRadius = std::sqrt(dx * dx + dy * dy);
    const double sign = (sideRadius < arc.radius()) ? -1.0 : 1.0;
    const double newRadius = arc.radius() + sign * distance;
    if (newRadius <= 0.0) return nullptr;
    return std::make_unique<ArcEntity>(arc.center(), newRadius, arc.startAngle(), arc.endAngle());
}

} // namespace bcad::geom