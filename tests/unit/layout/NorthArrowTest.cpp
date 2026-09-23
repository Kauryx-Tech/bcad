#include "bcad/layout/NorthArrow.h"
#include <cassert>
#include <cmath>

using namespace bcad::layout;

int main() {
    NorthArrow na;
    na.position = {10, 10};
    na.sizeMm = 10;
    auto tri = na.triangle();
    assert(tri.size() == 3);
    // Pointe Nord en haut (y négatif)
    assert(tri[0].y_ < na.position.y_);

    na.angleDeg = 90;
    auto tri2 = na.triangle();
    assert(tri2[0].x_ > na.position.x_); // rotation 90°

    return 0;
}
