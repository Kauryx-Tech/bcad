#pragma once

// Cotations : hypot() par côté (3-4-5 → 5 vérifié), angles via atan2.
// Cf. OASL Ch. A (Inverse Computation) pour la convention Nord/Est.

#include "bcad/geometry/Point.h"
#include <cmath>
#include <string>
#include <vector>

namespace bcad::layout {

// Cotation linéaire (H1) : distance entre deux bornes
struct Dimension {
    bcad::geom::Point2 from, to;
    double value;           // distance en m
    bcad::geom::Point2 mid; // milieu pour placement texte

    std::string text() const {
        char buf[32];
        if (value >= 1000) snprintf(buf, sizeof(buf), "%.2f m", value);
        else if (value >= 1) snprintf(buf, sizeof(buf), "%.2f m", value);
        else snprintf(buf, sizeof(buf), "%.0f mm", value * 1000);
        return buf;
    }

    static std::vector<Dimension> forPolyline(const std::vector<bcad::geom::Point2>& vertices) {
        std::vector<Dimension> dims;
        size_t n = vertices.size();
        for (size_t i = 0; i < n; ++i) {
            auto& a = vertices[i];
            auto& b = vertices[(i + 1) % n];
            double dx = b.x_ - a.x_, dy = b.y_ - a.y_;
            double d = std::hypot(dx, dy);
            bcad::geom::Point2 mid{(a.x_ + b.x_) * 0.5, (a.y_ + b.y_) * 0.5};
            dims.push_back({a, b, d, mid});
        }
        return dims;
    }
};

} // namespace bcad::layout
