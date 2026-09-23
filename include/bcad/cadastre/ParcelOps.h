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

// Aire en m² (via BooleanOps polygonArea)
double parcelArea(const bcad::geom::PolylineEntity& parcel);
std::string formatContenance(double areaM2); // ex: "500 m²", "1.2 ha"

// Divise une parcelle en N lots égaux par des lignes parallèles perpendiculaires
// à la direction donnée (directionLine = segment définissant la direction de coupe).
// Retourne un vecteur de N parcelles si succès.
std::optional<std::vector<bcad::geom::PolylineEntity>>
subdivideParcel(const bcad::geom::PolylineEntity& parcel, int n,
                const bcad::geom::PolylineEntity& directionLine);

} // namespace bcad::cadastre
