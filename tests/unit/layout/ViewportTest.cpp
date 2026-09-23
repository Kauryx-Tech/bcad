#include "bcad/layout/Viewport.h"
#include "bcad/layout/Sheet.h"
#include <cassert>
#include <cmath>

using namespace bcad::layout;
using namespace bcad::geom;

int main() {
    Sheet a3(PaperFormat::A3, Orientation::Paysage); // 420x297, printable 400x277
    BoundingBox bbox;
    bbox.expand(Point2{0, 0});
    bbox.expand(Point2{100, 50}); // 100m x 50m

    Viewport vp;
    vp.setSource(bbox);
    vp.setScale(500);
    // 100m à 1:500 = 200mm, 50m = 100mm
    assert(std::abs(vp.widthOnSheet() - 200) < 1e-9);
    assert(std::abs(vp.heightOnSheet() - 100) < 1e-9);
    assert(vp.fitsIn(a3));

    // Ne rentre pas à 1:200
    vp.setScale(200);
    assert(std::abs(vp.widthOnSheet() - 500) < 1e-9);
    assert(!vp.fitsIn(a3));

    // Auto-échelle
    vp.setSource(bbox);
    double s = vp.autoScale(a3);
    assert(s >= 200 && s <= 500);

    return 0;
}
