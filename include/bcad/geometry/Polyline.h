#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include <limits>

namespace bcad::geom {

class PolylineEntity : public Entity {
public:
    PolylineEntity() = default;
    explicit PolylineEntity(std::vector<Point2> vertices, bool closed = false)
        : vertices_(std::move(vertices)), closed_(closed) {}

    EntityType type() const override { return EntityType::Polyline; }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        for (const auto& v : vertices_) bb.expand(v);
        return bb;
    }

    void applyTransform(const AffTransform2& t) override {
        for (auto& v : vertices_) v = t.transform(v);
    }

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<PolylineEntity>(*this);
    }

    std::vector<Point2> tessellate(double /*maxDeviation*/) const override {
        std::vector<Point2> pts = vertices_;
        if (closed_ && !pts.empty()) pts.push_back(pts.front());
        return pts;
    }

    double distanceTo(const Point2& p) const override {
        double best = std::numeric_limits<double>::infinity();
        std::size_t n = vertices_.size();
        if (n == 0) return best;
        if (n == 1) return distance(p, vertices_[0]);
        std::size_t segCount = closed_ ? n : n - 1;
        for (std::size_t i = 0; i < segCount; ++i) {
            const Point2& a = vertices_[i];
            const Point2& b = vertices_[(i + 1) % n];
            best = std::min(best, distance(p, closestPointOnSegment(p, a, b)));
        }
        return best;
    }

    double length() const {
        double total = 0.0;
        std::size_t n = vertices_.size();
        if (n < 2) return 0.0;
        std::size_t segCount = closed_ ? n : n - 1;
        for (std::size_t i = 0; i < segCount; ++i) {
            total += distance(vertices_[i], vertices_[(i + 1) % n]);
        }
        return total;
    }

    // Only meaningful for closed polylines with no self-intersections;
    // used as the bridge into CGAL::Polygon_2 for boolean ops / triangulation.
    Polygon2 toPolygon() const {
        Polygon2 poly;
        for (const auto& v : vertices_) poly.push_back(v);
        return poly;
    }

    static PolylineEntity fromPolygon(const Polygon2& poly) {
        std::vector<Point2> verts(poly.vertices_begin(), poly.vertices_end());
        return PolylineEntity(std::move(verts), true);
    }

    const std::vector<Point2>& vertices() const { return vertices_; }
    std::vector<Point2>& vertices() { return vertices_; }
    void addVertex(const Point2& p) { vertices_.push_back(p); }

    bool closed() const { return closed_; }
    void setClosed(bool c) { closed_ = c; }

private:
    std::vector<Point2> vertices_;
    bool closed_ = false;
};

} // namespace bcad::geom
