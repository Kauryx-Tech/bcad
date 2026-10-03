// Round-trip tests for polygon holes via the real SerializerRegistry.
// Validates that encodeRings/decodeRings is backwards-compatible with v1 files.

#include "bcad/geometry/Polyline.h"
#include "bcad/serialization/Serializer.h"
#include "bcad/registry/EntityRegistry.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace bcad::registry { void registerNativeEntities(); }
namespace bcad::serialization { void registerNativeSerializers(); }

static const bcad::serialization::IEntitySerializer* polylineSer() {
    const auto* s = bcad::serialization::SerializerRegistry::find(bcad::geom::TypeId_Polyline);
    assert(s && "PolylineSerializer not registered");
    return s;
}

static const bcad::geom::PolylineEntity* toPolyline(const bcad::geom::Entity* e) {
    const auto* p = dynamic_cast<const bcad::geom::PolylineEntity*>(e);
    assert(p && "deserialize did not return a PolylineEntity");
    return p;
}

int main() {
    bcad::registry::registerNativeEntities();
    bcad::serialization::registerNativeSerializers();

    // Test 1: single hole — serialize then deserialize via registry
    std::cout << "Test 1: Polyline with hole..." << std::endl;
    {
        bcad::geom::PolylineEntity outer({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true);
        outer.addHole({{3, 3}, {7, 3}, {7, 7}, {3, 7}});

        assert(outer.hasHoles());
        assert(outer.holeCount() == 1);

        const auto* ser = polylineSer();
        std::string serialized = ser->serialize(outer);
        std::cout << "Serialized: " << serialized << std::endl;

        auto deserialized = ser->deserialize(serialized);
        assert(deserialized != nullptr);
        const auto* parsed = toPolyline(deserialized.get());
        assert(parsed->closed());
        assert(parsed->vertices().size() == 4);
        assert(parsed->hasHoles());
        assert(parsed->holeCount() == 1);
        assert(parsed->holes()[0].size() == 4);
        assert(std::abs(parsed->holes()[0][0].x_ - 3.0) < 1e-9);
        assert(std::abs(parsed->holes()[0][0].y_ - 3.0) < 1e-9);

        std::cout << "Test 1 passed" << std::endl;
    }

    // Test 2: multiple holes
    std::cout << "Test 2: Multiple holes round-trip..." << std::endl;
    {
        bcad::geom::PolylineEntity test({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true);
        test.addHole({{3, 3}, {7, 3}, {7, 7}, {3, 7}});
        test.addHole({{11, 11}, {15, 11}, {15, 15}, {11, 15}});

        const auto* ser = polylineSer();
        auto deserialized = ser->deserialize(ser->serialize(test));
        assert(deserialized != nullptr);
        const auto* parsed = toPolyline(deserialized.get());
        assert(parsed->holeCount() == 2);
        assert(parsed->holes()[0].size() == 4);
        assert(parsed->holes()[1].size() == 4);
        assert(std::abs(parsed->holes()[0][0].x_ - 3.0) < 1e-9);
        assert(std::abs(parsed->holes()[1][0].x_ - 11.0) < 1e-9);

        std::cout << "Test 2 passed" << std::endl;
    }

    // Test 3: no holes — round-trip stays clean
    std::cout << "Test 3: No holes..." << std::endl;
    {
        bcad::geom::PolylineEntity noHoles({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true);
        assert(!noHoles.hasHoles());

        const auto* ser = polylineSer();
        auto deserialized = ser->deserialize(ser->serialize(noHoles));
        assert(deserialized != nullptr);
        assert(!toPolyline(deserialized.get())->hasHoles());

        std::cout << "Test 3 passed" << std::endl;
    }

    // Test 4: clearHoles
    std::cout << "Test 4: clearHoles..." << std::endl;
    {
        bcad::geom::PolylineEntity e({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true);
        e.addHole({{3, 3}, {7, 3}, {7, 7}, {3, 7}});
        assert(e.hasHoles());
        e.clearHoles();
        assert(!e.hasHoles());
        assert(e.holeCount() == 0);

        std::cout << "Test 4 passed" << std::endl;
    }

    // Test 5: v1 backwards compatibility — old payload has no ";" separator
    std::cout << "Test 5: v1 backwards compatibility..." << std::endl;
    {
        const std::string v1 = "1,0,0,20,0,20,10,0,10";
        const auto* ser = polylineSer();
        auto deserialized = ser->deserialize(v1);
        assert(deserialized != nullptr);
        const auto* parsed = toPolyline(deserialized.get());
        assert(parsed->closed());
        assert(parsed->vertices().size() == 4);
        assert(!parsed->hasHoles());

        std::cout << "Test 5 passed" << std::endl;
    }

    std::cout << "\nAll hole tests passed!" << std::endl;
    return 0;
}
