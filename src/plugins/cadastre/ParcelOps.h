#pragma once

#include "bcad/geometry/Polyline.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bcad::cadastre {

std::optional<std::pair<geom::PolylineEntity, geom::PolylineEntity>>
splitParcel(const geom::PolylineEntity& parcel, const geom::PolylineEntity& cutLine);

std::optional<geom::PolylineEntity>
mergeParcels(const geom::PolylineEntity& first, const geom::PolylineEntity& second);

} // namespace bcad::cadastre
