#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/properties/PropertyMap.h"
#include <sstream>

namespace bcad::geom {

class PointEntity : public Entity {
public:
    PointEntity() = default;
    explicit PointEntity(Point2 position) : position_(position) {}

    EntityType type() const override { return EntityType::Point; }
    TypeId typeId() const override { return TypeId_Point; }

    BoundingBox boundingBox() const override {
        BoundingBox bb;
        bb.expand(position_);
        return bb;
    }

    void applyTransform(const Transform2D& t) override { position_ = t.transform(position_); }

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
        double x = position_.x_;
        double y = position_.y_;
        return {
            Point2(x - half, y), Point2(x, y), Point2(x, y + half), Point2(x, y),
            Point2(x + half, y), Point2(x, y), Point2(x, y - half),
        };
    }

    double distanceTo(const Point2& p) const override { return distance(p, position_); }

    std::string serializeParams() const override {
        std::ostringstream ss;
        ss.precision(17);
        ss << position_.x_ << ',' << position_.y_;
        return ss.str();
    }

    void writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const override {
        // Export de l'entité POINT omis dans ce sous-ensemble MVP
        (void)f; (void)layer; (void)colorOverride;
    }

    std::string geometryInfo() const override {
        std::ostringstream ss;
        ss.precision(3);
        ss << "Point\n(" << position_.x_ << ", " << position_.y_ << ")";
        return ss.str();
    }

    void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const override {
        add(position_, SnapPointType::Endpoint);
    }

    const Point2& position() const { return position_; }
    void setPosition(const Point2& p) { position_ = p; }

    // PropertyMap access
    properties::PropertyMap& properties() override { return properties_; }
    const properties::PropertyMap& properties() const override { return properties_; }

private:
    Point2 position_{0, 0};
    properties::PropertyMap properties_;
};

} // namespace bcad::geom
