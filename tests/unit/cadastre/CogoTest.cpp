#include "bcad/cadastre/Cogo.h"
#include <cassert>
#include <cmath>
using namespace bcad::cadastre;
using namespace bcad::geom;
static bool near(double a, double b, double e = 1e-6) { return std::abs(a - b) < e; }
int main() {
    // Vecteur de référence US Army EN0593 : az 70°15'15" = 70.2541667°, dist 568.78
    // dN = +192.16, dE = +535.34
    Point2 a = forward({0, 0}, 70.2541667, 568.78);
    assert(near(a.y_, 192.16, 0.01));
    assert(near(a.x_, 535.34, 0.01));
    // Inverse : retour
    auto inv = inverse({0, 0}, a);
    assert(near(inv.distance, 568.78, 1e-6));
    assert(near(inv.azimuthDeg, 70.2541667, 1e-6));
    // Cardinaux
    assert(near(forward({0,0}, 0, 10).y_, 10));
    assert(near(forward({0,0}, 90, 10).x_, 10));
    assert(near(forward({0,0}, 180, 10).y_, -10));
    assert(near(forward({0,0}, 270, 10).x_, -10));
    // Gisements quadrant (table §A : NE=b, SE=180-b, SW=180+b, NW=360-b)
    assert(near(bearingToAzimuth(Quadrant::NE, 45), 45));
    assert(near(bearingToAzimuth(Quadrant::SE, 45), 135));
    assert(near(bearingToAzimuth(Quadrant::SW, 45), 225));
    assert(near(bearingToAzimuth(Quadrant::NW, 45), 315));
    // Parse "N45 30 00E" -> 45.5
    auto p1 = parseBearing("N45 30 00E");
    assert(p1 && near(*p1, 45.5));
    auto p2 = parseBearing("S15 02W");
    assert(p2 && near(*p2, 195.033333, 1e-4));
    assert(!parseBearing("X45E"));
    assert(!parseBearing("N100E")); // >90 invalide
    auto inv2 = inverse({0,0}, {10,0});
    assert(near(inv2.azimuthDeg, 90) && near(inv2.distance, 10));
    return 0;
}
