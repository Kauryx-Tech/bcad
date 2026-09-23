#pragma once

#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/BooleanOps.h"
#include <utility>
#include <vector>

namespace bcad::cadastre {

struct Overlap {
    size_t i, j;
    double area;
};

// Détecte les recouvrements entre parcelles (F5)
inline std::vector<Overlap> findOverlaps(const std::vector<bcad::geom::PolylineEntity>& parcels) {
    std::vector<Overlap> result;
    for (size_t i = 0; i < parcels.size(); ++i)
        for (size_t j = i + 1; j < parcels.size(); ++j) {
            auto inter = bcad::geom::booleanOp(parcels[i], parcels[j], bcad::geom::BooleanOp::Intersection);
            if (!inter.empty()) {
                double area = 0;
                for (auto& p : inter) area += std::abs(bcad::geom::polygonArea(p));
                if (area > 1e-9) result.push_back({i, j, area});
            }
        }
    return result;
}

} // namespace bcad::cadastre
