#include "bcad/layout/Dimension.h"
#include <cassert>
#include <cmath>

using namespace bcad::layout;
using namespace bcad::geom;

int main() {
    std::vector<Point2> square = {{0,0},{10,0},{10,10},{0,10}};
    auto dims = Dimension::forPolyline(square);
    assert(dims.size() == 4);
    for (auto& d : dims) assert(std::abs(d.value - 10) < 1e-9);
    assert(dims[0].text().find("10") != std::string::npos);

    std::vector<Point2> tri = {{0,0},{3,0},{0,4}};
    auto d2 = Dimension::forPolyline(tri);
    assert(d2.size() == 3);
    // Le polygone est ferme : cote 0 = (0,0)->(3,0), cote 1 = (3,0)->(0,4) = l'hypotenuse.
    assert(std::abs(d2[0].value - 3) < 1e-9);
    assert(std::abs(d2[1].value - 5) < 1e-9); // hypot(3,4)=5
    assert(std::abs(d2[2].value - 4) < 1e-9);

    return 0;
}
