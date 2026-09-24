#pragma once

#include "bcad/geometry/Polyline.h"
#include <optional>
#include <string>
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

} // namespace bcad::cadastre
