#pragma once

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include <algorithm>
#include <memory>
#include <vector>

namespace bcad::geom {

// ==================== Basic Types for Intersection ====================

struct Line2 {
    Point2 a;
    Point2 b;
    
    Line2() = default;
    Line2(const Point2& a, const Point2& b) : a(a), b(b) {}
};

struct Circle2 {
    Point2 center;
    double radius = 0.0;
    
    Circle2() = default;
    Circle2(const Point2& c, double r) : center(c), radius(r) {}
};

struct Segment2 {
    Point2 a;
    Point2 b;
    
    Segment2() = default;
    Segment2(const Point2& a, const Point2& b) : a(a), b(b) {}
};

struct Arc2 {
    Point2 center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 0.0;
    
    Arc2() = default;
    Arc2(const Point2& c, double r, double sa, double ea) 
        : center(c), radius(r), startAngle(sa), endAngle(ea) {}
};

struct Polyline2 {
    std::vector<Point2> vertices;
    bool closed = false;
};

// ==================== Intersection Functions ====================

// Line / Line intersection
inline std::optional<Point2> intersect(const Line2& a, const Line2& b) {
    Point2 r = {a.b.x_ - a.a.x_, a.b.y_ - a.a.y_};
    Point2 s = {b.b.x_ - b.a.x_, b.b.y_ - b.a.y_};
    
    double rxs = r.x_ * s.y_ - r.y_ * s.x_;
    if (std::abs(r.x_ * s.y_ - r.y_ * s.x_) < Tolerance::kDegenerateLength) 
        return std::nullopt;
    
    Point2 qp = {b.a.x_ - a.a.x_, b.a.y_ - a.a.y_};
    double t = (qp.x_ * s.y_ - qp.y_ * s.x_) / (r.x_ * s.y_ - r.y_ * s.x_);
    double u = (qp.x_ * r.y_ - qp.y_ * r.x_) / (r.x_ * s.y_ - r.y_ * s.x_);
    
    if (t < -Tolerance::kLinear || t > 1.0 + Tolerance::kLinear ||
        u < -Tolerance::kLinear || u > 1.0 + Tolerance::kLinear) 
        return std::nullopt;
    
    return Point2{ a.a.x_ + t * (a.b.x_ - a.a.x_), a.a.y_ + t * (a.b.y_ - a.a.y_) };
}

// Segment / Segment intersection
inline std::optional<Point2> intersect(const Segment2& a, const Segment2& b) {
    Point2 r = {a.b.x_ - a.a.x_, a.b.y_ - a.a.y_};
    Point2 s = {b.b.x_ - b.a.x_, b.b.y_ - b.a.y_};
    
    double rxs = r.x_ * s.y_ - r.y_ * s.x_;
    if (std::abs(r.x_ * s.y_ - r.y_ * s.x_) < Tolerance::kDegenerateLength) 
        return std::nullopt;
    
    Point2 qp = {b.a.x_ - a.a.x_, b.a.y_ - a.a.y_};
    double t = (qp.x_ * s.y_ - qp.y_ * s.x_) / (r.x_ * s.y_ - r.y_ * s.x_);
    double u = (qp.x_ * r.y_ - qp.y_ * r.x_) / (r.x_ * s.y_ - r.y_ * s.x_);
    
    if (t < -Tolerance::kLinear || t > 1.0 + Tolerance::kLinear ||
        u < -Tolerance::kLinear || u > 1.0 + Tolerance::kLinear) 
        return std::nullopt;
    
    return Point2{ a.a.x_ + t * (a.b.x_ - a.a.x_), a.a.y_ + t * (a.b.y_ - a.a.y_) };
}

// Line / Circle intersection
inline std::vector<Point2> intersect(const Line2& line, const Circle2& circle) {
    std::vector<Point2> out;
    Point2 d = {line.b.x_ - line.a.x_, line.b.y_ - line.a.y_};
    Point2 f = {line.a.x_ - circle.center.x_, line.a.y_ - circle.center.y_};
    
    double A = d.x_ * d.x_ + d.y_ * d.y_;
    double B = 2.0 * (f.x_ * d.x_ + f.y_ * d.y_);
    double C = f.x_ * f.x_ + f.y_ * f.y_ - circle.radius * circle.radius;
    
    double disc = B * B - 4.0 * A * C;
    if (disc < 0.0) return out;
    double sq = std::sqrt(disc);
    
    for (double t : { (-B - sq) / (2.0 * A), (-B + sq) / (2.0 * A) }) {
        if (t >= -Tolerance::kLinear && t <= 1.0 + Tolerance::kLinear) {
            double tc = std::clamp(t, 0.0, 1.0);
            out.emplace_back(line.a.x_ + tc * d.x_, line.a.y_ + tc * d.y_);
        }
    }
    if (out.size() == 2 && distance(out[0], out[1]) < Tolerance::kLinear) out.pop_back();
    return out;
}

// Segment / Circle intersection
inline std::vector<Point2> intersect(const Segment2& seg, const Circle2& circle) {
    auto pts = intersect(Line2{seg.a, seg.b}, Circle2{circle.center, circle.radius});
    std::vector<Point2> out;
    for (const auto& p : pts) {
        double minX = std::min(seg.a.x_, seg.b.x_) - Tolerance::kLinear;
        double maxX = std::max(seg.a.x_, seg.b.x_) + Tolerance::kLinear;
        double minY = std::min(seg.a.y_, seg.b.y_) - Tolerance::kLinear;
        double maxY = std::max(seg.a.y_, seg.b.y_) + Tolerance::kLinear;
        if (p.x_ >= minX && p.x_ <= maxX && p.y_ >= minY - Tolerance::kLinear && 
            p.y_ <= maxY + Tolerance::kLinear) {
            out.push_back(p);
        }
    }
    return out;
}

// Circle / Circle intersection
inline std::vector<Point2> intersect(const Circle2& c1, const Circle2& c2) {
    std::vector<Point2> out;
    double dx = c2.center.x_ - c1.center.x_;
    double dy = c2.center.y_ - c1.center.y_;
    double d = std::sqrt(dx * dx + dy * dy);
    
    if (d < Tolerance::kDegenerateLength) return out;
    if (d > c1.radius + c2.radius + Tolerance::kLinear || 
        d < std::abs(c1.radius - c2.radius) - Tolerance::kLinear) 
        return out;
    
    double a = (c1.radius * c1.radius - c2.radius * c2.radius + d * d) / (2.0 * d);
    double hSq = std::max(0.0, c1.radius * c1.radius - a * a);
    double h = std::sqrt(hSq);
    
    double mx = c1.center.x_ + a * dx / d;
    double my = c1.center.y_ + a * dy / d;
    double rx = -dy * (h / d), ry = dx * (h / d);
    
    out.emplace_back(mx + rx, my + ry);
    if (h > Tolerance::kLinear) out.emplace_back(mx - rx, my - ry);
    return out;
}

// Line / Polyline intersection
inline std::vector<Point2> intersect(const Line2& line, const Polyline2& poly) {
    std::vector<Point2> out;
    for (size_t i = 0; i + 1 < poly.vertices.size(); ++i) {
        Segment2 seg{poly.vertices[i], poly.vertices[i+1]};
        if (auto pt = intersect(Segment2{line.a, line.b}, Segment2{poly.vertices[i], poly.vertices[i+1]})) {
            out.push_back(*pt);
        }
    }
    if (poly.closed && poly.vertices.size() > 2) {
        Segment2 seg{poly.vertices.back(), poly.vertices.front()};
        if (auto pt = intersect(Segment2{line.a, line.b}, seg)) {
            out.push_back(*pt);
        }
    }
    return out;
}

// Entity intersection (implemented in .cpp)
std::vector<Point2> entityIntersections(const Entity& a, const Entity& b);

// ==================== Trim Operations ====================

bool trimLine(LineEntity& line, const Entity& cuttingEntity, const Point2& pickPoint);
bool trimPolyline(PolylineEntity& polyline, const Entity& cuttingEntity, const Point2& pickPoint);

// ==================== Extend Operations ====================

bool extendLine(LineEntity& line, const Entity& boundaryEntity, const Point2& pickPoint);

// ==================== Break Operations ====================

std::pair<std::unique_ptr<LineEntity>, std::unique_ptr<LineEntity>> 
breakLine(const LineEntity& line, const Point2& breakPoint);
void breakPolyline(PolylineEntity& polyline, const Point2& breakPoint);

// ==================== Offset Operations ====================

std::unique_ptr<LineEntity> offsetLine(const LineEntity& line, double distance, const Point2& sidePoint);
std::unique_ptr<CircleEntity> offsetCircle(const CircleEntity& circle, double distance, const Point2& sidePoint);
std::unique_ptr<PolylineEntity> offsetPolyline(const PolylineEntity& poly, double distance, const Point2& sidePoint);

} // namespace bcad::geom