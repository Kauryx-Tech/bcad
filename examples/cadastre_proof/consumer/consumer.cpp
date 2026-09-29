#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Point.h"
#include <iostream>
#include <memory>
#include <vector>

// Consommateur externe du SDK BCAD (Phase 9) : compilé contre les targets
// importés `BCAD::core` / `BCAD::geometry` via find_package(BCAD CONFIG REQUIRED).
// Démontre la création d'une parcelle géométrique (polyline fermée) via le SDK.
int main() {
    bcad::core::Document doc;

    // Parcelle simple : rectangle 10x5 m, fermé.
    std::vector<bcad::geom::Point2> vertices = {
        {0.0, 0.0}, {10.0, 0.0}, {10.0, 5.0}, {0.0, 5.0}
    };
    auto parcel = std::make_unique<bcad::geom::PolylineEntity>(std::move(vertices), /*closed=*/true);

    doc.addEntity(std::move(parcel));

    if (doc.entities().size() != 1) {
        std::cerr << "FAIL: document size\n";
        return 1;
    }

    const auto& e = *doc.entities().front();
    if (e.typeId() != bcad::geom::TypeId_Polyline) {
        std::cerr << "FAIL: typeId (attendu bcad.Polyline)\n";
        return 1;
    }

    std::cout << "OK: consumer SDK (entity '" << e.typeId().value << "', vertices: " 
              << static_cast<const bcad::geom::PolylineEntity&>(e).vertices().size() << ")\n";
    return 0;
}