#pragma once

#include "bcad/core/Document.h"
#include <optional>

namespace bcad::app {

// La grille est gérée séparément par Viewport (ce n'est pas de la géométrie
// du document), mais figure dans la même énumération pour que l'indicateur
// de snap puisse la dessiner comme n'importe quel autre résultat de snap.
enum class SnapType { None, Endpoint, Midpoint, Center, Intersection, Quadrant, Perpendicular, Nearest, Grid };

struct SnapResult {
    SnapType type = SnapType::None;
    geom::Point2 point{ 0, 0 };

    explicit operator bool() const { return type != SnapType::None; }
};

// Accrochage aux objets (object snapping) : trouve le point géométriquement
// "intéressant" le plus proche du curseur (extrémité, milieu, centre,
// intersection) dans une tolérance en coordonnées monde, plus la
// perpendiculaire-depuis-référence quand un point de référence (le dernier
// point placé par l'outil) est fourni. Endpoint/Center/Intersection sont un
// niveau de priorité au-dessus de Midpoint — cela correspond au ressenti CAO
// habituel où les points "durs" accrochent plus fort qu'un point milieu
// même quand celui-ci est un cheveu plus proche. La perpendiculaire ne
// s'active que si rien d'autre ne qualifie, car elle a besoin d'un point de
// référence explicite pour avoir un sens.
class SnapEngine {
public:
    SnapResult findSnap(const core::Document& doc, const geom::Point2& cursor, double worldTolerance,
                         std::optional<geom::Point2> referencePoint = std::nullopt) const;
};

} // namespace bcad::app
