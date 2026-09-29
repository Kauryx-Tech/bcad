#include "bcad/layout/Viewport.h"
#include "bcad/layout/Composition.h"
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

    // Echelle non fixee : rien n'est mesurable sur la feuille, et rien ne
    // pretend y tenir. Le 500 par defaut d'origine faisait mentir fitsIn().
    Viewport undecided;
    undecided.setSource(bbox);
    assert(undecided.scale() == 0);
    assert(undecided.widthOnSheet() == 0);
    assert(!undecided.fitsIn(a3));

    // L'echelle admise est donnee par la zone reellement libre et la liste du
    // profil, pas par la feuille entiere : voir CompositionTest pour la
    // difference.
    vp.setSource(bbox);
    const RectMm full{0, 0, a3.printableWidth(), a3.printableHeight()};
    const double s = permittedScaleFor(bbox, full, {200, 500});
    assert(s >= 200 && s <= 500);
    vp.setScale(s);
    assert(vp.fitsIn(a3));

    return 0;
}
