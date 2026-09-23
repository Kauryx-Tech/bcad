#include "bcad/cadastre/ParcelOps.h"
#include <cassert>
using namespace bcad::cadastre;
using namespace bcad::geom;
int main() {
    // Carré 10x10
    PolylineEntity parcel({{0,0},{10,0},{10,10},{0,10}}, true);
    
    // Direction verticale (coupe horizontale)
    PolylineEntity dirVert({{5,-1},{5,11}}, false);
    
    // Subdiviser en 4 lots égaux
    auto lots = subdivideParcel(parcel, 4, dirVert);
    assert(lots.has_value());
    assert(lots->size() == 4);
    // Chaque lot devrait faire ~25m²
    for (auto& lot : *lots) {
        assert(std::abs(lot.area() - 25) < 1e-6);
    }
    
    // Direction horizontale (coupe verticale)
    PolylineEntity dirHoriz({{-1,5},{11,5}}, false);
    auto lots2 = subdivideParcel(parcel, 2, dirHoriz);
    assert(lots2.has_value());
    assert(lots2->size() == 2);
    assert(std::abs(lots2->at(0).area() - 50) < 1e-6);
    assert(std::abs(lots2->at(1).area() - 50) < 1e-6);
    
    // N invalide
    assert(!subdivideParcel(parcel, 1, dirVert).has_value());
    
    return 0;
}
