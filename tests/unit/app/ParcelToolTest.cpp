#include "bcad/app/ParcelTool.h"
#include <cassert>
using namespace bcad::app;
using namespace bcad::geom;
int main() {
    ParcelTool t;
    assert(!t.canClose());
    t.addPoint({0,0}); t.addPoint({10,0});
    assert(!t.canClose());
    t.addPoint({10,10});
    assert(t.canClose());
    std::vector<Point2> out;
    assert(t.close(out) && out.size() == 3);
    t.undoLast();
    assert(!t.canClose());
    t.clear();
    assert(t.count() == 0);
    return 0;
}
