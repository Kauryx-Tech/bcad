#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace bcad::layout {

// Échelles cadastrales standard (G3)
inline const std::vector<int> kStandardScales = {100, 200, 250, 500, 1000, 2000, 5000, 10000};

inline std::string scaleText(int scale) {
    return "1:" + std::to_string(scale);
}

inline int nearestStandardScale(double rawScale) {
    for (int s : kStandardScales) if (rawScale <= s) return s;
    return kStandardScales.back();
}

// Carroyage : pas de grille en mm sur feuille pour une échelle donnée
inline double gridStepMm(int scale) {
    // 10m terrain → X mm sur feuille
    if (scale <= 500) return 20;   // 10m = 20mm à 1:500
    if (scale <= 1000) return 10;  // 10m = 10mm à 1:1000
    if (scale <= 2000) return 5;
    return 2;
}

} // namespace bcad::layout
