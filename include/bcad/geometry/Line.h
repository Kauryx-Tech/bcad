#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"

namespace bcad::geom {

class LineEntity : public Entity {
public:
    LineEntity() = default;
    LineEntity(Point2 start, Point2 end) : start_(start), end_(end) {}

    EntityType type() const override { return EntityType::Line; }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        bb.expand(start_);
        bb.expand(end_);
        return bb;
    }

    void applyTransform(const AffTransform2& t) override {
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

    double length() const { return distance(start_, end_); }

    const Point2& start() const { return start_; }
    const Point2& end() const { return end_; }
    void setStart(const Point2& p) { start_ = p; }
    void setEnd(const Point2& p) { end_ = p; }

private:
    Point2 start_{0, 0};
    Point2 end_{0, 0};
};

} // namespace bcad::geom
