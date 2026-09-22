// Test de référence pour le pattern "commande BCAD"
// Vérifie le comportement attendu de execute, undo, redo, des snapshots et de mergeWith.

#include "bcad/commands/Command.h"
#include "bcad/commands/ConcreteCommands.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Transform2D.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

using namespace bcad;
using namespace bcad::commands;
using namespace bcad::geom;

void testSetLayerCommand_fullCycle() {
    std::printf("\n=== testSetLayerCommand_fullCycle ===\n");
    core::Document doc;
    doc.layerManager().createLayer("TestLayer", geom::Color::fromRgb255(255, 0, 0));

    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    line->setLayer("TestLayer");
    int entityId = doc.addEntity(std::move(line))->id();

    // Vérifier calque initial
    auto* e = doc.findEntity(entityId);
    check(e->layer() == "TestLayer", "Initial layer is TestLayer");

    // Changer le calque via commande
    auto cmd = std::make_unique<SetLayerCommand>(entityId, "0", "Change Layer");
    CommandStack stack;
    stack.setDocument(doc);
    stack.push(std::move(cmd));

    e = doc.findEntity(entityId);
    check(e->layer() == "0", "Layer changed to 0 after execute");

    // Undo
    stack.undo();
    e = doc.findEntity(entityId);
    check(e != nullptr, "Entity exists after undo");
    check(e->layer() == "TestLayer", "Layer restored to TestLayer after undo");

    // Redo
    stack.redo();
    e = doc.findEntity(entityId);
    check(e != nullptr, "Entity exists after redo");
    check(e->layer() == "0", "Layer back to 0 after redo");

    // Vérifier log
    const auto& log = stack.log();
    check(log.size() == 3, "Log has 3 entries (push, undo, redo)");
    check(log[0].isUndo == false, "First log entry is execute");
    check(log[1].isUndo == true, "Second log entry is undo");
    check(log[2].isUndo == false, "Third log entry is redo");
}

void testTransformEntityCommand_merge() {
    std::printf("\n=== testTransformEntityCommand_merge ===\n");
    core::Document doc;

    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    int entityId = doc.addEntity(std::move(line))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Première translation
    auto t1 = Transform2D::translation(1, 0);
    auto cmd1 = std::make_unique<TransformEntityCommand>(entityId, t1, "Move +1 X");
    stack.push(std::move(cmd1));

    auto* linePtr = dynamic_cast<LineEntity*>(doc.findEntity(entityId));
    check(linePtr != nullptr, "Entity is a LineEntity");
    check(nearlyEqual(linePtr->start().x_, 1.0), "First translation applied");

    // Deuxième translation (doit merger avec la première)
    auto t2 = Transform2D::translation(2, 0);
    auto cmd2 = std::make_unique<TransformEntityCommand>(entityId, t2, "Move +2 X");
    stack.push(std::move(cmd2));

    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(entityId));
    check(linePtr != nullptr, "Entity is still a LineEntity");
    check(nearlyEqual(linePtr->start().x_, 3.0), "Second translation merged (total +3)");

    // Undo de la commande fusionnée
    stack.undo();
    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(entityId));
    check(linePtr != nullptr, "Entity exists after undo");
    check(nearlyEqual(linePtr->start().x_, 0.0), "Undo restored original position");

    // Redo
    stack.redo();
    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(entityId));
    check(linePtr != nullptr, "Entity exists after redo");
    check(nearlyEqual(linePtr->start().x_, 3.0), "Redo restored merged translation");
}

