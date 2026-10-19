// Saisie directe de distance (AutoCAD) : un nombre seul apres un premier
// point, dans la direction du curseur. Calcul pur de CoordinateInput.h.

#include "detail/CoordinateInput.h"

#include <cmath>
#include <cstdio>

using bcad::app::parseDirectDistance;
using bcad::geom::Point2;

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
    const Point2 c(10, 10);
    auto p = parseDirectDistance("5", c, Point2(10, 30));
    check(p && near(p->x_, 10) && near(p->y_, 15), "vers le curseur, a la distance tapee");
    p = parseDirectDistance(" 2.5 ", c, Point2(13, 14));
    check(p && near(p->x_, 11.5) && near(p->y_, 12), "direction quelconque, espaces toleres");
    p = parseDirectDistance("5", c, c);
    check(p && near(p->x_, 15) && near(p->y_, 10), "curseur sur le point : vers +X");
    p = parseDirectDistance("5", c, std::nullopt);
    check(p && near(p->x_, 15), "sans curseur : vers +X");
    p = parseDirectDistance("-4", c, Point2(20, 10));
    check(p && near(p->x_, 6), "negatif : a l'oppose du curseur");
    check(!parseDirectDistance("5", std::nullopt, Point2(1, 1)), "sans premier point : rien");
    check(!parseDirectDistance("0", c, Point2(1, 1)), "zero : rien");
    check(!parseDirectDistance("5,3", c, Point2(1, 1)), "une coordonnee n'est pas une distance");
    check(!parseDirectDistance("5abc", c, Point2(1, 1)), "texte en trop refuse");
    check(!parseDirectDistance("D", c, Point2(1, 1)), "une option n'est pas une distance");

    if (failures) return 1;
    std::printf("Saisie directe de distance : tests PASSED\n");
    return 0;
}
