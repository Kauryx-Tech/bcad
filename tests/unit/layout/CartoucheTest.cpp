#include "bcad/layout/Cartouche.h"
#include <cassert>

using namespace bcad::layout;

int main() {
    Cartouche c;
    assert(!c.isValid());
    c.commune = "Severin";
    c.section = "A";
    assert(c.isValid());
    assert(c.title().find("Severin") != std::string::npos);
    assert(c.title().find("Section A") != std::string::npos);
    c.echelle = "1:500";
    assert(c.title().find("1:500") != std::string::npos);
    c.heightMm = 30;
    assert(c.heightMm == 30);
    return 0;
}
