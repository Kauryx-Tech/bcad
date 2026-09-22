#pragma once

#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Point.h"
#include "bcad/properties/PropertyMap.h"
#include <cmath>
#include <sstream>

namespace bcad::geom {

// L'arc va dans le sens antihoraire de startAngle à endAngle (radians).
class ArcEntity : public Entity {
public:
    ArcEntity() = default;
    ArcEntity(Point2 center, double radius, double startAngle, double endAngle)
        : center_(center), radius_(radius), startAngle_(startAngle), endAngle_(endAngle) {}

    EntityType type() const override { return EntityType::Arc; }
    TypeId typeId() const override { return TypeId_Arc; }

    double sweep() const {
        double s = normalizeAngle(endAngle_) - normalizeAngle(startAngle_);
        if (s <= 0) s += 2.0 * std::numbers::pi;
        return s;
    }

    Point2 startPoint() const {
        return { center_.x_ + radius_ * std::cos(startAngle_),
                  center_.y_ + radius_ * std::sin(startAngle_) };
    }

    Point2 endPoint() const {
        return { center_.x_ + radius_ * std::cos(endAngle_),
                  center_.y_ + radius_ * std::sin(endAngle_) };
    }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        bb.expand(startPoint());
        bb.expand(endPoint());
        // Inclut les extrema alignés sur les axes (0, 90, 180, 270 deg) qui
        // tombent dans le balayage, car les seules extrémités peuvent sous-estimer largement la boîte.
        double cx = center_.x_, cy = center_.y_;
        for (double a : {0.0, std::numbers::pi / 2, std::numbers::pi, 3 * std::numbers::pi / 2}) {
            double rel = normalizeAngle(a - startAngle_);
            if (rel <= sweep()) {
                bb.expand(Point2(cx + radius_ * std::cos(a), cy + radius_ * std::sin(a)));
            }
        }
        return bb;
    }

    void applyTransform(const Transform2D& t) override {
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
        double cx = center_.x_, cy = center_.y_;
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

    std::string serializeParams() const override {
        std::ostringstream ss;
        ss.precision(17);
        ss << center_.x_ << ',' << center_.y_ << ',' << radius_ << ',' << startAngle_ << ',' << endAngle_;
        return ss.str();
    }

    void writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const override {
        auto writeGroup = [&](int code, const std::string& value) { f << code << "\n" << value << "\n"; };
        auto writeGroupD = [&](int code, double value) { f << code << "\n" << value << "\n"; };
        
        writeGroup(0, "ARC");
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
        writeGroupD(50, geom::toDegrees(startAngle_));
        writeGroupD(51, geom::toDegrees(endAngle_));
    }

    std::string geometryInfo() const override {
        std::ostringstream ss;
        ss.precision(3);
        ss << "Arc\nRadius: " << radius_ << "\nSweep: " << geom::toDegrees(sweep()) << "°";
        return ss.str();
    }

    void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const override {
        add(center_, SnapPointType::Center);
        add(startPoint(), SnapPointType::Endpoint);
        add(endPoint(), SnapPointType::Endpoint);
        for (double ang : { 0.0, std::numbers::pi / 2, std::numbers::pi, 3 * std::numbers::pi / 2 }) {
            double rel = geom::normalizeAngle(ang - startAngle_);
            if (rel <= sweep()) {
                add(Point2(center_.x_ + radius_ * std::cos(ang), center_.y_ + radius_ * std::sin(ang)),
                    SnapPointType::Quadrant);
            }
        }
    }

    const Point2& center() const { return center_; }
    double radius() const { return radius_; }
    double startAngle() const { return startAngle_; }
    double endAngle() const { return endAngle_; }
    void setCenter(const Point2& c) { center_ = c; }
    void setRadius(double r) { radius_ = r; }
    void setStartAngle(double a) { startAngle_ = a; }
    void setEndAngle(double a) { endAngle_ = a; }

    // PropertyMap access
    properties::PropertyMap& properties() override { return properties_; }
    const properties::PropertyMap& properties() const override { return properties_; }

private:
    Point2 center_{0, 0};
    double radius_ = 1.0;
    double startAngle_ = 0.0;
    double endAngle_ = std::numbers::pi;
    properties::PropertyMap properties_;
};

} // namespace bcad::geom