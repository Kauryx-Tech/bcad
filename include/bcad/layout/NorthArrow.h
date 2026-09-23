#pragma once

#include "bcad/geometry/Point.h"
#include <cmath>
#include <vector>

namespace bcad::layout {

// Flèche Nord (H5) — position sur feuille (mm), angle 0 = Nord
struct NorthArrow {
    bcad::geom::Point2 position{10, 10}; // mm depuis coin sup-gauche zone imprimable
    double sizeMm = 15.0;
    double angleDeg = 0; // 0 = Nord en haut

    // Sommets du triangle Nord (3 points)
    std::vector<bcad::geom::Point2> triangle() const {
        double r = sizeMm * 0.5;
        double rad = angleDeg * 3.14159265358979 / 180.0;
        double c = std::cos(rad), s = std::sin(rad);
        auto rot = [&](double x, double y) -> bcad::geom::Point2 {
            return {position.x_ + x * c - y * s, position.y_ + x * s + y * c};
        };
        return {rot(0, -r), rot(-r * 0.5, r * 0.5), rot(r * 0.5, r * 0.5)};
    }
};

} // namespace bcad::layout
