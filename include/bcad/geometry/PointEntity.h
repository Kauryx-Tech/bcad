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

    // GlRenderer dessine tout en GL_LINE_STRIP (voir Document::buildTessellation,
    // qui ignore silencieusement les tessellations à moins de 2 sommets) : un
    // point seul serait invisible. On dessine donc une petite croix « + » en
    // un seul strip continu (aller-retour par le centre entre chaque bras),
    // ce qui reste visuellement identique à une croix propre. maxDeviation
    // vient de render::worldToleranceForZoom(pixelsPerUnit) — sur son unique
    // site d'appel (Viewport::requestTessellation) il vaut toujours
    // 0.5/pixelsPerUnit, donc pixelsPerUnit ≈ 0.5/maxDeviation ; on vise un
    // bras d'environ 6px à l'écran, quel que soit le zoom.
    std::vector<Point2> tessellate(double maxDeviation) const override {
        double half = 12.0 * maxDeviation;
        double x = CGAL::to_double(position_.x());
        double y = CGAL::to_double(position_.y());
        return {
            Point2(x - half, y), Point2(x, y), Point2(x, y + half), Point2(x, y),
            Point2(x + half, y), Point2(x, y), Point2(x, y - half),
        };
    }

    double distanceTo(const Point2& p) const override { return distance(p, position_); }

    const Point2& position() const { return position_; }
    void setPosition(const Point2& p) { position_ = p; }

private:
    Point2 position_{0, 0};
};

} // namespace bcad::geom
