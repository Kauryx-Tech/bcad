#include "bcad/layout/Borne.h"
#include <cassert>

using namespace bcad::layout;
using namespace bcad::geom;

int main() {
    std::vector<Point2> v = {{0,0},{10,0},{10,10},{0,10}};
    auto bornes = Borne::forParcel(v);
    assert(bornes.size() == 4);
    assert(bornes[0].numero == "B1");
    assert(bornes[3].numero == "B4");
    assert(bornes[0].position.x_ == 0 && bornes[0].position.y_ == 0);
    return 0;
}
