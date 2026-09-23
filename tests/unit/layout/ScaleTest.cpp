#include "bcad/layout/Scale.h"
#include <cassert>

using namespace bcad::layout;

int main() {
    assert(scaleText(500) == "1:500");
    assert(nearestStandardScale(300) == 500);
    assert(nearestStandardScale(500) == 500);
    assert(nearestStandardScale(600) == 1000);
    assert(nearestStandardScale(9999) == 10000);
    assert(gridStepMm(500) == 20);
    assert(gridStepMm(1000) == 10);
    assert(gridStepMm(5000) == 2);
    return 0;
}
