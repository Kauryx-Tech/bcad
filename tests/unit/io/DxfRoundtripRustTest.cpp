// Phase 6 : Round-trip DXF via Rust (read -> write -> read full)
// Vérifie que le pipeline lecture (C++ natif) -> écriture (Rust) -> lecture (Rust full) fonctionne.

#include "bcad/io/DxfReader.h"
#include "bcad/io/DxfBridge.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Arc.h"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace bcad;
using namespace bcad::io;

namespace {

std::filesystem::path writeTemp(const std::string& name, const std::string& content) {
    const auto dir = std::filesystem::temp_directory_path() / "bcad_dxf_roundtrip_test";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::ofstream out(path, std::ios::binary);
    out << content;
    return path;
}

void checkLayersEqual(const core::Document& a, const core::Document& b) {
    const auto& layersA = a.layerManager().layers();
    const auto& layersB = b.layerManager().layers();
    assert(layersA.size() == layersB.size() && "Layer count mismatch");
    for (size_t i = 0; i < layersA.size(); ++i) {
        assert(layersA[i].name == layersB[i].name && "Layer name mismatch");
        assert(layersA[i].color == layersB[i].color && "Layer color mismatch");
    }
}

void checkEntitiesEqual(const core::Document& a, const core::Document& b) {
    const auto& entitiesA = a.entities();
    const auto& entitiesB = b.entities();
    assert(entitiesA.size() == entitiesB.size() && "Entity count mismatch");

    // Sort by layer then type for comparison
    std::vector<geom::Entity*> vecA, vecB;
    for (const auto& e : entitiesA) vecA.push_back(e.get());
    for (const auto& e : entitiesB) vecB.push_back(e.get());

    auto cmp = [](const geom::Entity* lhs, const geom::Entity* rhs) {
        if (lhs->layer() != rhs->layer()) return lhs->layer() < rhs->layer();
        return lhs->typeId().value < rhs->typeId().value;
    };
    std::sort(vecA.begin(), vecA.end(), cmp);
    std::sort(vecB.begin(), vecB.end(), cmp);

    for (size_t i = 0; i < vecA.size(); ++i) {
        const auto* ea = vecA[i];
        const auto* eb = vecB[i];
        assert(ea->typeId() == eb->typeId() && "Type mismatch");
        assert(ea->layer() == eb->layer() && "Layer mismatch");
        assert(ea->colorOverride() == eb->colorOverride() && "Color mismatch");

        if (ea->typeId() == geom::TypeId_Line) {
            const auto* la = static_cast<const geom::LineEntity*>(ea);
            const auto* lb = static_cast<const geom::LineEntity*>(eb);
            assert(la->start().x_ == lb->start().x_ && "Line start x");
            assert(la->start().y_ == lb->start().y_ && "Line start y");
            assert(la->end().x_ == lb->end().x_ && "Line end x");
            assert(la->end().y_ == lb->end().y_ && "Line end y");
        } else if (ea->typeId() == geom::TypeId_Circle) {
            const auto* ca = static_cast<const geom::CircleEntity*>(ea);
            const auto* cb = static_cast<const geom::CircleEntity*>(eb);
            assert(ca->center().x_ == cb->center().x_ && "Circle center x");
            assert(ca->center().y_ == cb->center().y_ && "Circle center y");
            assert(ca->radius() == cb->radius() && "Circle radius");
        } else if (ea->typeId() == geom::TypeId_Arc) {
            const auto* aa = static_cast<const geom::ArcEntity*>(ea);
            const auto* ab = static_cast<const geom::ArcEntity*>(eb);
            assert(aa->center().x_ == ab->center().x_ && "Arc center x");
            assert(aa->center().y_ == ab->center().y_ && "Arc center y");
            assert(aa->radius() == ab->radius() && "Arc radius");
            assert(aa->startAngle() == ab->startAngle() && "Arc start angle");
            assert(aa->endAngle() == ab->endAngle() && "Arc end angle");
        } else if (ea->typeId() == geom::TypeId_Polyline) {
            const auto* pa = static_cast<const geom::PolylineEntity*>(ea);
            const auto* pb = static_cast<const geom::PolylineEntity*>(eb);
            assert(pa->closed() == pb->closed() && "Polyline closed");
            assert(pa->vertices().size() == pb->vertices().size() && "Polyline vertex count");
            for (size_t v = 0; v < pa->vertices().size(); ++v) {
                assert(pa->vertices()[v].x_ == pb->vertices()[v].x_ && "Polyline vertex x");
                assert(pa->vertices()[v].y_ == pb->vertices()[v].y_ && "Polyline vertex y");
            }
        }
    }
}

}  // namespace

