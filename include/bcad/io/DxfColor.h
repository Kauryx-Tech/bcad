#pragma once

#include "bcad/geometry/Types.h"
#include <array>
#include <cmath>

namespace bcad::io {

// AutoCAD Color Index: DXF's legacy (pre-truecolor) palette. We only need
// enough of it to round-trip our own layer colors reasonably; exact fidelity
// with AutoCAD's full 255-entry palette is not a goal.
inline geom::Color aciToRgb(int aci) {
    static constexpr std::array<std::array<int, 3>, 8> kAci = {{
        {0, 0, 0},       // 0 unused
        {255, 0, 0},     // 1 red
        {255, 255, 0},   // 2 yellow
        {0, 255, 0},     // 3 green
        {0, 255, 255},   // 4 cyan
        {0, 0, 255},     // 5 blue
        {255, 0, 255},   // 6 magenta
        {255, 255, 255}, // 7 white/black
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
