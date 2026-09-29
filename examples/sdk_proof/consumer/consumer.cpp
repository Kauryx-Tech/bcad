#include "bcad/core/Document.h"
#include "bcad/geometry/PointEntity.h"
#include <iostream>
#include <memory>

// Consommateur externe du SDK BCAD (Phase 9) : compilé contre les targets
// importés `BCAD::core` / `BCAD::geometry` via find_package(BCAD CONFIG REQUIRED).
int main() {
    bcad::core::Document doc;
    doc.addEntity(std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{1.0, 2.0}));
    if (doc.entities().size() != 1) {
        std::cerr << "FAIL: document size\n";
        return 1;
    }
    const auto& e = *doc.entities().front();
    if (e.typeId() != bcad::geom::TypeId_Point) {
        std::cerr << "FAIL: typeId\n";
        return 1;
    }
    std::cout << "OK: consumer SDK (entity '" << e.typeId().value << "')\n";
    return 0;
}