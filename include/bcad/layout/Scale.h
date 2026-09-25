#pragma once

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace bcad::layout {

// Échelles cadastrales FR (G3).
// Source : BOFiP DGFiP BOI-CAD-DIFF-10 (1/5000→1/500, +1/8000, 1/250 rares),
// FranceArchives (rénové : 1/1000, 1/1250, 1/2000, 1/2500, 1/500 dense).
inline const std::vector<int> kStandardScales = {100, 200, 250, 500, 1000, 1250, 2000, 2500, 5000, 8000, 10000};

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

// Barre d'échelle graduée (H2). La longueur représentée est une distance
// terrain « ronde » (1, 2 ou 5 × 10^n) : chaque graduation tombe aussi sur une
// valeur ronde, sinon la barre n'est pas lisible.
struct ScaleBar {
    double groundMeters = 0;
    double lengthMm = 0;
    int segments = 1;
    double segmentMeters = 0;

    std::string label() const {
        double value = groundMeters;
        const char* unit = "m";
        if (value >= 1000) { value /= 1000.0; unit = "km"; }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", value);
        std::string number(buf);
        while (number.back() == '0') number.pop_back();
        if (number.back() == '.') number.pop_back();
        for (char& c : number) if (c == '.') c = ',';
        return number + " " + unit;
    }
};

// Plus grande distance ronde dont la barre tient dans maxMm sur la feuille.
inline ScaleBar makeScaleBar(double scale, double maxMm = 60.0) {
    ScaleBar bar;
    double bestMeters = 0;
    int bestSegments = 1;
    for (int exponent = -2; exponent <= 6; ++exponent) {
        const double base = std::pow(10.0, exponent);
        for (const auto& [mantissa, segments] :
             std::vector<std::pair<double, int>>{{1, 5}, {2, 4}, {5, 5}}) {
            const double meters = mantissa * base;
            if (meters * 1000.0 / scale > maxMm) continue;
            if (meters <= bestMeters) continue;
            bestMeters = meters;
            bestSegments = segments;
        }
    }
    if (bestMeters <= 0) return bar;
    bar.groundMeters = bestMeters;
    bar.lengthMm = bestMeters * 1000.0 / scale;
    bar.segments = bestSegments;
    bar.segmentMeters = bestMeters / bestSegments;
    return bar;
}

} // namespace bcad::layout
