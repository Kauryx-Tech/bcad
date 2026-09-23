#pragma once

#include "bcad/geometry/Polyline.h"
#include <optional>
#include <vector>

namespace bcad::cadastre {

// Découpe une parcelle (polygone fermé) par une polyligne de coupe.
// Retourne 2 polygones si la coupe traverse, sinon std::nullopt.
std::optional<std::pair<bcad::geom::PolylineEntity, bcad::geom::PolylineEntity>>
splitParcel(const bcad::geom::PolylineEntity& parcel,
            const bcad::geom::PolylineEntity& cutLine);

// Fusion de 2 parcelles adjacentes (union booléenne).
std::optional<bcad::geom::PolylineEntity>
mergeParcels(const bcad::geom::PolylineEntity& a,
             const bcad::geom::PolylineEntity& b);

} // namespace bcad::cadastre
