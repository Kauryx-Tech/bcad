#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Point.h"
#include "bcad/properties/PropertyMap.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>

namespace bcad::geom {

class CircleEntity : public Entity {
public:
    CircleEntity() = default;
    CircleEntity(Point2 center, double radius) : center_(center), radius_(radius) {}

    EntityType type() const override { return EntityType::Circle; }
    TypeId typeId() const override { return TypeId_Circle; }

    BoundingBox boundingBox() const override {
        return BoundingBox{ center_.x_ - radius_,
                             center_.y_ - radius_,
                             center_.x_ + radius_,
                             center_.y_ + radius_ };
    }

    void applyTransform(const Transform2D& t) override {
        // Sûr en cas d'échelle uniforme : transforme le centre, puis redérive le
        // rayon à partir d'un point du cercle afin qu'une échelle non uniforme se dégrade correctement.
        Point2 edge(center_.x_ + radius_, center_.y_);
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
            pts.emplace_back(center_.x_ + radius_ * std::cos(a),
                              center_.y_ + radius_ * std::sin(a));
        }
        return pts;
    }

    double distanceTo(const Point2& p) const override {
        return std::abs(distance(p, center_) - radius_);
    }

    std::string serializeParams() const override {
        std::ostringstream ss;
        ss.precision(17);
        ss << center_.x_ << ',' << center_.y_ << ',' << radius_;
        return ss.str();
    }

    void writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const override {
        auto writeGroup = [&](int code, const std::string& value) { f << code << "\n" << value << "\n"; };
        auto writeGroupD = [&](int code, double value) { f << code << "\n" << value << "\n"; };
        
        writeGroup(0, "CIRCLE");
        writeGroup(8, layer);
        if (colorOverride) {
            int r = static_cast<int>(colorOverride->r * 255);
            int g = static_cast<int>(colorOverride->g * 255);
            int b = static_cast<int>(colorOverride->b * 255);
            int aci = (r == g && g == b) ? std::clamp(r / 8, 1, 255) : 7;
            writeGroup(62, std::to_string(aci));
        }
        writeGroupD(10, center_.x_);
        writeGroupD(20, center_.y_);
        writeGroupD(40, radius_);
    }

    std::string geometryInfo() const override {
        std::ostringstream ss;
        ss.precision(3);
        ss << "Circle\nRadius: " << radius_ << "\nCenter: (" << center_.x_ << ", " << center_.y_ << ")";
        return ss.str();
    }

    void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const override {
        add(center_, SnapPointType::Center);
        for (double a : { 0.0, std::numbers::pi / 2, std::numbers::pi, 3 * std::numbers::pi / 2 }) {
            add(Point2(center_.x_ + radius_ * std::cos(a), center_.y_ + radius_ * std::sin(a)), SnapPointType::Quadrant);
        }
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

    // PropertyMap access
    properties::PropertyMap& properties() override { return properties_; }
    const properties::PropertyMap& properties() const override { return properties_; }

private:
    Point2 center_{0, 0};
    double radius_ = 1.0;
    properties::PropertyMap properties_;
};

} // namespace bcad::geom