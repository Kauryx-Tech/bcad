#include "bcad/layout/AngleDimension.h"
#include <cassert>
#include <cmath>
using namespace bcad::layout;
using namespace bcad::geom;
int main() {
    std::vector<Point2> square = {{0,0},{10,0},{10,10},{0,10}};
    auto dims = AngleDimension::forPolyline(square);
    assert(dims.size() == 4);
    for (auto& d : dims) assert(std::abs(d.angleDeg - 90) < 1e-6);
    return 0;
}
