#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/properties/PropertyMap.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace bcad::geom {

class TextEntity final : public Entity {
public:
    TextEntity() = default;
    TextEntity(Point2 position, std::string text, double height = 3.0, double rotation = 0.0)
        : position_(position), text_(std::move(text)), height_(height), rotation_(rotation) {}

    EntityType type() const override { return EntityType::Text; }
    TypeId typeId() const override { return TypeId_Text; }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        const double width = std::max(height_, 0.1) * std::max<std::size_t>(1, text_.size()) * 0.6;
        bb.expand(position_);
        bb.expand(Point2(position_.x_ + width, position_.y_ + std::max(height_, 0.1)));
        return bb;
    }

    void applyTransform(const Transform2D& t) override {
        position_ = t.transform(position_);
    }

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<TextEntity>(*this);
    }

    std::vector<Point2> tessellate(double /*maxDeviation*/) const override {
        return {position_, Point2(position_.x_ + std::max(height_, 0.1), position_.y_)};
    }

    double distanceTo(const Point2& p) const override {
        return distance(p, position_);
    }

    std::string serializeParams() const override {
        std::ostringstream ss;
        ss.precision(17);
        ss << position_.x_ << ',' << position_.y_ << ',' << height_ << ',' << rotation_
           << ',' << text_;
        return ss.str();
    }

    void writeDxf(std::ostream& f, const std::string& layer,
                  const std::optional<Color>& /*colorOverride*/) const override {
        f << "0\nTEXT\n8\n" << layer << "\n"
          << "10\n" << position_.x_ << "\n20\n" << position_.y_ << "\n"
          << "40\n" << height_ << "\n1\n" << text_ << "\n50\n"
          << rotation_ * 180.0 / 3.14159265358979323846 << "\n";
    }

    std::string geometryInfo() const override {
        return "Texte\n" + text_;
    }

    void doAddSnapCandidates(const Point2& /*cursor*/, SnapCallback add) const override {
        add(position_, SnapPointType::Endpoint);
    }

    const Point2& position() const { return position_; }
    const std::string& text() const { return text_; }
    double height() const { return height_; }
    double rotation() const { return rotation_; }
    void setPosition(const Point2& p) { position_ = p; }
    void setText(std::string text) { text_ = std::move(text); }
    void setHeight(double height) { height_ = std::max(height, 0.1); }
    void setRotation(double rotation) { rotation_ = rotation; }

    properties::PropertyMap& properties() override { return properties_; }
    const properties::PropertyMap& properties() const override { return properties_; }

private:
    Point2 position_{0, 0};
    std::string text_;
    double height_ = 3.0;
    double rotation_ = 0.0;
    properties::PropertyMap properties_;
};

} // namespace bcad::geom
