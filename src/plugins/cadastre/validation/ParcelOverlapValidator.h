#pragma once

#include "bcad/geometry/Entity.h"
#include <string>
#include <vector>

namespace bcad::cadastre {

class ParcelOverlapValidator {
public:
    struct Result {
        bool overlap = false;
        std::string details;
    };

    Result check(const geom::Entity& a, const geom::Entity& b) const;
    std::vector<Result> checkAll(const std::vector<geom::Entity*>& entities) const;
};

} // namespace bcad::cadastre
