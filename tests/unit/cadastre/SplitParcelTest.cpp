#include "bcad/cadastre/ParcelOps.h"
#include <cassert>
using namespace bcad::cadastre;
using namespace bcad::geom;
int main() {
    // Carré 10x10
    PolylineEntity parcel({{0,0},{10,0},{10,10},{0,10}}, true);
    
    // Coupe verticale au milieu
    PolylineEntity cut({{5,-1},{5,11}}, false);
    
    auto split = splitParcel(parcel, cut);
    assert(split.has_value());
    auto [a, b] = *split;
    assert(std::abs(a.area() - 50) < 1e-6);
    assert(std::abs(b.area() - 50) < 1e-6);
    
    // Coupe qui ne traverse pas
    PolylineEntity cutOutside({{20,20},{30,30}}, false);
    auto noSplit = splitParcel(parcel, cutOutside);
    assert(!noSplit.has_value());
    
    return 0;
}
