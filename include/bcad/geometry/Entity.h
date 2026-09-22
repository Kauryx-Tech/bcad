#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/Transform2D.h"
#include "bcad/geometry/TypeId.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bcad::properties {
class PropertyMap;
} // namespace bcad::properties

namespace bcad::geom {

class DxfWriter; // Forward declaration

// DEPRECATED: Use TypeId instead. Kept for backward compatibility with old file formats.
// Will be removed in a future version.
enum class [[deprecated("Use TypeId instead. Kept for backward compatibility with old file formats.")]] EntityType { Point, Line, Circle, Arc, Polyline };

[[deprecated("Use TypeId instead. Kept for backward compatibility.")]]
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

// TypeId stables pour les entités natives BCAD (utilisés par le registry et la sérialisation).
inline constexpr TypeId TypeId_Point{"bcad.Point"};
inline constexpr TypeId TypeId_Line{"bcad.Line"};
inline constexpr TypeId TypeId_Circle{"bcad.Circle"};
inline constexpr TypeId TypeId_Arc{"bcad.Arc"};
inline constexpr TypeId TypeId_Polyline{"bcad.Polyline"};

// Classe de base pour tout objet dessinable/éditable du document.
// Les entités concrètes possèdent leur représentation géométrique exacte ; tessellate()
// est l'unique pont vers tout ce qui ne veut qu'une approximation en polyligne
// (moteur de rendu, repli pour les tests de sélection, export DXF LWPOLYLINE des courbes, ...).
class Entity {
public:
    virtual ~Entity() = default;

    // DEPRECATED: Use typeId() instead. Kept for backward compatibility.
    [[deprecated("Use typeId() instead. Kept for backward compatibility.")]]
    virtual EntityType type() const = 0;
    virtual TypeId typeId() const = 0;
    virtual BoundingBox boundingBox() const = 0;
    virtual void applyTransform(const Transform2D& t) = 0;
    virtual std::unique_ptr<Entity> clone() const = 0;

    // Approximation en polyligne de l'entité, assez dense pour qu'aucune corde
    // ne s'écarte de la géométrie réelle de plus de maxDeviation (unités monde).
    virtual std::vector<Point2> tessellate(double maxDeviation) const = 0;

    // Distance la plus courte entre p et la géométrie de l'entité (pour le picking/la sélection).
    virtual double distanceTo(const Point2& p) const = 0;

    // Sérialisation des paramètres géométriques (format CSV pour compatibilité SQLite).
    // Format par type:
    //   Point: "x,y"
    //   Line: "x1,y1,x2,y2"
    //   Circle: "cx,cy,r"
    //   Arc: "cx,cy,r,startAngle,endAngle"
    //   Polyline: "closed,x1,y1,x2,y2,..."
    virtual std::string serializeParams() const = 0;

    // Écriture DXF : chaque entité sait s'écrire elle-même.
    // L'implémentation concrète se trouve dans le module io (DxfWriter.cpp).
    // On utilise un type effacé (std::ostream) pour éviter la dépendance circulaire.
    virtual void writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const = 0;

    // Retourne une description textuelle des propriétés géométriques pour l'UI.
    virtual std::string geometryInfo() const = 0;

    // Types de snap pour l'accrochage objet (utilisés par SnapEngine).
    enum class SnapPointType { Endpoint, Midpoint, Center, Quadrant, Intersection, Perpendicular, Nearest };

    // Ajoute les points d'accrochage caractéristiques de cette entité.
    // `cursor` est la position actuelle du curseur (pour le tri par distance).
    // `add` est appelé pour chaque point caractéristique avec son type.
    using SnapCallback = std::function<void(const Point2&, SnapPointType)>;
    void addSnapCandidates(const Point2& cursor, SnapCallback add) const {
        doAddSnapCandidates(cursor, add);
    }

    // PropertyMap access
    virtual properties::PropertyMap& properties() = 0;
    virtual const properties::PropertyMap& properties() const = 0;

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

    // Point d'extension pour addSnapCandidates (NVI pattern).
    virtual void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const = 0;
};

} // namespace bcad::geom