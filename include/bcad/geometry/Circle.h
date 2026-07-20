#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace bcad::geom {

class CircleEntity : public Entity {
public:
    CircleEntity() = default;
    CircleEntity(Point2 center, double radius) : center_(center), radius_(radius) {}

    EntityType type() const override { return EntityType::Circle; }

    BoundingBox boundingBox() const override {
        return BoundingBox{ CGAL::to_double(center_.x()) - radius_,
                             CGAL::to_double(center_.y()) - radius_,
                             CGAL::to_double(center_.x()) + radius_,
                             CGAL::to_double(center_.y()) + radius_ };
    }

    void applyTransform(const AffTransform2& t) override {
        // Sûr en cas d'échelle uniforme : transforme le centre, puis redérive le
        // rayon à partir d'un point du cercle afin qu'une échelle non uniforme se dégrade correctement.
        Point2 edge(CGAL::to_double(center_.x()) + radius_, CGAL::to_double(center_.y()));
        center_ = t.transform(center_);
        Point2 edgeT = t.transform(edge);
        radius_ = distance(center_, edgeT);
    }

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<CircleEntity>(*this);
    }

    // Le nombre de segments croît avec le rayon/l'écart pour que les silhouettes
    // restent lisses à tout niveau de zoom (formule classique de LOD basée sur la flèche).
    std::vector<Point2> tessellate(double maxDeviation) const override {
        int segments = segmentCountForDeviation(radius_, maxDeviation);
        std::vector<Point2> pts;
        pts.reserve(segments + 1);
        for (int i = 0; i <= segments; ++i) {
            double a = 2.0 * std::numbers::pi * i / segments;
            pts.emplace_back(CGAL::to_double(center_.x()) + radius_ * std::cos(a),
                              CGAL::to_double(center_.y()) + radius_ * std::sin(a));
        }
        return pts;
    }

    double distanceTo(const Point2& p) const override {
        return std::abs(distance(p, center_) - radius_);
    }

    static int segmentCountForDeviation(double radius, double maxDeviation) {
        if (radius <= 0.0) return 8;
        double dev = std::clamp(maxDeviation, radius * 1e-6, radius);
        double angleStep = 2.0 * std::acos(1.0 - dev / radius);
        int segments = static_cast<int>(std::ceil(2.0 * std::numbers::pi / angleStep));
        return std::clamp(segments, 12, 256);
    }

    const Point2& center() const { return center_; }
    double radius() const { return radius_; }
    void setCenter(const Point2& c) { center_ = c; }
    void setRadius(double r) { radius_ = r; }

private:
    Point2 center_{0, 0};
    double radius_ = 1.0;
};

} // namespace bcad::geom
