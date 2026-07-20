#pragma once

#include "bcad/geometry/Types.h"
#include <array>
#include <cmath>

namespace bcad::io {

// AutoCAD Color Index : la palette héritée (pré-truecolor) de DXF. On n'en a
// besoin que pour faire un aller-retour raisonnable de nos propres couleurs
// de calque ; une fidélité exacte avec la palette complète à 255 entrées
// d'AutoCAD n'est pas un objectif.
inline geom::Color aciToRgb(int aci) {
    static constexpr std::array<std::array<int, 3>, 8> kAci = {{
        {0, 0, 0},       // 0 inutilisé
        {255, 0, 0},     // 1 rouge
        {255, 255, 0},   // 2 jaune
        {0, 255, 0},     // 3 vert
        {0, 255, 255},   // 4 cyan
        {0, 0, 255},     // 5 bleu
        {255, 0, 255},   // 6 magenta
        {255, 255, 255}, // 7 blanc/noir
    }};
    if (aci < 0 || aci > 7) aci = 7;
    const auto& c = kAci[aci];
    return geom::Color::fromRgb255(c[0], c[1], c[2]);
}

inline int rgbToAci(const geom::Color& c) {
    static constexpr std::array<std::array<float, 3>, 8> kAci = {{
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 1, 1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1},
    }};
    int best = 7;
    float bestDist = 1e9f;
    for (int i = 1; i <= 7; ++i) {
        float dr = c.r - kAci[i][0], dg = c.g - kAci[i][1], db = c.b - kAci[i][2];
        float d = dr * dr + dg * dg + db * db;
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

} // namespace bcad::io
