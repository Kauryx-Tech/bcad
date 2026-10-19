// Reglages du pointeur (S-11) : valeurs par defaut d'AutoCAD, bornes, longueur
// des traits du reticule. Calculs purs de detail/CursorStyle.h.

#include "detail/CursorStyle.h"

#include <cmath>
#include <cstdio>

using bcad::app::detail::CursorStyle;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAILED: %s\n", what);
        ++failures;
    }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
} // namespace

int main() {
    const CursorStyle def;
    check(def.crosshairPercent == 5 && !def.systemPointer && !def.crosshairRgb, "par defaut : la croix d'AutoCAD, 5 %");
    check(near(def.crosshairHalfLength(1000, 600), 25), "5 % de 1000 px : traits de 50 px");

    CursorStyle full;
    full.crosshairPercent = 100;
    check(near(full.crosshairHalfLength(1000, 600), 1000), "100 % : les traits traversent le canevas");

    CursorStyle wild;
    wild.crosshairPercent = 400;
    wild.pickboxPx = -3;
    const CursorStyle c = wild.clamped();
    check(c.crosshairPercent == CursorStyle::kCrosshairMax && c.pickboxPx == CursorStyle::kPickboxMin,
          "valeurs hors bornes ramenees");
    check(near(wild.crosshairHalfLength(800, 800), 800), "longueur bornee meme sans clamped()");

    if (failures) return 1;
    std::printf("Pointeur : tests PASSED\n");
    return 0;
}
