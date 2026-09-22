// Round-trip serialization test - verifies each native entity can be
// serialized and deserialized without data loss.

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Types.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

// Force native entity registration (static init might not run in static linking)
namespace bcad::registry { void registerNativeEntities(); }

constexpr double EPS = 1e-9;

bool nearlyEqual(double a, double b, double eps = EPS) {
    return std::abs(a - b) < eps;
}

bool pointEqual(const bcad::geom::Point2& a, const bcad::geom::Point2& b) {
    return nearlyEqual(a.x_, b.x_) && nearlyEqual(a.y_, b.y_);
}

bool colorEqual(const bcad::geom::Color& a, const bcad::geom::Color& b) {
    return nearlyEqual(a.r, b.r) && nearlyEqual(a.g, b.g) && 
           nearlyEqual(a.b, b.b) && nearlyEqual(a.a, b.a);
}

bool optionalColorEqual(const std::optional<bcad::geom::Color>& a, const std::optional<bcad::geom::Color>& b) {
    if (a.has_value() != b.has_value()) return false;
    if (!a.has_value()) return true;
    return colorEqual(*a, *b);
}

bool entityEqual(const bcad::geom::Entity* a, const bcad::geom::Entity* b) {
    if (a->typeId() != b->typeId()) return false;
    if (a->layer() != b->layer()) return false;
    if (!optionalColorEqual(a->colorOverride(), b->colorOverride())) return false;
    if (a->selected != b->selected) return false;
    
    if (a->typeId() == bcad::geom::TypeId_Point) {
        return pointEqual(static_cast<const bcad::geom::PointEntity*>(a)->position(),
                          static_cast<const bcad::geom::PointEntity*>(b)->position());
    } else if (a->typeId() == bcad::geom::TypeId_Line) {
        const auto* la = static_cast<const bcad::geom::LineEntity*>(a);
        const auto* lb = static_cast<const bcad::geom::LineEntity*>(b);
        return pointEqual(la->start(), lb->start()) && pointEqual(la->end(), lb->end());
    } else if (a->typeId() == bcad::geom::TypeId_Circle) {
        const auto* ca = static_cast<const bcad::geom::CircleEntity*>(a);
        const auto* cb = static_cast<const bcad::geom::CircleEntity*>(b);
        return pointEqual(ca->center(), cb->center()) && nearlyEqual(ca->radius(), cb->radius());
    } else if (a->typeId() == bcad::geom::TypeId_Arc) {
        const auto* aa = static_cast<const bcad::geom::ArcEntity*>(a);
        const auto* ab = static_cast<const bcad::geom::ArcEntity*>(b);
        return pointEqual(aa->center(), ab->center()) && 
               nearlyEqual(aa->radius(), ab->radius()) &&
               nearlyEqual(aa->startAngle(), ab->startAngle()) &&
               nearlyEqual(aa->endAngle(), ab->endAngle());
    } else if (a->typeId() == bcad::geom::TypeId_Polyline) {
        const auto* pa = static_cast<const bcad::geom::PolylineEntity*>(a);
        const auto* pb = static_cast<const bcad::geom::PolylineEntity*>(b);
        if (pa->closed() != pb->closed()) return false;
        if (pa->vertices().size() != pb->vertices().size()) return false;
        for (size_t i = 0; i < pa->vertices().size(); ++i) {
            if (!pointEqual(pa->vertices()[i], pb->vertices()[i])) return false;
        }
        return true;
    }
    return false;
}

int test_entity(const char* name, std::unique_ptr<bcad::geom::Entity> entity) {
    // Initialize serializers
    bcad::registry::registerNativeEntities();
    
    // Get serializer
    const auto* serializer = bcad::serialization::SerializerRegistry::find(entity->typeId());
    if (!serializer) {
        std::cerr << "FAIL: No serializer for " << name << std::endl;
        return 1;
    }
    
    // Store original layer and color for comparison
    std::string original_layer = entity->layer();
    std::optional<bcad::geom::Color> original_color = entity->colorOverride();
    
    // Serialize
    std::string serialized = serializer->serialize(*entity);
    if (serialized.empty()) {
        std::cerr << "FAIL: Serialization returned empty string for " << name << std::endl;
        return 1;
    }
    
    // Deserialize
    auto deserialized = serializer->deserialize(serialized);
    if (!deserialized) {
        std::cerr << "FAIL: Deserialization failed for " << name << std::endl;
        return 1;
    }
    
    // Restore layer and color on deserialized entity (serializer only handles geometric params)
    deserialized->setLayer(original_layer);
    deserialized->setColorOverride(original_color);
    
    // Compare
    if (!entityEqual(entity.get(), deserialized.get())) {
        std::cerr << "FAIL: Round-trip mismatch for " << name << std::endl;
        return 1;
    }
    
    std::cout << "OK: Round-trip for " << name << std::endl;
    return 0;
}

int main() {
    int failures = 0;
    
    // Test Point
    failures += test_entity("Point", 
        std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2(1.5, 2.5)));
    
    // Test Line
    failures += test_entity("Line",
        std::make_unique<bcad::geom::LineEntity>(bcad::geom::Point2(0, 0), bcad::geom::Point2(10, 5)));
    
    // Test Circle
    failures += test_entity("Circle",
        std::make_unique<bcad::geom::CircleEntity>(bcad::geom::Point2(5, 5), 3.0));
    
    // Test Arc
    failures += test_entity("Arc",
        std::make_unique<bcad::geom::ArcEntity>(bcad::geom::Point2(0, 0), 5.0, 0.0, M_PI/2));
    
    // Test Polyline (open)
    failures += test_entity("Polyline (open)",
        std::make_unique<bcad::geom::PolylineEntity>(
            std::vector<bcad::geom::Point2>{{0,0}, {1,0}, {1,1}, {0,1}}, false));
    
    // Test Polyline (closed)
    failures += test_entity("Polyline (closed)",
        std::make_unique<bcad::geom::PolylineEntity>(
            std::vector<bcad::geom::Point2>{{0,0}, {1,0}, {1,1}, {0,1}}, true));
    
    // Test with layer and color override
    {
        auto entity = std::make_unique<bcad::geom::CircleEntity>(bcad::geom::Point2(1, 2), 4.0);
        entity->setLayer("TestLayer");
        entity->setColorOverride(bcad::geom::Color::fromRgb255(255, 0, 0));
        failures += test_entity("Circle with layer/color", std::move(entity));
    }
    
    if (failures == 0) {
        std::cout << "\nAll round-trip tests PASSED" << std::endl;
        return 0;
    } else {
        std::cerr << "\n" << failures << " round-trip test(s) FAILED" << std::endl;
        return 1;
    }
}