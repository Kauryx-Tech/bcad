#pragma once

#include "bcad/geometry/Point.h"
#include <string>
#include <vector>

namespace bcad::layout {

// Borne cadastrale (H4) : point + numéro
struct Borne {
    bcad::geom::Point2 position;
    std::string numero; // ex: "B1", "B2"
    double radiusMm = 1.5;

    static std::vector<Borne> forParcel(const std::vector<bcad::geom::Point2>& vertices) {
        std::vector<Borne> bornes;
        for (size_t i = 0; i < vertices.size(); ++i) {
            bornes.push_back({vertices[i], "B" + std::to_string(i + 1)});
        }
        return bornes;
    }
};

} // namespace bcad::layout
