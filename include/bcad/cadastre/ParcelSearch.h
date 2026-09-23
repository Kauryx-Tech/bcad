#pragma once

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include <string>
#include <vector>

namespace bcad::cadastre {

// Recherche par référence cadastrale (F4)
inline std::vector<bcad::geom::Entity*> findByRef(bcad::core::Document& doc,
                                                   const std::string& section,
                                                   const std::string& numero) {
    std::string target = section + "|" + numero;
    std::vector<bcad::geom::Entity*> result;
    for (auto& e : doc.entities()) {
        if (e->typeId().value != "cadastre.parcel") continue;
        // Parcelle sérialisée contient section|numero — on peut chercher via properties si dispo
        // Fallback : via serializeParams
        std::string params = e->serializeParams();
        if (params.find(target) != std::string::npos) result.push_back(e.get());
    }
    return result;
}

inline bcad::geom::Entity* findOneByRef(bcad::core::Document& doc,
                                         const std::string& section,
                                         const std::string& numero) {
    auto v = findByRef(doc, section, numero);
    return v.empty() ? nullptr : v[0];
}

} // namespace bcad::cadastre