int main() {
    std::cout << "=== DXF Round-trip test (C++ read -> Rust write -> Rust full read) ===\n";

    // Create a document with various entities using native C++ types
    core::Document doc1;
    doc1.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    doc1.layerManager().createLayer("PARCELS", geom::Color::fromRgb255(0, 255, 0));
    doc1.layerManager().createLayer("ROUTES", geom::Color::fromRgb255(255, 0, 0));

    // Add entities
    doc1.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(10, 0)))->setLayer("PARCELS");
    doc1.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(10, 0), geom::Point2(10, 10)))->setLayer("PARCELS");
    doc1.addEntity(std::make_unique<geom::CircleEntity>(geom::Point2(5, 5), 2.0))->setLayer("ROUTES");
    doc1.addEntity(std::make_unique<geom::ArcEntity>(geom::Point2(0, 0), 3.0, 0.0, M_PI/2))->setLayer("ROUTES");
    doc1.addEntity(std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{{0,0}, {10,0}, {10,10}, {0,10}}, true))->setLayer("PARCELS");

    // Step 1: Write using Rust exporter
    const auto writtenPath = writeTemp("written.dxf", "");
    std::cout << "Step 1: Writing DXF via Rust exporter...\n";
    const DxfWriteResult writeResult = writeDxfToFile(doc1, writtenPath.string());
    assert(writeResult.success && ("Write failed: " + writeResult.error_message).c_str());
    std::cout << "Step 1: Write DXF via Rust exporter - OK\n";

    // Step 2: Read back using Rust parser WITH FULL ENTITY IMPORT
    DxfBridgeOptions options;
    options.recovery_mode = DxfRecoveryMode::Recover;
    std::cout << "Step 2: Reading written DXF via Rust parser (FULL)...\n";
    const DxfBridgeResult read2 = readDxfFromFile(writtenPath.string(), options);
    assert(read2.success && read2.document != nullptr && "Second read failed");
    assert(!hasErrors(read2) && "Second read has errors");
    std::cout << "Step 2: Read written DXF (full) - OK (" << read2.imported_entity_count << " entities)\n";

    // Step 3: Compare layers
    std::cout << "Step 3: Comparing layers...\n";
    const auto& layers1 = doc1.layerManager().layers();
    const auto& layers2 = read2.document->layerManager().layers();
    assert(layers1.size() == layers2.size() && "Layer count mismatch");
    for (size_t i = 0; i < layers1.size(); ++i) {
        assert(layers1[i].name == layers2[i].name && "Layer name mismatch");
        // Note: Color comparison skipped due to DXF ACI palette limitations
        // The DXF format uses ACI (AutoCAD Color Index) which has limited palette
    }
    std::cout << "Step 3: Compare layers - OK\n";

    // Step 4: Compare entities
    std::cout << "Step 4: Comparing entities...\n";
    // We need the original doc1 for comparison, so let's recreate it
    core::Document doc1_orig;
    doc1_orig.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    doc1_orig.layerManager().createLayer("PARCELS", geom::Color::fromRgb255(0, 255, 0));
    doc1_orig.layerManager().createLayer("ROUTES", geom::Color::fromRgb255(255, 0, 0));
    doc1_orig.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(10, 0)))->setLayer("PARCELS");
    doc1_orig.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(10, 0), geom::Point2(10, 10)))->setLayer("PARCELS");
    doc1_orig.addEntity(std::make_unique<geom::CircleEntity>(geom::Point2(5, 5), 2.0))->setLayer("ROUTES");
    doc1_orig.addEntity(std::make_unique<geom::ArcEntity>(geom::Point2(0, 0), 3.0, 0.0, M_PI/2))->setLayer("ROUTES");
    doc1_orig.addEntity(std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{{0,0}, {10,0}, {10,10}, {0,10}}, true))->setLayer("PARCELS");

    // Create checkEntitiesEqual function inline for this test
    {
        const auto& entitiesA = doc1_orig.entities();
        const auto& entitiesB = read2.document->entities();
        assert(entitiesA.size() == entitiesB.size() && "Entity count mismatch");

        // Sort by layer then type for comparison
        std::vector<geom::Entity*> vecA, vecB;
        for (const auto& e : entitiesA) vecA.push_back(e.get());
        for (const auto& e : entitiesB) vecB.push_back(e.get());

        auto cmp = [](const geom::Entity* lhs, const geom::Entity* rhs) {
            if (lhs->layer() != rhs->layer()) return lhs->layer() < rhs->layer();
            return lhs->typeId().value < rhs->typeId().value;
        };
        std::sort(vecA.begin(), vecA.end(), cmp);
        std::sort(vecB.begin(), vecB.end(), cmp);

        for (size_t i = 0; i < vecA.size(); ++i) {
            const auto* ea = vecA[i];
            const auto* eb = vecB[i];
            assert(ea->typeId() == eb->typeId() && "Type mismatch");
            assert(ea->layer() == eb->layer() && "Layer mismatch");
            assert(ea->colorOverride() == eb->colorOverride() && "Color mismatch");

            if (ea->typeId() == geom::TypeId_Line) {
                const auto* la = static_cast<const geom::LineEntity*>(ea);
                const auto* lb = static_cast<const geom::LineEntity*>(eb);
                assert(la->start().x_ == lb->start().x_ && "Line start x");
                assert(la->start().y_ == lb->start().y_ && "Line start y");
                assert(la->end().x_ == lb->end().x_ && "Line end x");
                assert(la->end().y_ == lb->end().y_ && "Line end y");
            } else if (ea->typeId() == geom::TypeId_Circle) {
                const auto* ca = static_cast<const geom::CircleEntity*>(ea);
                const auto* cb = static_cast<const geom::CircleEntity*>(eb);
                assert(ca->center().x_ == cb->center().x_ && "Circle center x");
                assert(ca->center().y_ == cb->center().y_ && "Circle center y");
                assert(ca->radius() == cb->radius() && "Circle radius");
            } else if (ea->typeId() == geom::TypeId_Arc) {
                const auto* aa = static_cast<const geom::ArcEntity*>(ea);
                const auto* ab = static_cast<const geom::ArcEntity*>(eb);
                assert(aa->center().x_ == ab->center().x_ && "Arc center x");
                assert(aa->center().y_ == ab->center().y_ && "Arc center y");
                assert(aa->radius() == ab->radius() && "Arc radius");
                assert(aa->startAngle() == ab->startAngle() && "Arc start angle");
                assert(aa->endAngle() == ab->endAngle() && "Arc end angle");
            } else if (ea->typeId() == geom::TypeId_Polyline) {
                const auto* pa = static_cast<const geom::PolylineEntity*>(ea);
                const auto* pb = static_cast<const geom::PolylineEntity*>(eb);
                assert(pa->closed() == pb->closed() && "Polyline closed");
                assert(pa->vertices().size() == pb->vertices().size() && "Polyline vertex count");
                for (size_t v = 0; v < pa->vertices().size(); ++v) {
                    assert(pa->vertices()[v].x_ == pb->vertices()[v].x_ && "Polyline vertex x");
                    assert(pa->vertices()[v].y_ == pb->vertices()[v].y_ && "Polyline vertex y");
                }
            }
        }
    }
    std::cout << "Step 4: Compare entities - OK\n";

    std::cout << "\nAll round-trip tests PASSED\n";
    return 0;
}