#pragma once

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace bcad::layout {

// Échelles de la feuille : la liste des échelles admises est une donnée du
// profil national du module, pas une constante de l'hôte (ADR-017). L'hôte ne
// fait qu'y choisir : la première qui tient, ou l'ajustement exact si le
// profil n'en donne aucune — une feuille sans liste n'est pas une feuille sans
// échelle, c'est une feuille où l'opérateur choisit au plus juste.
inline int nearestPermittedScale(double rawScale, const std::vector<int>& permitted) {
    if (permitted.empty()) return std::max(1, static_cast<int>(std::ceil(rawScale)));
    for (int s : permitted)
        if (rawScale <= s) return s;
    return permitted.back();
}

inline std::string scaleText(int scale) {
    return "1:" + std::to_string(scale);
}

// Carroyage : supprimé avec les échelles FR — `gridStepMm` n'avait aucun
// appelant dans `src/`, et le pas d'une grille nationale est une donnée du
// profil du module, pas une constante de l'hôte (ADR-017).

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
