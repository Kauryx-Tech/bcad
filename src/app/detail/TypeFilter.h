#pragma once

// En-tete interne a src/app : il n'est pas installe et n'entre pas dans le SDK.
// Filtre generique sur les TypeIds declares par un module : l'hote compare des
// chaines, il ne sait pas ce qu'elles designent (ADR-016).

#include "bcad/geometry/Entity.h"

#include <string>
#include <vector>

namespace bcad::app {

inline bool typeMatches(const geom::Entity& entity,
                        const std::vector<std::string>& acceptedTypes) {
    if (acceptedTypes.empty()) return true;
    for (const auto& type : acceptedTypes) {
        if (entity.typeId().value == type) return true;
    }
    return false;
}

} // namespace bcad::app
