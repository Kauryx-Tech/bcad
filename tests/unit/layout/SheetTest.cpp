#include "bcad/layout/Sheet.h"
#include <cassert>
#include <cmath>

using namespace bcad::layout;

int main() {
    Sheet a4(PaperFormat::A4, Orientation::Portrait);
    assert(std::abs(a4.width() - 210) < 1e-9);
    assert(std::abs(a4.height() - 297) < 1e-9);
    assert(a4.isValid());
    Sheet a3(PaperFormat::A3, Orientation::Paysage);
    assert(std::abs(a3.width() - 420) < 1e-9);
    assert(std::abs(a3.height() - 297) < 1e-9);
    Sheet s(PaperFormat::A4, Orientation::Portrait, {20, 20, 15, 15});
    assert(std::abs(s.printableWidth() - 180) < 1e-9);
    assert(std::abs(s.printableHeight() - 257) < 1e-9);
    Sheet a0(PaperFormat::A0, Orientation::Portrait);
    assert(std::abs(a0.width() - 841) < 1e-9);
    assert(std::abs(a0.height() - 1189) < 1e-9);
    a4.setFormat(PaperFormat::A3);
    assert(a4.format() == PaperFormat::A3);
    a4.setOrientation(Orientation::Paysage);
    assert(a4.orientation() == Orientation::Paysage);
    return 0;
}
