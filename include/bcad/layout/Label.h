#pragma once

// Centroïde polygone : formule standard Cx = Σ(xi+xi+1)·cross / (6A).
// Vérifié : carré 10×10 → (5,5) ; triangle (0,0)(6,0)(3,6) → x=3.

#include "bcad/geometry/Point.h"
#include <cmath>
#include <string>
#include <vector>

namespace bcad::layout {

// Étiquette parcelle (H3) : numéro + contenance au centre du polygone
struct Label {
    std::string text;
    bcad::geom::Point2 position;
    double fontSizeMm = 3.0; // taille en mm sur feuille

    static Label forParcel(const std::string& section, const std::string& numero,
                           const std::string& contenance,
                           const std::vector<bcad::geom::Point2>& vertices) {
        // Centroïde du polygone
        double cx = 0, cy = 0;
        double area = 0;
        size_t n = vertices.size();
        for (size_t i = 0; i < n; ++i) {
            auto& p = vertices[i];
            auto& q = vertices[(i + 1) % n];
            double cross = p.x_ * q.y_ - q.x_ * p.y_;
            area += cross;
            cx += (p.x_ + q.x_) * cross;
            cy += (p.y_ + q.y_) * cross;
        }
        area *= 0.5;
        if (std::abs(area) > 1e-9) {
            cx /= (6 * area);
            cy /= (6 * area);
        } else if (!vertices.empty()) {
            // fallback : moyenne des sommets
            cx = 0; cy = 0;
            for (auto& v : vertices) { cx += v.x_; cy += v.y_; }
            cx /= n; cy /= n;
        }

        std::string text = section + " " + numero;
        if (!contenance.empty()) text += "\n" + contenance;

        return Label{text, {cx, cy}};
    }
};

} // namespace bcad::layout
