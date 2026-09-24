#pragma once

#include "bcad/geometry/Entity.h"
#include <string>
#include <vector>

namespace bcad::cadastre {

class CsvCoordinateExporter {
public:
    std::string exportCoordinates(const std::vector<geom::Entity*>& entities) const;
};

} // namespace bcad::cadastre
