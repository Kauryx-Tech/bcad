#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Point.h"
#include "bcad/properties/PropertyMap.h"
#include <sstream>

namespace bcad::geom {

class LineEntity : public Entity {
public:
    LineEntity() = default;
    LineEntity(Point2 start, Point2 end) : start_(start), end_(end) {}

    EntityType type() const override { return EntityType::Line; }
    TypeId typeId() const override { return TypeId_Line; }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        bb.expand(start_);
        bb.expand(end_);
        return bb;
    }

    void applyTransform(const Transform2D& t) override {
        start_ = t.transform(start_);
        end_ = t.transform(end_);
    }

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<LineEntity>(*this);
    }

    std::vector<Point2> tessellate(double /*maxDeviation*/) const override {
        return { start_, end_ };
    }

    double distanceTo(const Point2& p) const override {
        return distance(p, closestPointOnSegment(p, start_, end_));
    }

    std::string serializeParams() const override {
        std::ostringstream ss;
        ss.precision(17);
        ss << start_.x_ << ',' << start_.y_ << ',' << end_.x_ << ',' << end_.y_;
        return ss.str();
    }

    void writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const override {
        auto writeGroup = [&](int code, const std::string& value) { f << code << "\n" << value << "\n"; };
        auto writeGroupD = [&](int code, double value) { f << code << "\n" << value << "\n"; };
        
        writeGroup(0, "LINE");
        writeGroup(8, layer);
        if (colorOverride) {
            int r = static_cast<int>(colorOverride->r * 255);
            int g = static_cast<int>(colorOverride->g * 255);
            int b = static_cast<int>(colorOverride->b * 255);
            int aci = (r == g && g == b) ? std::clamp(r / 8, 1, 255) : 7;
            writeGroup(62, std::to_string(aci));
        }
        writeGroupD(10, start_.x_);
        writeGroupD(20, start_.y_);
        writeGroupD(11, end_.x_);
        writeGroupD(21, end_.y_);
    }

    std::string geometryInfo() const override {
        std::ostringstream ss;
        ss.precision(3);
        ss << "Ligne\nLongueur : " << length();
        return ss.str();
    }

    void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const override {
        auto midpoint = [](const Point2& a, const Point2& b) {
            return Point2((a.x_ + b.x_) / 2.0, (a.y_ + b.y_) / 2.0);
        };
        add(start_, SnapPointType::Endpoint);
        add(end_, SnapPointType::Endpoint);
        add(midpoint(start_, end_), SnapPointType::Midpoint);
    }

    double length() const { return distance(start_, end_); }

    const Point2& start() const { return start_; }
    const Point2& end() const { return end_; }
    void setStart(const Point2& p) { start_ = p; }
    void setEnd(const Point2& p) { end_ = p; }

    // PropertyMap access
    properties::PropertyMap& properties() override { return properties_; }
    const properties::PropertyMap& properties() const override { return properties_; }

private:
    Point2 start_{0, 0};
    Point2 end_{0, 0};
    properties::PropertyMap properties_;
};

} // namespace bcad::geom
