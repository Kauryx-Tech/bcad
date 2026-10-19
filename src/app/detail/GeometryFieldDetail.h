// Champs de la section « Géométrie » de la palette Propriétés (P-01) : quels
// champs une entité expose, la valeur de chacun, et le clone obtenu en en
// changeant un. Pur, sans Qt : le widget n'est qu'un éditeur de texte autour
// de ces fonctions, qui seules sont testées (tests/unit/app/GeometryFieldDetailTest).
//
// Une écriture ne modifie jamais l'entité du document : elle rend un clone
// (comme GripDetail.h), que l'appelant remplace par EditBatch — un seul pas
// d'annulation, l'identifiant étant conservé.

#pragma once

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace bcad::app::detail {

// Les champs qu'une entité expose, dans l'ordre où la section les affiche.
// Length, ArcLength, Diameter et Area sont calculés : jamais éditables.
enum class GeometryField {
    StartX, StartY, EndX, EndY,      // ligne
    CenterX, CenterY, Radius,        // cercle, arc
    StartAngle, EndAngle,            // arc, en degrés, sens trigonométrique
    PositionX, PositionY,            // point
    VertexX, VertexY,                // polyligne, sommet choisi
    Length, ArcLength, Diameter, Area,
};

// Refus d'une saisie : le champ garde sa valeur et la section montre un bord
// rouge avec le message correspondant — jamais une fenêtre de dialogue
// (docs/DESIGN_SYSTEM.md, « refus »).
enum class GeometryEditReject {
    None,
    NotFinite,           // vide, « 12a », « nan », « inf »
    RadiusNotPositive,   // rayon nul ou négatif
    Degenerate,          // début et fin confondus sur une ligne
    InvalidVertex,       // sommet hors de la polyligne
    NotApplicable,       // champ demandé pour un autre type d'entité
    ReadOnly,            // champ calculé, jamais écrit
};

// Champ en lecture seule : visible dans la section, désactivé dans l'interface.
inline bool isReadOnlyGeometryField(GeometryField field) {
    switch (field) {
        case GeometryField::Length:
        case GeometryField::ArcLength:
        case GeometryField::Diameter:
        case GeometryField::Area:
            return true;
        default:
            return false;
    }
}

// Champs de `e`, dans l'ordre d'affichage. Vide : entité sans champs propres
// (texte, cotation, type inconnu) — la section ne s'affiche alors pas.
inline std::vector<GeometryField> geometryFields(const geom::Entity& e) {
    if (e.typeId() == geom::TypeId_Line) {
        return {GeometryField::StartX, GeometryField::StartY, GeometryField::EndX, GeometryField::EndY,
                GeometryField::Length};
    }
    if (e.typeId() == geom::TypeId_Circle) {
        return {GeometryField::CenterX, GeometryField::CenterY, GeometryField::Radius, GeometryField::Diameter,
                GeometryField::Area};
    }
    if (e.typeId() == geom::TypeId_Arc) {
        return {GeometryField::CenterX, GeometryField::CenterY, GeometryField::Radius, GeometryField::StartAngle,
                GeometryField::EndAngle, GeometryField::ArcLength};
    }
    if (e.typeId() == geom::TypeId_Point) {
        return {GeometryField::PositionX, GeometryField::PositionY};
    }
    if (const auto* poly = dynamic_cast<const geom::PolylineEntity*>(&e)) {
        std::vector<GeometryField> out = {GeometryField::VertexX, GeometryField::VertexY, GeometryField::Length};
        // L'aire n'a de sens que fermée : sur une polyligne ouverte, le
        // « polygone » implicite est celui de la fermeture, pas le sien.
        if (poly->closed()) out.push_back(GeometryField::Area);
        return out;
    }
    return {};
}

// Nombre de sommets éditables (anneau extérieur), 0 pour une entité sans
// sommet. Les trous ne sont pas éditables depuis la palette.
inline std::size_t geometryVertexCount(const geom::Entity& e) {
    const auto* poly = dynamic_cast<const geom::PolylineEntity*>(&e);
    return poly ? poly->vertices().size() : 0;
}

// Valeur affichée d'un champ. Nullopt : champ sans valeur pour cette entité
// (type erroné, sommet hors de la polyligne, aire d'une polyligne ouverte).
inline std::optional<double> geometryFieldValue(const geom::Entity& e, GeometryField field, int vertex = 0) {
    switch (field) {
        case GeometryField::StartX:
        case GeometryField::StartY: {
            if (e.typeId() != geom::TypeId_Line) return std::nullopt;
            const auto& l = static_cast<const geom::LineEntity&>(e);
            return field == GeometryField::StartX ? l.start().x_ : l.start().y_;
        }
        case GeometryField::EndX:
        case GeometryField::EndY: {
            if (e.typeId() != geom::TypeId_Line) return std::nullopt;
            const auto& l = static_cast<const geom::LineEntity&>(e);
            return field == GeometryField::EndX ? l.end().x_ : l.end().y_;
        }
        case GeometryField::CenterX:
        case GeometryField::CenterY: {
            std::optional<geom::Point2> center;
            if (e.typeId() == geom::TypeId_Circle) center = static_cast<const geom::CircleEntity&>(e).center();
            else if (e.typeId() == geom::TypeId_Arc) center = static_cast<const geom::ArcEntity&>(e).center();
            if (!center) return std::nullopt;
            return field == GeometryField::CenterX ? center->x_ : center->y_;
        }
        case GeometryField::Radius: {
            if (e.typeId() == geom::TypeId_Circle) return static_cast<const geom::CircleEntity&>(e).radius();
            if (e.typeId() == geom::TypeId_Arc) return static_cast<const geom::ArcEntity&>(e).radius();
            return std::nullopt;
        }
        case GeometryField::StartAngle:
        case GeometryField::EndAngle: {
            if (e.typeId() != geom::TypeId_Arc) return std::nullopt;
            const auto& a = static_cast<const geom::ArcEntity&>(e);
            return geom::toDegrees(field == GeometryField::StartAngle ? a.startAngle() : a.endAngle());
        }
        case GeometryField::PositionX:
        case GeometryField::PositionY: {
            if (e.typeId() != geom::TypeId_Point) return std::nullopt;
            const auto& p = static_cast<const geom::PointEntity&>(e);
            return field == GeometryField::PositionX ? p.position().x_ : p.position().y_;
        }
        case GeometryField::VertexX:
        case GeometryField::VertexY: {
            const auto* poly = dynamic_cast<const geom::PolylineEntity*>(&e);
            if (!poly || vertex < 0 || static_cast<std::size_t>(vertex) >= poly->vertices().size())
                return std::nullopt;
            const geom::Point2& v = poly->vertices()[static_cast<std::size_t>(vertex)];
            return field == GeometryField::VertexX ? v.x_ : v.y_;
        }
        case GeometryField::Length: {
            if (e.typeId() == geom::TypeId_Line) return static_cast<const geom::LineEntity&>(e).length();
            if (const auto* poly = dynamic_cast<const geom::PolylineEntity*>(&e)) return poly->length();
            return std::nullopt;
        }
        case GeometryField::ArcLength: {
            if (e.typeId() != geom::TypeId_Arc) return std::nullopt;
            const auto& a = static_cast<const geom::ArcEntity&>(e);
            return a.radius() * a.sweep();
        }
        case GeometryField::Diameter: {
            if (e.typeId() != geom::TypeId_Circle) return std::nullopt;
            return 2.0 * static_cast<const geom::CircleEntity&>(e).radius();
        }
        case GeometryField::Area: {
            if (e.typeId() == geom::TypeId_Circle) {
                const double r = static_cast<const geom::CircleEntity&>(e).radius();
                return std::numbers::pi * r * r;
            }
            const auto* poly = dynamic_cast<const geom::PolylineEntity*>(&e);
            if (!poly || !poly->closed()) return std::nullopt;
            return std::abs(geom::polygonArea(*poly));
        }
    }
    return std::nullopt;
}

// Lecture d'une saisie : « 12,5 » et « 12.5 » valent la même chose, les
// espaces de bord sont tolérés. Nullopt pour le vide, « 12a », « nan »,
// « inf » — jamais de troncature silencieuse de ce qui reste non lu.
inline std::optional<double> parseFieldValue(const std::string& textIn) {
    const std::size_t first = textIn.find_first_not_of(" \t");
    if (first == std::string::npos) return std::nullopt;
    const std::size_t last = textIn.find_last_not_of(" \t");
    std::string text = textIn.substr(first, last - first + 1);
    for (char& c : text)
        if (c == ',') c = '.';
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size() || !std::isfinite(value)) return std::nullopt;
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

// Forme affichée d'une valeur : douze chiffres significatifs, notation
// générale, séparateur indépendant de la langue (un champ ne montre jamais
// « 1,2e+03 »). Le miroir de -0 est « 0 » : la saisie doit retomber sur la
// même forme que la lecture pour ne pas pousser de pas d'annulation vide.
inline std::string formatFieldValue(double value) {
    if (value == 0.0) return "0";
    std::ostringstream ss;
    ss << std::defaultfloat << std::setprecision(12) << value;
    return ss.str();
}

// La saisie change-t-elle la valeur déjà affichée ? Comparée sur leur forme
// affichée : retaper ce qui est déjà là (ou l'arrondir à la virgule près) ne
// pousse rien dans l'historique.
inline bool sameFieldValue(double current, double typed) {
    return formatFieldValue(current) == formatFieldValue(typed);
}

// Clone de `e` avec le champ `field` pris à `value`, ou nulle si l'écriture
// est refusée — `reject` dit alors pourquoi (None reste inchangé à succès).
// `vertex` désigne le sommet pour VertexX/VertexY, ignoré ailleurs.
inline std::unique_ptr<geom::Entity> withGeometryField(const geom::Entity& e, GeometryField field, int vertex,
                                                        double value, GeometryEditReject* reject = nullptr) {
    auto refuse = [reject](GeometryEditReject reason) -> std::unique_ptr<geom::Entity> {
        if (reject) *reject = reason;
        return nullptr;
    };
    if (reject) *reject = GeometryEditReject::None;
    if (isReadOnlyGeometryField(field)) return refuse(GeometryEditReject::ReadOnly);
    if (!std::isfinite(value)) return refuse(GeometryEditReject::NotFinite);

    if (field == GeometryField::StartX || field == GeometryField::StartY || field == GeometryField::EndX ||
        field == GeometryField::EndY) {
        if (e.typeId() != geom::TypeId_Line) return refuse(GeometryEditReject::NotApplicable);
        std::unique_ptr<geom::Entity> out = e.clone();
        auto& line = static_cast<geom::LineEntity&>(*out);
        geom::Point2 start = line.start(), end = line.end();
        if (field == GeometryField::StartX) start.x_ = value;
        else if (field == GeometryField::StartY) start.y_ = value;
        else if (field == GeometryField::EndX) end.x_ = value;
        else end.y_ = value;
        if (geom::distance(start, end) < geom::Tolerance::kDegenerateLength)
            return refuse(GeometryEditReject::Degenerate);
        line.setStart(start);
        line.setEnd(end);
        return out;
    }

    if (field == GeometryField::CenterX || field == GeometryField::CenterY || field == GeometryField::Radius) {
        const bool isCircle = e.typeId() == geom::TypeId_Circle;
        const bool isArc = e.typeId() == geom::TypeId_Arc;
        if (!isCircle && !isArc) return refuse(GeometryEditReject::NotApplicable);
        geom::Point2 center;
        double radius = 0.0;
        if (isCircle) {
            const auto& c = static_cast<const geom::CircleEntity&>(e);
            center = c.center();
            radius = c.radius();
        } else {
            const auto& a = static_cast<const geom::ArcEntity&>(e);
            center = a.center();
            radius = a.radius();
        }
        if (field == GeometryField::CenterX) center.x_ = value;
        else if (field == GeometryField::CenterY) center.y_ = value;
        else radius = value;
        if (radius <= 0.0) return refuse(GeometryEditReject::RadiusNotPositive);
        std::unique_ptr<geom::Entity> out = e.clone();
        if (isCircle) {
            auto& c = static_cast<geom::CircleEntity&>(*out);
            c.setCenter(center);
            c.setRadius(radius);
        } else {
            auto& a = static_cast<geom::ArcEntity&>(*out);
            a.setCenter(center);
            a.setRadius(radius);
        }
        return out;
    }

    if (field == GeometryField::StartAngle || field == GeometryField::EndAngle) {
        if (e.typeId() != geom::TypeId_Arc) return refuse(GeometryEditReject::NotApplicable);
        std::unique_ptr<geom::Entity> out = e.clone();
        auto& arc = static_cast<geom::ArcEntity&>(*out);
        // Conservé tel quel, sans normalisation : afficher 450° puis retaper
        // 450° doit retomber sur 450°, pas décaler d'un tour.
        if (field == GeometryField::StartAngle) arc.setStartAngle(geom::toRadians(value));
        else arc.setEndAngle(geom::toRadians(value));
        return out;
    }

    if (field == GeometryField::PositionX || field == GeometryField::PositionY) {
        if (e.typeId() != geom::TypeId_Point) return refuse(GeometryEditReject::NotApplicable);
        std::unique_ptr<geom::Entity> out = e.clone();
        auto& point = static_cast<geom::PointEntity&>(*out);
        geom::Point2 position = point.position();
        if (field == GeometryField::PositionX) position.x_ = value;
        else position.y_ = value;
        point.setPosition(position);
        return out;
    }

    if (field == GeometryField::VertexX || field == GeometryField::VertexY) {
        const auto* poly = dynamic_cast<const geom::PolylineEntity*>(&e);
        if (!poly) return refuse(GeometryEditReject::NotApplicable);
        if (vertex < 0 || static_cast<std::size_t>(vertex) >= poly->vertices().size())
            return refuse(GeometryEditReject::InvalidVertex);
        std::unique_ptr<geom::Entity> out = e.clone();
        auto& copy = static_cast<geom::PolylineEntity&>(*out);
        geom::Point2 v = copy.vertices()[static_cast<std::size_t>(vertex)];
        if (field == GeometryField::VertexX) v.x_ = value;
        else v.y_ = value;
        copy.vertices()[static_cast<std::size_t>(vertex)] = v;
        return out;
    }

    return refuse(GeometryEditReject::NotApplicable);
}

} // namespace bcad::app::detail
