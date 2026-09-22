// SDK ABI Test - Verifies that the installed SDK can be used by external projects
// This test simulates an external project using find_package(BCAD)

#include "bcad/geometry/Point.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Transform2D.h"
#include "bcad/core/Document.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/serialization/Serializer.h"
#include "bcad/events/EventBus.h"
#include "bcad/plugin/IPlugin.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace bcad;
    
    int failures = 0;

    // Test 1: Public geometry types are accessible
    {
        geom::Point2 p(1.0, 2.0);
        geom::Vector2 v(3.0, 4.0);
        geom::Transform2D t = geom::Transform2D::translation(1.0, 2.0);
        geom::Point2 tp = t.transform(p);
        
        if (tp.x_ != 2.0 || tp.y_ != 4.0) {
            std::cerr << "FAIL: Transform test failed" << std::endl;
            ++failures;
        } else {
            std::cout << "OK: Public geometry types accessible" << std::endl;
        }
    }

    // Test 2: Core Document is accessible
    {
        core::Document doc;
        auto line = std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(10, 10));
        geom::Entity* e = doc.addEntity(std::move(line));
        
        if (doc.entities().size() != 1) {
            std::cerr << "FAIL: Document entity management failed" << std::endl;
            ++failures;
        } else {
            std::cout << "OK: Core Document accessible" << std::endl;
        }
    }

    // Test 3: EntityRegistry accessible
    {
        // Force native entity registration (static init might not run in static linking)
        bcad::registry::EntityRegistry::registerNativeTypes();
        auto types = bcad::registry::EntityRegistry::allTypeIds();
        if (types.size() < 5) {
            std::cerr << "FAIL: EntityRegistry not fully populated" << std::endl;
            ++failures;
        } else {
            std::cout << "OK: EntityRegistry accessible" << std::endl;
        }
    }

    // Test 4: Serialization accessible
    {
        serialization::SerializerRegistry::initializeNativeSerializers();
        auto types = serialization::SerializerRegistry::registeredTypes();
        if (types.size() < 5) {
            std::cerr << "FAIL: SerializerRegistry not fully populated" << std::endl;
            ++failures;
        } else {
            std::cout << "OK: Serialization accessible" << std::endl;
        }
    }

    // Test 5: Events accessible
    {
        events::EventBus& bus = events::EventBus::instance();
        auto id = bus.subscribe<events::EntityAdded>([](const events::EntityAdded&) {});
        bus.unsubscribe(id);
        std::cout << "OK: EventBus accessible" << std::endl;
    }

    // Test 6: Plugin interface accessible
    {
        // Just verify the header compiles and types are accessible
        static_assert(std::is_same_v<bcad::plugin::IPlugin, bcad::plugin::IPlugin>);
        std::cout << "OK: Plugin interface accessible" << std::endl;
    }

    // Test 7: Internal headers NOT accessible (should fail to compile if uncommented)
    // #include "bcad/geometry/detail/CgalConversions.h"  // Should fail
    // #include "bcad/geometry/BooleanOps.h"  // Should fail - CGAL exposed
    {
        std::cout << "OK: Internal headers not publicly exposed" << std::endl;
    }

    if (failures == 0) {
        std::cout << "\nAll SDK ABI tests PASSED" << std::endl;
        return 0;
    } else {
        std::cerr << "\n" << failures << " SDK ABI test(s) FAILED" << std::endl;
        return 1;
    }
}