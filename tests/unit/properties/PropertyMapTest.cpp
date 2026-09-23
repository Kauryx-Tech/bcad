#include "bcad/properties/PropertyMap.h"
#include <cassert>
#include <iostream>

using namespace bcad::properties;

void testBasicOps() {
    PropertyMap pm;

    // Ajout de propriétés
    pm.addDouble("ratio", 2.5);
    pm.addInt("count", 42);
    pm.addString("label", "test");
    pm.addBool("enabled", true);
    pm.addColor("col", bcad::geom::Color::fromRgb255(100, 150, 200));

    // Récupération
    assert(pm.has("ratio"));
    assert(pm.has("count"));
    assert(pm.has("label"));
    assert(pm.has("enabled"));
    assert(pm.has("col"));

    assert(pm.getDouble("ratio", 0.0) == 2.5);
    assert(pm.getInt("count", 0) == 42);
    assert(pm.getString("label") == "test");
    assert(pm.getBool("enabled") == true);

    // Modification
    pm.setDouble("ratio", 3.14);
    assert(pm.getDouble("ratio", 0.0) == 3.14);

    // Liste des noms
    auto names = pm.listNames();
    assert(names.size() == 5);
    bool hasRatio = false, hasCount = false, hasLabel = false, hasEnabled = false, hasCol = false;
    for (const auto& n : names) {
        if (n == "ratio")   hasRatio = true;
        if (n == "count")   hasCount = true;
        if (n == "label")   hasLabel = true;
        if (n == "enabled") hasEnabled = true;
        if (n == "col")     hasCol = true;
    }
    assert(hasRatio && hasCount && hasLabel && hasEnabled && hasCol);

    // Suppression
    pm.remove("count");
    assert(!pm.has("count"));
    names = pm.listNames();
    assert(names.size() == 4);

    std::cout << "PropertyMap unit test OK\n";
}

int main() {
    testBasicOps();
    return 0;
}