void testRemoveEntityCommand_snapshotCapturedAtExecution() {
    std::printf("\n=== testRemoveEntityCommand_snapshotCapturedAtExecution ===\n");
    core::Document doc;
    doc.layerManager().createLayer("ModifiedLayer", geom::Color::fromRgb255(0, 255, 0));

    // Créer une entité
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    line->setLayer("ModifiedLayer");
    int entityId = doc.addEntity(std::move(line))->id();

    std::string layerBeforeRemove = "ModifiedLayer";

    // Créer et exécuter la commande de suppression SANS CommandStack
    auto cmd = std::make_unique<RemoveEntityCommand>(entityId, "Remove Entity");
    cmd->execute(doc);

    // Vérifier que l'entité est supprimée
    check(doc.findEntity(entityId) == nullptr, "Entity removed after execute");

    // Undo direct
    cmd->undo(doc);

    // Vérifier que l'entité est restaurée
    geom::Entity* restored = nullptr;
    for (const auto& e : doc.entities()) {
        if (e->layer() == "ModifiedLayer") {
            restored = e.get();
            break;
        }
    }
    check(restored != nullptr, "Entity restored after undo (direct call)");
    if (restored) {
        check(restored->layer() == "ModifiedLayer", "Snapshot captured layer at execution time");
    }

    // Redo
    cmd->execute(doc);
    check(doc.entities().empty(), "Entity removed again after redo");
}

void testAddEntityCommand_undoRedo() {
    std::printf("\n=== testAddEntityCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    // Créer et exécuter une commande d'ajout
    auto cmd = std::make_unique<AddEntityCommand>(
        std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0)), "Add Line");
    stack.push(std::move(cmd));

    auto* linePtr = dynamic_cast<LineEntity*>(doc.entities()[0].get());
    check(linePtr != nullptr, "Entity added");
    check(linePtr->start().x_ == 0.0 && linePtr->start().y_ == 0.0, "Start point correct");
    check(linePtr->end().x_ == 10.0 && linePtr->end().y_ == 0.0, "End point correct");

    // Undo
    stack.undo();
    check(doc.entities().empty(), "Entity removed after undo");

    // Redo
    stack.redo();
    check(doc.entities().size() == 1, "Entity restored after redo");
    auto* linePtr2 = dynamic_cast<LineEntity*>(doc.entities()[0].get());
    check(linePtr2 != nullptr, "Restored entity is LineEntity");
}

void testSetColorOverrideCommand_undoRedo() {
    std::printf("\n=== testSetColorOverrideCommand_undoRedo ===\n");
    core::Document doc;

    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    circle->setColorOverride(Color::fromRgb255(255, 0, 0));
    int entityId = doc.addEntity(std::move(circle))->id();

    auto* circlePtr = dynamic_cast<CircleEntity*>(doc.findEntity(entityId));
    check(circlePtr->colorOverride().has_value(), "Initial color override set");

    CommandStack stack;
    stack.setDocument(doc);

    auto cmd = std::make_unique<SetColorOverrideCommand>(entityId, Color::fromRgb255(0, 255, 0), "Change Color");
    stack.push(std::move(cmd));

    auto* circlePtr2 = dynamic_cast<CircleEntity*>(doc.findEntity(entityId));
    check(circlePtr2->colorOverride().has_value(), "Color override changed");
    check(circlePtr2->colorOverride()->g == 1.0f, "New color is green");

    // Undo
    stack.undo();
    auto* circlePtr3 = dynamic_cast<CircleEntity*>(doc.findEntity(entityId));
    check(circlePtr3->colorOverride()->r == 1.0f, "Color restored to red after undo");

    // Redo
    stack.redo();
    auto* circlePtr4 = dynamic_cast<CircleEntity*>(doc.findEntity(entityId));
    check(circlePtr4->colorOverride()->g == 1.0f, "Color is green again after redo");
}

void testTransaction() {
    std::printf("\n=== testTransaction ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    auto tx = std::make_unique<Transaction>("Add two lines");
    tx->add(std::make_unique<AddEntityCommand>(
        std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0)), "Add Line 1"));
    tx->add(std::make_unique<AddEntityCommand>(
        std::make_unique<LineEntity>(Point2(0, 1), Point2(10, 1)), "Add Line 2"));

    stack.push(std::move(tx));

    check(doc.entities().size() == 2, "Transaction added two entities");

    stack.undo();
    check(doc.entities().empty(), "Transaction undo removed both entities");

    stack.redo();
    check(doc.entities().size() == 2, "Transaction redo restored both entities");
}

// --- Drawing Commands Tests ---

