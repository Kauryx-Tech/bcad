#include "bcad/cadastre/OverlapCheck.h"
#include <cassert>
using namespace bcad::cadastre;
using namespace bcad::geom;
int main() {
    PolylineEntity a({{0,0},{10,0},{10,10},{0,10}}, true);
    PolylineEntity b({{5,5},{15,5},{15,15},{5,15}}, true);
    PolylineEntity c({{20,20},{30,20},{30,30},{20,30}}, true);

    auto overlaps = findOverlaps({a, b});
    assert(overlaps.size() == 1);
    assert(overlaps[0].area > 0);

    auto noOverlap = findOverlaps({a, c});
    assert(noOverlap.empty());

    auto empty = findOverlaps({});
    assert(empty.empty());

    return 0;
}
