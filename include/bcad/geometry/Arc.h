#pragma once

#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include <cmath>

namespace bcad::geom {

// L'arc va dans le sens antihoraire de startAngle à endAngle (radians).
class ArcEntity : public Entity {
public:
    ArcEntity() = default;
    ArcEntity(Point2 center, double radius, double startAngle, double endAngle)
        : center_(center), radius_(radius), startAngle_(startAngle), endAngle_(endAngle) {}

    EntityType type() const override { return EntityType::Arc; }

    double sweep() const {
        double s = normalizeAngle(endAngle_) - normalizeAngle(startAngle_);
        if (s <= 0) s += 2.0 * std::numbers::pi;
        return s;
    }

    Point2 startPoint() const {
        return { CGAL::to_double(center_.x()) + radius_ * std::cos(startAngle_),
                 CGAL::to_double(center_.y()) + radius_ * std::sin(startAngle_) };
    }

    Point2 endPoint() const {
        return { CGAL::to_double(center_.x()) + radius_ * std::cos(endAngle_),
                 CGAL::to_double(center_.y()) + radius_ * std::sin(endAngle_) };
    }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        bb.expand(startPoint());
        bb.expand(endPoint());
        // Inclut les extrema alignés sur les axes (0, 90, 180, 270 deg) qui
        // tombent dans le balayage, car les seules extrémités peuvent sous-estimer largement la boîte.
        double cx = CGAL::to_double(center_.x()), cy = CGAL::to_double(center_.y());
        for (double a : {0.0, std::numbers::pi / 2, std::numbers::pi, 3 * std::numbers::pi / 2}) {
            double rel = normalizeAngle(a - startAngle_);
            if (rel <= sweep()) {
                bb.expand(Point2(cx + radius_ * std::cos(a), cy + radius_ * std::sin(a)));
            }
        }
        return bb;
    }

    void applyTransform(const AffTransform2& t) override {
        Point2 sp = t.transform(startPoint());
        Point2 ep = t.transform(endPoint());
        Point2 old = center_;
        center_ = t.transform(center_);
        radius_ = distance(center_, sp);
        startAngle_ = angleOf(center_, sp);
        endAngle_ = angleOf(center_, ep);
        (void)old;
    }

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<ArcEntity>(*this);
    }

    std::vector<Point2> tessellate(double maxDeviation) const override {
        int fullCircleSegments = CircleEntity::segmentCountForDeviation(radius_, maxDeviation);
        int segments = std::max(2, static_cast<int>(std::ceil(fullCircleSegments * sweep() / (2.0 * std::numbers::pi))));
        std::vector<Point2> pts;
        pts.reserve(segments + 1);
        double cx = CGAL::to_double(center_.x()), cy = CGAL::to_double(center_.y());
        for (int i = 0; i <= segments; ++i) {
            double a = startAngle_ + sweep() * i / segments;
            pts.emplace_back(cx + radius_ * std::cos(a), cy + radius_ * std::sin(a));
        }
        return pts;
    }

    double distanceTo(const Point2& p) const override {
        double a = angleOf(center_, p);
        double rel = normalizeAngle(a - startAngle_);
        if (rel <= sweep()) {
            return std::abs(distance(p, center_) - radius_);
        }
        return std::min(distance(p, startPoint()), distance(p, endPoint()));
    }

    const Point2& center() const { return center_; }
    double radius() const { return radius_; }
    double startAngle() const { return startAngle_; }
    double endAngle() const { return endAngle_; }
    void setCenter(const Point2& c) { center_ = c; }
    void setRadius(double r) { radius_ = r; }
    void setStartAngle(double a) { startAngle_ = a; }
    void setEndAngle(double a) { endAngle_ = a; }

private:
    Point2 center_{0, 0};
    double radius_ = 1.0;
    double startAngle_ = 0.0;
    double endAngle_ = std::numbers::pi;
};

} // namespace bcad::geom
