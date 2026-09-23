#pragma once

#include "bcad/geometry/Point.h"
#include <cmath>
#include <string>
#include <vector>

namespace bcad::layout {

struct AngleDimension {
    bcad::geom::Point2 vertex, prev, next;
    double angleDeg;

    std::string text() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f°", angleDeg);
        return buf;
    }

    static std::vector<AngleDimension> forPolyline(const std::vector<bcad::geom::Point2>& v) {
        std::vector<AngleDimension> dims;
        size_t n = v.size();
        for (size_t i = 0; i < n; ++i) {
            auto& p = v[i];
            auto& a = v[(i + n - 1) % n];
            auto& b = v[(i + 1) % n];
            double v1x = a.x_ - p.x_, v1y = a.y_ - p.y_;
            double v2x = b.x_ - p.x_, v2y = b.y_ - p.y_;
            double dot = v1x * v2x + v1y * v2y;
            double det = v1x * v2y - v1y * v2x;
            double ang = std::atan2(std::abs(det), dot) * 180.0 / 3.14159265358979;
            dims.push_back({p, a, b, ang});
        }
        return dims;
    }
};

} // namespace bcad::layout
