#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"

namespace bcad::geom {

class PointEntity : public Entity {
public:
    PointEntity() = default;
    explicit PointEntity(Point2 position) : position_(position) {}

    EntityType type() const override { return EntityType::Point; }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        bb.expand(position_);
        return bb;
    }

    void applyTransform(const AffTransform2& t) override { position_ = t.transform(position_); }

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<PointEntity>(*this);
    }

    std::vector<Point2> tessellate(double /*maxDeviation*/) const override { return { position_ }; }

    double distanceTo(const Point2& p) const override { return distance(p, position_); }

    const Point2& position() const { return position_; }
    void setPosition(const Point2& p) { position_ = p; }

private:
    Point2 position_{0, 0};
};

} // namespace bcad::geom
