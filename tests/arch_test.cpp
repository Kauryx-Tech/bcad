// Architecture regression test - verifies no render symbols in bcad_core
// This test links against bcad_core and bcad_render and verifies
// that bcad_core has no undefined references to render symbols.

#include <bcad/core/Document.h>
#include <bcad/geometry/Line.h>
#include <bcad/geometry/Circle.h>
#include <bcad/geometry/PointEntity.h>
#include <bcad/geometry/Arc.h>
#include <bcad/geometry/Polyline.h>
#include <iostream>

int main() {
    // This test simply verifies that bcad_core can be used
    // without any render dependency. If render symbols were needed,
    // the linker would fail when only linking bcad_core.
    
    bcad::core::Document doc;
    
    // Add various entity types
    doc.addEntity(std::make_unique<bcad::geom::LineEntity>(bcad::geom::Point2(0,0), bcad::geom::Point2(10,10)));
    doc.addEntity(std::make_unique<bcad::geom::CircleEntity>(bcad::geom::Point2(5,5), 3.0));
    doc.addEntity(std::make_unique<bcad::geom::ArcEntity>(bcad::geom::Point2(0,0), 5.0, 0.0, 1.5));
    doc.addEntity(std::make_unique<bcad::geom::PolylineEntity>(
        std::vector<bcad::geom::Point2>{{0,0}, {1,0}, {1,1}, {0,1}}, true));
    doc.addEntity(std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2(2,2)));
    
    // Verify entities exist
    if (doc.entities().size() != 5) {
        std::cerr << "FAIL: Expected 5 entities, got " << doc.entities().size() << std::endl;
        return 1;
    }
    
    // Test spatial queries
    auto inRegion = doc.entitiesInRegion(bcad::geom::BoundingBox{-1,-1, 11,11});
    if (inRegion.size() != 5) {
        std::cerr << "FAIL: entitiesInRegion returned " << inRegion.size() << " instead of 5" << std::endl;
        return 1;
    }
    
    // Test picking
    auto picked = doc.pickEntity(bcad::geom::Point2(5,5), 1.0);
    if (!picked) {
        std::cerr << "FAIL: pickEntity returned null" << std::endl;
        return 1;
    }
    
    // Test tessellation (doesn't need render, just builds vertex data)
    auto tess = doc.buildTessellation(bcad::geom::BoundingBox{-1,-1, 11,11}, 0.1);
    if (tess.batches.empty()) {
        std::cerr << "FAIL: buildTessellation returned empty batches" << std::endl;
        return 1;
    }
    
    std::cout << "OK: Core functionality works without render dependency" << std::endl;
    return 0;
}