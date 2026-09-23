#include "bcad/app/ParcelTool.h"
#include <cassert>
#include <cmath>
using namespace bcad::app;
int main() {
    ParcelTool t;
    assert(!t.addCogoPoint(0, 10)); // sans point de départ
    t.setStart({0, 0});
    assert(t.addCogoPoint(0, 10));   // N 10m
    assert(t.addCogoPoint(90, 10));  // E 10m
    assert(t.addCogoPoint(180, 10)); // S 10m
    assert(t.addCogoPoint(270, 10)); // W 10m -> retour au départ
    assert(t.canClose());
    assert(t.closureError() < 1e-9); // carré fermé
    std::vector<bcad::geom::Point2> out;
    assert(t.close(out) && out.size() == 5);
    // Polygone ouvert : erreur = distance au départ
    ParcelTool t2;
    t2.setStart({0, 0});
    t2.addCogoPoint(90, 100);
    assert(std::abs(t2.closureError() - 100) < 1e-9);
    assert(!t2.addCogoPoint(0, -5)); // distance négative refusée
    return 0;
}
