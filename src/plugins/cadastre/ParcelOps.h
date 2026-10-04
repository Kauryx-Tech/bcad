#pragma once

#include "bcad/geometry/Polyline.h"
#include <optional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bcad::cadastre {

std::optional<std::pair<geom::PolylineEntity, geom::PolylineEntity>>
splitParcel(const geom::PolylineEntity& parcel, const geom::PolylineEntity& cutLine);

std::optional<std::vector<geom::PolylineEntity>>
subdivideParcel(const geom::PolylineEntity& parcel, int n,
                const geom::PolylineEntity& directionLine);

std::optional<geom::PolylineEntity>
mergeParcels(const geom::PolylineEntity& first, const geom::PolylineEntity& second);

double parcelArea(const geom::PolylineEntity& parcel);

std::string formatContenance(double areaM2);

// Contenance en m² avec deux decimales, a la francaise : « 1 250,50 m² ».
std::string formatSquareMetres(double areaM2);

// Contenance ecrite dans un acte, en m² : « 1 250,50 m² », « 1250.5 m2 »,
// « 2,35 ha », « 2 ha 3 a 50 ca », « 12 a 50 ca », ou un nombre seul (m²).
// Vide si le texte ne se lit pas : une valeur devinee vaudrait pire que rien.
std::optional<double> parseContenance(std::string_view text);

} // namespace bcad::cadastre