void testLineCommand_undoRedo() {
    std::printf("\n=== testLineCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    auto cmd = std::make_unique<LineCommand>(Point2(0, 0), Point2(10, 0), "Draw Line");
    stack.push(std::move(cmd));

    check(doc.entities().size() == 1, "Line entity added");
    auto* linePtr = dynamic_cast<LineEntity*>(doc.entities()[0].get());
    check(linePtr != nullptr, "Entity is LineEntity");
    check(linePtr->start().x_ == 0.0 && linePtr->start().y_ == 0.0, "Start point correct");
    check(linePtr->end().x_ == 10.0 && linePtr->end().y_ == 0.0, "End point correct");

    // Undo
    stack.undo();
    check(doc.entities().empty(), "Entity removed after undo");

    // Redo
    stack.redo();
    check(doc.entities().size() == 1, "Entity restored after redo");
    auto* linePtr2 = dynamic_cast<LineEntity*>(doc.entities()[0].get());
    check(linePtr2 != nullptr, "Restored entity is LineEntity");
    check(linePtr2->start().x_ == 0.0 && linePtr2->start().y_ == 0.0, "Start point correct after redo");
    check(linePtr2->end().x_ == 10.0 && linePtr2->end().y_ == 0.0, "End point correct after redo");
}

void testCircleCommand_undoRedo() {
    std::printf("\n=== testCircleCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    auto cmd = std::make_unique<CircleCommand>(Point2(0, 0), 5.0, "Draw Circle");
    stack.push(std::move(cmd));

    check(doc.entities().size() == 1, "Circle entity added");
    auto* circlePtr = dynamic_cast<CircleEntity*>(doc.entities()[0].get());
    check(circlePtr != nullptr, "Entity is CircleEntity");
    check(circlePtr->center().x_ == 0.0 && circlePtr->center().y_ == 0.0, "Center correct");
    check(circlePtr->radius() == 5.0, "Radius correct");

    // Undo
    stack.undo();
    check(doc.entities().empty(), "Entity removed after undo");

    // Redo
    stack.redo();
    check(doc.entities().size() == 1, "Entity restored after redo");
    auto* circlePtr2 = dynamic_cast<CircleEntity*>(doc.entities()[0].get());
    check(circlePtr2 != nullptr, "Restored entity is CircleEntity");
    check(circlePtr2->center().x_ == 0.0 && circlePtr2->center().y_ == 0.0, "Center correct after redo");
    check(circlePtr2->radius() == 5.0, "Radius correct after redo");
}

void testArcCommand_undoRedo() {
    std::printf("\n=== testArcCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    double startAngle = 0.0;
    double endAngle = std::numbers::pi / 2;  // 90 degrees
    auto cmd = std::make_unique<ArcCommand>(Point2(0, 0), 5.0, startAngle, endAngle, "Draw Arc");
    stack.push(std::move(cmd));

    check(doc.entities().size() == 1, "Arc entity added");
    auto* arcPtr = dynamic_cast<ArcEntity*>(doc.entities()[0].get());
    check(arcPtr != nullptr, "Entity is ArcEntity");
    check(arcPtr->center().x_ == 0.0 && arcPtr->center().y_ == 0.0, "Center correct");
    check(arcPtr->radius() == 5.0, "Radius correct");
    check(arcPtr->startAngle() == startAngle, "Start angle correct");
    check(arcPtr->endAngle() == endAngle, "End angle correct");

    // Undo
    stack.undo();
    check(doc.entities().empty(), "Entity removed after undo");

    // Redo
    stack.redo();
    check(doc.entities().size() == 1, "Entity restored after redo");
    auto* arcPtr2 = dynamic_cast<ArcEntity*>(doc.entities()[0].get());
    check(arcPtr2 != nullptr, "Restored entity is ArcEntity");
    check(arcPtr2->center().x_ == 0.0 && arcPtr2->center().y_ == 0.0, "Center correct after redo");
    check(arcPtr2->radius() == 5.0, "Radius correct after redo");
    check(arcPtr2->startAngle() == startAngle, "Start angle correct after redo");
    check(arcPtr2->endAngle() == endAngle, "End angle correct after redo");
}

void testPolylineCommand_undoRedo() {
    std::printf("\n=== testPolylineCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    std::vector<Point2> points = { Point2(0, 0), Point2(10, 0), Point2(10, 10), Point2(0, 10) };
    auto cmd = std::make_unique<PolylineCommand>(points, "Draw Polyline");
    stack.push(std::move(cmd));

    check(doc.entities().size() == 1, "Polyline entity added");
    auto* polyPtr = dynamic_cast<PolylineEntity*>(doc.entities()[0].get());
    check(polyPtr != nullptr, "Entity is PolylineEntity");
    check(polyPtr->vertices().size() == 4, "4 vertices");
    check(polyPtr->vertices()[0].x_ == 0.0 && polyPtr->vertices()[0].y_ == 0.0, "Vertex 0 correct");
    check(polyPtr->vertices()[3].x_ == 0.0 && polyPtr->vertices()[3].y_ == 10.0, "Vertex 3 correct");

    // Undo
    stack.undo();
    check(doc.entities().empty(), "Entity removed after undo");

    // Redo
    stack.redo();
    check(doc.entities().size() == 1, "Entity restored after redo");
    auto* polyPtr2 = dynamic_cast<PolylineEntity*>(doc.entities()[0].get());
    check(polyPtr2 != nullptr, "Restored entity is PolylineEntity");
    check(polyPtr2->vertices().size() == 4, "4 vertices after redo");
}

// --- Vague 2 Commands Tests ---

void testTrimCommand_undoRedo() {
    std::printf("\n=== testTrimCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    // Create a line to trim
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    int lineId = doc.addEntity(std::move(line))->id();

    // Create a cutting line (vertical at x=5)
    auto cutting = std::make_unique<LineEntity>(Point2(5, -5), Point2(5, 5));
    int cuttingId = doc.addEntity(std::move(cutting))->id();

    auto* linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists");
    check(linePtr->end().x_ == 10.0, "Line initially ends at x=10");

    // Trim the line at the intersection, keeping the part containing pickPoint (8,0)
    auto cmd = std::make_unique<TrimCommand>(lineId, cuttingId, Point2(8, 0), "Trim Line");
    stack.push(std::move(cmd));

    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists after trim");
    check(geom::nearlyEqual(linePtr->end().x_, 5.0), "Line trimmed to x=5");
    check(geom::nearlyEqual(linePtr->end().y_, 0.0), "Line end y=0");

    // Undo
    stack.undo();
    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists after undo");
    check(geom::nearlyEqual(linePtr->end().x_, 10.0), "Line restored to x=10");

    // Redo
    stack.redo();
    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists after redo");
    check(geom::nearlyEqual(linePtr->end().x_, 5.0), "Line re-trimmed to x=5");
}

void testExtendCommand_undoRedo() {
    std::printf("\n=== testExtendCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    // Create a line to extend
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(5, 0));
    int lineId = doc.addEntity(std::move(line))->id();

    // Create a boundary line (vertical at x=10)
    auto boundary = std::make_unique<LineEntity>(Point2(10, -5), Point2(10, 5));
    int boundaryId = doc.addEntity(std::move(boundary))->id();

    auto* linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists");
    check(geom::nearlyEqual(linePtr->end().x_, 5.0), "Line initially ends at x=5");

    // Extend the line to the boundary, pickPoint on the right side
    auto cmd = std::make_unique<ExtendCommand>(lineId, boundaryId, Point2(7, 0), "Extend Line");
    stack.push(std::move(cmd));

    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists after extend");
    check(geom::nearlyEqual(linePtr->end().x_, 10.0), "Line extended to x=10");
    check(geom::nearlyEqual(linePtr->end().y_, 0.0), "Line end y=0");

    // Undo
    stack.undo();
    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists after undo");
    check(geom::nearlyEqual(linePtr->end().x_, 5.0), "Line restored to x=5");

    // Redo
    stack.redo();
    linePtr = dynamic_cast<LineEntity*>(doc.findEntity(lineId));
    check(linePtr != nullptr, "Line entity exists after redo");
    check(geom::nearlyEqual(linePtr->end().x_, 10.0), "Line re-extended to x=10");
}

void testBreakCommand_undoRedo() {
    std::printf("\n=== testBreakCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    // Create a line to break
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    int lineId = doc.addEntity(std::move(line))->id();

    // Break at x=5
    auto cmd = std::make_unique<BreakCommand>(lineId, Point2(5, 0), "Break Line");
    stack.push(std::move(cmd));

    // Original entity should be replaced by two new lines
    auto* original = doc.findEntity(lineId);
    check(original == nullptr, "Original entity removed");

    // Should have two new lines
    check(doc.entities().size() == 2, "Two new lines created");
    int newId1 = -1, newId2 = -1;
    for (const auto& e : doc.entities()) {
        auto* l = dynamic_cast<LineEntity*>(e.get());
        check(l != nullptr, "Entity is LineEntity");
        if (l->start().x_ == 0.0 && l->end().x_ == 5.0) newId1 = l->id();
        else if (l->start().x_ == 5.0 && l->end().x_ == 10.0) newId2 = l->id();
    }
    check(newId1 != -1 && newId2 != -1, "Both half-lines found");

    // Undo
    stack.undo();
    check(doc.entities().size() == 1, "Original line restored after undo");
    // Entity restored with new ID, find it by iterating
    LineEntity* restored = nullptr;
    for (const auto& e : doc.entities()) {
        restored = dynamic_cast<LineEntity*>(e.get());
    }
    check(restored != nullptr, "Original line entity restored");
    check(geom::nearlyEqual(restored->start().x_, 0.0) && geom::nearlyEqual(restored->end().x_, 10.0), "Line restored to 0-10");

    // Redo
    stack.redo();
    check(doc.entities().size() == 2, "Two lines re-created after redo");
}

void testOffsetCommand_undoRedo() {
    std::printf("\n=== testOffsetCommand_undoRedo ===\n");
    core::Document doc;
    CommandStack stack;
    stack.setDocument(doc);

    // Test offset line
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    int lineId = doc.addEntity(std::move(line))->id();

    auto cmd = std::make_unique<OffsetCommand>(lineId, 5.0, Point2(0, 5), "Offset Line"); // offset up
    stack.push(std::move(cmd));

    check(doc.entities().size() == 2, "Original + offset line");
    int offsetId = -1;
    for (const auto& e : doc.entities()) {
        auto* l = dynamic_cast<LineEntity*>(e.get());
        if (l && l->start().y_ == 5.0) {
            offsetId = l->id();
            check(geom::nearlyEqual(l->start().y_, 5.0), "Offset line at y=5");
            check(geom::nearlyEqual(l->end().y_, 5.0), "Offset line at y=5");
        }
    }
    check(offsetId != -1, "Offset line created");

    // Undo
    stack.undo();
    check(doc.entities().size() == 1, "Offset line removed after undo");

    // Redo
    stack.redo();
    check(doc.entities().size() == 2, "Offset line re-created after redo");

    // Test offset circle - use fresh document to avoid interference
    core::Document doc2;
    CommandStack stack2;
    stack2.setDocument(doc2);

    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    int circleId = doc2.addEntity(std::move(circle))->id();

    auto cmd2 = std::make_unique<OffsetCommand>(circleId, 2.0, Point2(0, 10), "Offset Circle"); // outside
    stack2.push(std::move(cmd2));

    check(doc2.entities().size() == 2, "Original + offset circle");
    for (const auto& e : doc2.entities()) {
        auto* c = dynamic_cast<CircleEntity*>(e.get());
        if (c && c->radius() == 7.0) {
            check(c->radius() == 7.0, "Offset circle radius = 7");
        }
    }

    // Undo
    stack2.undo();
    check(doc2.entities().size() == 1, "Offset circle removed after undo");
}

} // namespace

int main() {
    testSetLayerCommand_fullCycle();
    testTransformEntityCommand_merge();
    testRemoveEntityCommand_snapshotCapturedAtExecution();
    testAddEntityCommand_undoRedo();
    testSetColorOverrideCommand_undoRedo();
    testTransaction();

    testLineCommand_undoRedo();
    testCircleCommand_undoRedo();
    testArcCommand_undoRedo();
    testPolylineCommand_undoRedo();

    testTrimCommand_undoRedo();
    testExtendCommand_undoRedo();
    testBreakCommand_undoRedo();
    testOffsetCommand_undoRedo();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll command pattern tests PASSED\n");
    return 0;
}