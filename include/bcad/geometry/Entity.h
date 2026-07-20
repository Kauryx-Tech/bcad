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

// Classe de base pour tout objet dessinable/éditable du document.
// Les entités concrètes possèdent leur représentation géométrique exacte ; tessellate()
// est l'unique pont vers tout ce qui ne veut qu'une approximation en polyligne
// (moteur de rendu, repli pour les tests de sélection, export DXF LWPOLYLINE des courbes, ...).
class Entity {
public:
    virtual ~Entity() = default;

    virtual EntityType type() const = 0;
    virtual BoundingBox boundingBox() const = 0;
    virtual void applyTransform(const AffTransform2& t) = 0;
    virtual std::unique_ptr<Entity> clone() const = 0;

    // Approximation en polyligne de l'entité, assez dense pour qu'aucune corde
    // ne s'écarte de la géométrie réelle de plus de maxDeviation (unités monde).
    virtual std::vector<Point2> tessellate(double maxDeviation) const = 0;

    // Distance la plus courte entre p et la géométrie de l'entité (pour le picking/la sélection).
    virtual double distanceTo(const Point2& p) const = 0;

    int id() const { return id_; }
    void setId(int id) { id_ = id; }

    const std::string& layer() const { return layer_; }
    void setLayer(std::string layer) { layer_ = std::move(layer); }

    // Si non défini, l'entité est dessinée avec la couleur de son calque ("ByLayer").
    const std::optional<Color>& colorOverride() const { return colorOverride_; }
    void setColorOverride(std::optional<Color> c) { colorOverride_ = c; }

    bool selected = false;

protected:
    int id_ = -1;
    std::string layer_ = "0";
    std::optional<Color> colorOverride_;
};

} // namespace bcad::geom
