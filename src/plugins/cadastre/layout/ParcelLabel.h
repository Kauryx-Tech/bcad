#pragma once

// Étiquette de parcelle (H3) : « section numéro » et contenance au centroïde du
// polygone. Vocabulaire cadastral, donc dans le module et non dans
// include/bcad/layout, où ne reste que la forme générique `layout::Label`.
//
// Centroïde polygone : formule standard Cx = Σ(xi+xi+1)·cross / (6A).
// Vérifié : carré 10×10 → (5,5) ; triangle (0,0)(6,0)(3,6) → x=3.

#include "bcad/geometry/Point.h"
#include "bcad/layout/Label.h"

#include <cmath>
#include <string>
#include <vector>

namespace bcad::cadastre {

inline layout::Label parcelLabel(const std::string& section, const std::string& numero,
                                 const std::string& contenance,
                                 const std::vector<geom::Point2>& vertices) {
    double cx = 0, cy = 0;
    double area = 0;
    const size_t n = vertices.size();
    for (size_t i = 0; i < n; ++i) {
        const auto& p = vertices[i];
        const auto& q = vertices[(i + 1) % n];
        const double cross = p.x_ * q.y_ - q.x_ * p.y_;
        area += cross;
        cx += (p.x_ + q.x_) * cross;
        cy += (p.y_ + q.y_) * cross;
    }
    area *= 0.5;
    if (std::abs(area) > 1e-9) {
        cx /= (6 * area);
        cy /= (6 * area);
    } else if (!vertices.empty()) {
        // Polygone dégénéré : moyenne des sommets.
        cx = 0; cy = 0;
        for (const auto& v : vertices) { cx += v.x_; cy += v.y_; }
        cx /= n; cy /= n;
    }

    std::string text = section + " " + numero;
    if (!contenance.empty()) text += "\n" + contenance;

    return layout::Label{text, {cx, cy}};
}

} // namespace bcad::cadastre
