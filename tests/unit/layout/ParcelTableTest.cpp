#include "bcad/layout/ParcelTable.h"
#include <cassert>
#include <cmath>

using namespace bcad::layout;

int main() {
    ParcelTable t;
    assert(t.size() == 0);
    t.add({"A", "42", "500m²", "Severin", 500});
    t.add({"A", "43", "300m²", "Severin", 300});
    assert(t.size() == 2);
    assert(std::abs(t.totalArea() - 800) < 1e-9);
    assert(t.toCsv().find("Section") != std::string::npos);
    assert(t.toCsv().find("A,42") != std::string::npos);
    t.clear();
    assert(t.size() == 0);
    return 0;
}
