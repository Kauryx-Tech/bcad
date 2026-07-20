#pragma once

#include <algorithm>
#include <cmath>

namespace bcad::render {

// Espacement de grille adaptatif : choisit un pas "propre" en espace monde
// (1/2/5 × 10^n) afin que, au zoom courant, l'espacement à l'écran reste
// proche de targetPx. Technique standard — évite que la grille ne devienne
// soit une tache uniforme en dézoomant, soit invisible en zoomant, sans
// devoir la redessiner à chaque cran de zoom.
inline double adaptiveGridSpacing(double pixelsPerUnit, double targetPx = 40.0) {
    double ppu = std::max(pixelsPerUnit, 1e-9);
    double worldTarget = targetPx / ppu;
    double magnitude = std::pow(10.0, std::floor(std::log10(worldTarget)));
    double residual = worldTarget / magnitude; // dans [1, 10)

    double niceResidual;
    if (residual < 1.5) niceResidual = 1.0;
    else if (residual < 3.5) niceResidual = 2.0;
    else if (residual < 7.5) niceResidual = 5.0;
    else niceResidual = 10.0;

    return niceResidual * magnitude;
}

inline double snapToGrid(double value, double spacing) {
    if (spacing <= 0.0) return value;
    return std::round(value / spacing) * spacing;
}

} // namespace bcad::render
