#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Types.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bcad::geom {

enum class EntityType { Point, Line, Circle, Arc, Polyline };

inline const char* entityTypeName(EntityType t) {
    switch (t) {
        case EntityType::Point: return "POINT";
        case EntityType::Line: return "LINE";
        case EntityType::Circle: return "CIRCLE";
        case EntityType::Arc: return "ARC";
        case EntityType::Polyline: return "POLYLINE";
    }
    return "UNKNOWN";
}

// Base class for every drawable/editable object in the document.
// Concrete entities own their exact geometric representation; tessellate()
// is the single bridge to anything that only wants a polyline approximation
// (renderer, hit-testing fallback, DXF LWPOLYLINE export of curves, ...).
class Entity {
public:
    virtual ~Entity() = default;

    virtual EntityType type() const = 0;
    virtual BoundingBox boundingBox() const = 0;
    virtual void applyTransform(const AffTransform2& t) = 0;
    virtual std::unique_ptr<Entity> clone() const = 0;

    // Polyline approximation of the entity, dense enough that no chord
    // deviates from the true geometry by more than maxDeviation (world units).
    virtual std::vector<Point2> tessellate(double maxDeviation) const = 0;

    // Shortest distance from p to the entity's geometry (for pick/select).
    virtual double distanceTo(const Point2& p) const = 0;

    int id() const { return id_; }
    void setId(int id) { id_ = id; }

    const std::string& layer() const { return layer_; }
    void setLayer(std::string layer) { layer_ = std::move(layer); }

    // When unset, the entity is drawn with its layer's color ("ByLayer").
    const std::optional<Color>& colorOverride() const { return colorOverride_; }
    void setColorOverride(std::optional<Color> c) { colorOverride_ = c; }

    bool selected = false;

protected:
    int id_ = -1;
    std::string layer_ = "0";
    std::optional<Color> colorOverride_;
};

} // namespace bcad::geom
