#pragma once

#include "bcad/geometry/Entity.h"
#include <memory>
#include <string>

namespace bcad::cadastre {

class GeoJsonSerializer {
public:
    std::string serialize(const geom::Entity& entity) const;
    std::unique_ptr<geom::Entity> deserialize(const std::string& json) const;
};

} // namespace bcad::cadastre
