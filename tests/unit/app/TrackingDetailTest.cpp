// Reperages du curseur : polaire (F10) et d'objet (F11), calculs purs de
// TrackingDetail.h.

#include "TrackingDetail.h"

#include <cmath>
#include <cstdio>
#include <vector>

using bcad::app::detail::polarTrack;
using bcad::app::detail::trackAlign;
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
    // --- Polaire ---
    const Point2 o(0, 0);
    auto h = polarTrack(o, Point2(10, 0.5), 90.0, 1.0);
    check(h && near(h->point.x_, 10) && near(h->point.y_, 0) && near(h->angleDeg, 0), "polaire : ramene sur 0°");
    h = polarTrack(o, Point2(7, 7.3), 45.0, 1.0);
    check(h && near(h->angleDeg, 45) && near(h->point.x_, h->point.y_), "polaire : ramene sur 45°");
    check(!polarTrack(o, Point2(10, 3), 90.0, 1.0), "polaire : trop loin de la direction");
    check(!polarTrack(o, Point2(0.5, 0), 90.0, 1.0), "polaire : trop pres de la reference");
    h = polarTrack(o, Point2(10, -0.2), 90.0, 1.0);
    check(h && near(h->angleDeg, 0), "polaire : 359,x° donne 0°, pas 360°");

    // --- Repérage d'objet ---
    const std::vector<Point2> none;
    check(!trackAlign(none, Point2(1, 1), 1.0), "objet : aucun point acquis");

    const std::vector<Point2> one{Point2(5, 5)};
    auto a = trackAlign(one, Point2(20, 5.4), 1.0);
    check(a && near(a->point.x_, 20) && near(a->point.y_, 5) && a->horizontalFrom && !a->verticalFrom,
          "objet : horizontale d'un point acquis");
    a = trackAlign(one, Point2(5.3, -8), 1.0);
    check(a && near(a->point.x_, 5) && near(a->point.y_, -8) && a->verticalFrom && !a->horizontalFrom,
          "objet : verticale d'un point acquis");
    check(!trackAlign(one, Point2(9, 9), 1.0), "objet : hors tolerance");

    // Croisement : horizontale de A, verticale de B.
    const std::vector<Point2> two{Point2(0, 10), Point2(30, 0)};
    a = trackAlign(two, Point2(29.6, 10.3), 1.0);
    check(a && near(a->point.x_, 30) && near(a->point.y_, 10) && a->horizontalFrom && a->verticalFrom,
          "objet : croisement de deux alignements");

    // Le plus proche l'emporte.
    const std::vector<Point2> close{Point2(0, 10), Point2(50, 10.6)};
    a = trackAlign(close, Point2(20, 10.5), 1.0);
    check(a && near(a->point.y_, 10.6), "objet : alignement le plus proche");

    if (failures) return 1;
    std::printf("Reperages polaire et d'objet : tests PASSED\n");
    return 0;
}
