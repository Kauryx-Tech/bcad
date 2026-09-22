// TypeId stability test - verifies native TypeId uniqueness and format

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <cassert>
#include <iostream>
#include <set>
#include <string>

// Force native entity registration (static init might not run in static linking)
namespace bcad::registry { void registerNativeEntities(); }

int main() {
    using namespace bcad::geom;
    
    // Force native entity registration
    bcad::registry::registerNativeEntities();
    
    std::set<std::string> seen_ids;
    int errors = 0;
    
    auto check_typeid = [&](const Entity* e, const char* expected_prefix) {
        std::string id = e->typeId().value;
        std::cout << "Checking " << e->typeId().value << " for " << expected_prefix << std::endl;
        
        // Check non-empty
        if (id.empty()) {
            std::cerr << "FAIL: " << expected_prefix << " has empty TypeId" << std::endl;
            ++errors;
            return;
        }
        
        // Check prefix format
        std::string expected = "bcad.";
        if (id.rfind(expected, 0) != 0) {
            std::cerr << "FAIL: " << expected_prefix << " TypeId '" << id << "' doesn't start with 'bcad.'" << std::endl;
            ++errors;
        }
        
        // Check uniqueness
        if (seen_ids.count(id)) {
            std::cerr << "FAIL: Duplicate TypeId '" << id << "'" << std::endl;
            ++errors;
        }
        seen_ids.insert(id);
    };
    
    // Test all native entities
    PointEntity point;
    LineEntity line;
    CircleEntity circle;
    ArcEntity arc;
    PolylineEntity polyline;
    
    check_typeid(&point, "Point");
    check_typeid(&line, "Line");
    check_typeid(&circle, "Circle");
    check_typeid(&arc, "Arc");
    check_typeid(&polyline, "Polyline");
    
    // Verify registry has all types
    auto all_types = bcad::registry::EntityRegistry::all();
    std::cout << "Registry contains " << all_types.size() << " types" << std::endl;
    
    if (all_types.size() < 5) {
        std::cerr << "FAIL: Registry has fewer than 5 types" << std::endl;
        ++errors;
    }
    
    // Verify expected TypeIds are in registry
    const char* expected_types[] = {"bcad.Point", "bcad.Line", "bcad.Circle", "bcad.Arc", "bcad.Polyline"};
    for (const char* expected : expected_types) {
        if (!bcad::registry::EntityRegistry::contains(expected)) {
            std::cerr << "FAIL: Registry missing expected type " << expected << std::endl;
            ++errors;
        }
    }
    
    // Test TypeId format for plugin types (if any registered)
    for (const auto& meta : all_types) {
        std::string id = meta.typeId.value;
        if (id.rfind("bcad.", 0) != 0) {
            // Plugin type - should use reverse domain
            bool has_dot = id.find('.') != std::string::npos;
            if (!has_dot) {
                std::cerr << "WARNING: Plugin TypeId '" << id << "' should use reverse-domain notation" << std::endl;
            }
        }
    }
    
    if (errors == 0) {
        std::cout << "\nAll TypeId stability checks PASSED" << std::endl;
        return 0;
    } else {
        std::cerr << "\n" << errors << " TypeId stability check(s) FAILED" << std::endl;
        return 1;
    }
}