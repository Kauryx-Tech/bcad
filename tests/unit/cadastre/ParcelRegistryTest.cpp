#include "bcad/cadastre/ParcelRegistry.h"
#include <cassert>
using namespace bcad::cadastre;
int main() {
    ParcelRegistry r;
    assert(r.add("A", "42"));
    assert(!r.add("A", "42")); // doublon
    assert(r.exists("A", "42"));
    assert(!r.exists("A", "43"));
    assert(r.add("A", "43"));
    assert(r.size() == 2);
    assert(r.remove("A", "42"));
    assert(!r.exists("A", "42"));
    r.clear();
    assert(r.size() == 0);
    return 0;
}
