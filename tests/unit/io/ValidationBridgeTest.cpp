// Phase 3 : Validation bridge test
// Tests that the C++ bridge correctly calls Rust validation

#include "bcad/io/DxfBridge.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/PointEntity.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

using namespace bcad;
using namespace bcad::io;

namespace {

void checkLayersEqual(const core::Document& a, const core::Document& b) {
    const auto& layersA = a.layerManager().layers();
    const auto& layersB = b.layerManager().layers();
    assert(layersA.size() == layersB.size() && "Layer count mismatch");
    for (size_t i = 0; i < layersA.size(); ++i) {
        assert(layersA[i].name == layersB[i].name && "Layer name mismatch");
    }
}

}  // namespace

int main() {
    std::cout << "=== Validation bridge test ===\n";

    // Create a document with various entities
    core::Document doc;
    doc.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    doc.layerManager().createLayer("PARCELS", geom::Color::fromRgb255(0, 255, 0));
    doc.layerManager().createLayer("ROUTES", geom::Color::fromRgb255(255, 0, 0));

    // Add valid entities
    doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(10, 0)))->setLayer("PARCELS");
    doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(10, 0), geom::Point2(10, 10)))->setLayer("PARCELS");
    doc.addEntity(std::make_unique<geom::CircleEntity>(geom::Point2(5, 5), 2.0))->setLayer("ROUTES");
    doc.addEntity(std::make_unique<geom::ArcEntity>(geom::Point2(0, 0), 3.0, 0.0, M_PI/2))->setLayer("ROUTES");
    doc.addEntity(std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{{0,0}, {10,0}, {10,10}, {0,10}}, true))->setLayer("PARCELS");

    // Test 1: Valid document should have no errors
    std::cout << "Test 1: Valid document...\n";
    ValidationOptions options;
    options.check_degenerate = true;
    options.check_duplicate_points = true;
    options.check_overlap = true;
    options.tolerance = 1e-9;

    ValidationReport report = validateDocument(doc, options);
    assert(report.success && "Validation should succeed");
    assert(!report.error_message.empty() == false && "No error message for valid document");
    assert(report.issues.empty() && "Valid document should have no issues");
    std::cout << "  Valid document: PASS\n";

    // Test 2: Degenerate line (zero length)
    std::cout << "Test 2: Degenerate line...\n";
    core::Document doc2;
    doc2.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    doc2.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(0, 0)))->setLayer("0");

    ValidationReport report2 = validateDocument(doc2, options);
    assert(report2.success && "Validation should succeed");
    assert(report2.issues.size() == 1 && "Should have 1 issue");
    assert(report2.issues[0].code == "VAL-GEOM-001" && "Should be degenerate line error");
    assert(report2.issues[0].severity == ValidationSeverity::Error && "Should be error severity");
    std::cout << "  Degenerate line: PASS\n";

    // Test 3: Degenerate circle (radius smaller than tolerance)
    std::cout << "Test 3: Degenerate circle...\n";
    std::cout.flush();
    core::Document doc3;
    doc3.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    auto circle = std::make_unique<geom::CircleEntity>(geom::Point2(0, 0), 1e-10);
    std::cout << "  Circle radius before write: " << circle->radius() << "\n";
    std::cout.flush();
    doc3.addEntity(std::move(circle))->setLayer("0");

    // Also write to a file to inspect the DXF
    std::string debugPath = "/tmp/debug_degen_circle.dxf";
    DxfWriteResult writeResult = writeDxfToFile(doc3, debugPath);
    std::cout << "  Write success: " << writeResult.success << "\n";
    if (!writeResult.success) {
        std::cout << "  Write error: " << writeResult.error_message << "\n";
    }
    std::cout.flush();

    ValidationReport report3 = validateDocument(doc3, options);
    std::cout << "  Issues found: " << report3.issues.size() << "\n";
    for (const auto& issue : report3.issues) {
        std::cout << "  Issue: " << issue.code << " - " << issue.message << "\n";
    }
    std::cout.flush();
    assert(report3.success && "Validation should succeed");
    assert(report3.issues.size() == 1 && "Should have 1 issue");
    assert(report3.issues[0].code == "VAL-GEOM-002" && "Should be degenerate circle error");
    assert(report3.issues[0].severity == ValidationSeverity::Error && "Should be error severity");
    std::cout << "  Degenerate circle: PASS\n";

    // Test 4: Degenerate arc (radius smaller than tolerance)
    std::cout << "Test 4: Degenerate arc...\n";
    core::Document doc4;
    doc4.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    doc4.addEntity(std::make_unique<geom::ArcEntity>(geom::Point2(0, 0), 1e-10, 0.0, M_PI/2))->setLayer("0");

    ValidationReport report4 = validateDocument(doc4, options);
    assert(report4.success && "Validation should succeed");
    assert(report4.issues.size() == 1 && "Should have 1 issue");
    assert(report4.issues[0].code == "VAL-GEOM-003" && "Should be degenerate arc error");
    assert(report4.issues[0].severity == ValidationSeverity::Error && "Should be error severity");
    std::cout << "  Degenerate arc: PASS\n";

    // Test 5: Duplicate consecutive points in polyline
    std::cout << "Test 5: Duplicate points in polyline...\n";
    core::Document doc5;
    doc5.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    doc5.addEntity(std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{{0,0}, {0,0}, {10,10}}, false))->setLayer("0");

    ValidationReport report5 = validateDocument(doc5, options);
    assert(report5.success && "Validation should succeed");
    assert(report5.issues.size() == 1 && "Should have 1 warning");
    assert(report5.issues[0].code == "VAL-GEOM-005" && "Should be duplicate points warning");
    assert(report5.issues[0].severity == ValidationSeverity::Warning && "Should be warning severity");
    std::cout << "  Duplicate points: PASS\n";

    // Test 6: Overlap detection (bounding box overlap)
    std::cout << "Test 6: Overlap detection...\n";
    core::Document doc6;
    doc6.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    // Two overlapping squares
    doc6.addEntity(std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{{0,0}, {10,0}, {10,10}, {0,10}}, true))->setLayer("0");
    doc6.addEntity(std::make_unique<geom::PolylineEntity>(std::vector<geom::Point2>{{5,5}, {15,5}, {15,15}, {5,15}}, true))->setLayer("0");

    ValidationReport report6 = validateDocument(doc6, options);
    assert(report6.success && "Validation should succeed");
    // Should have overlap warning
    bool hasOverlap = false;
    for (const auto& issue : report6.issues) {
        if (issue.code == "VAL-TOPO-001") {
            hasOverlap = true;
            break;
        }
    }
    assert(hasOverlap && "Should have overlap warning");
    assert(report6.issues[0].severity == ValidationSeverity::Warning && "Should be warning severity");
    std::cout << "  Overlap detection: PASS\n";

    // Test 7: Multiple issues in one document
    std::cout << "Test 7: Multiple issues...\n";
    core::Document doc7;
    doc7.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    // Degenerate line + valid circle
    doc7.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(0, 0)))->setLayer("0");
    doc7.addEntity(std::make_unique<geom::CircleEntity>(geom::Point2(5, 5), 2.0))->setLayer("0");

    ValidationReport report7 = validateDocument(doc7, options);
    assert(report7.success && "Validation should succeed");
    assert(report7.issues.size() == 1 && "Should have 1 error (degenerate line)");
    assert(report7.issues[0].code == "VAL-GEOM-001" && "Should be degenerate line error");
    std::cout << "  Multiple issues: PASS\n";

    // Test 8: Options control
    std::cout << "Test 8: Options control...\n";
    core::Document doc8;
    doc8.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));
    doc8.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(0, 0)))->setLayer("0");

    // With check_degenerate = false, should have no issues
    ValidationOptions options2;
    options2.check_degenerate = false;
    options2.check_duplicate_points = true;
    options2.check_overlap = true;
    options2.tolerance = 1e-9;

    ValidationReport report8 = validateDocument(doc8, options2);
    assert(report8.success && "Validation should succeed");
    assert(report8.issues.empty() && "Should have no issues when check_degenerate is false");
    std::cout << "  Options control: PASS\n";

    // Test 9: Empty document
    std::cout << "Test 9: Empty document...\n";
    core::Document doc9;
    doc9.layerManager().createLayer("0", geom::Color::fromRgb255(255, 255, 255));

    ValidationReport report9 = validateDocument(doc9, options);
    assert(report9.success && "Validation should succeed");
    assert(report9.issues.empty() && "Empty document should have no issues");
    std::cout << "  Empty document: PASS\n";

    std::cout << "\nAll validation bridge tests PASSED\n";
    return 0;
}