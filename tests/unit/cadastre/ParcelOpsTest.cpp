#include "bcad/cadastre/ParcelOps.h"
#include <cassert>
using namespace bcad::cadastre;
using namespace bcad::geom;
int main() {
    PolylineEntity a({{0,0},{10,0},{10,10},{0,10}}, true);
    PolylineEntity b({{10,0},{20,0},{20,10},{10,10}}, true);
    auto merged = mergeParcels(a, b);
    assert(merged.has_value());
    assert(merged->vertices().size() >= 4);
    PolylineEntity cut({{5,-5},{5,15}}, false);
    auto split = splitParcel(a, cut);
    (void)split;
    return 0;
}
