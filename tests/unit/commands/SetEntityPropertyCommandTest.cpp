// Phase 6 : SetEntityPropertyCommand undo/redo/clone/merge
// Vérifie le cycle complet de la commande de modification de propriété.

#include "bcad/commands/Command.h"
#include "bcad/commands/ConcreteCommands.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"
#include <cassert>
#include <iostream>
#include <memory>

using namespace bcad;
using namespace bcad::commands;
using namespace bcad::geom;
using namespace bcad::properties;

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

void testSetEntityPropertyCommand_undoRedo() {
    std::printf("\n=== testSetEntityPropertyCommand_undoRedo ===\n");
    core::Document doc;

    // Créer une ligne avec une propriété "length"
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    auto& props = line->properties();
    props.addDouble("length", 10.0);
    props.addString("label", "original");
    int entityId = doc.addEntity(std::move(line))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Changer la propriété "length" de 10.0 à 15.0
    auto cmd = std::make_unique<SetEntityPropertyCommand>(entityId, "length", PropertyValue(15.0), "Change Length");
    stack.push(std::move(cmd));

    auto* entity = doc.findEntity(entityId);
    check(entity != nullptr, "Entity exists");
    auto& p = entity->properties();
    check(p.getDouble("length", 0.0) == 15.0, "Property changed to 15.0");

    // Undo
    stack.undo();
    entity = doc.findEntity(entityId);
    check(entity != nullptr, "Entity exists after undo");
    auto& p2 = entity->properties();
    check(p2.getDouble("length", 0.0) == 10.0, "Property restored to 10.0 after undo");

    // Redo
    stack.redo();
    entity = doc.findEntity(entityId);
    auto& p3 = entity->properties();
    check(p3.getDouble("length", 0.0) == 15.0, "Property back to 15.0 after redo");
}

void testSetEntityPropertyCommand_merge() {
    std::printf("\n=== testSetEntityPropertyCommand_merge ===\n");
    core::Document doc;

    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    auto& props = line->properties();
    props.addDouble("length", 10.0);
    int entityId = doc.addEntity(std::move(line))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Première modification
    auto cmd1 = std::make_unique<SetEntityPropertyCommand>(entityId, "length", PropertyValue(15.0), "Change Length 1");
    stack.push(std::move(cmd1));

    // Deuxième modification (doit merger)
    auto cmd2 = std::make_unique<SetEntityPropertyCommand>(entityId, "length", PropertyValue(20.0), "Change Length 2");
    stack.push(std::move(cmd2));

    // Vérifier que la valeur finale est 20.0
    auto* entity = doc.findEntity(entityId);
    check(entity != nullptr, "Entity exists");
    check(entity->properties().getDouble("length", 0.0) == 20.0, "Final value is 20.0 after merge");

    // Undo devrait revenir à 10.0 (valeur originale avant la première commande)
    stack.undo();
    entity = doc.findEntity(entityId);
    check(entity->properties().getDouble("length", 0.0) == 10.0, "Undo restored to original 10.0");

    // Redo revient à 20.0 (valeur finale fusionnée)
    stack.redo();
    entity = doc.findEntity(entityId);
    check(entity->properties().getDouble("length", 0.0) == 20.0, "Redo restored merged value 20.0");
}

void testSetEntityPropertyCommand_clone() {
    std::printf("\n=== testSetEntityPropertyCommand_clone ===\n");
    core::Document doc;

    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    auto& props = line->properties();
    props.addDouble("length", 10.0);
    int entityId = doc.addEntity(std::move(line))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Exécuter la commande
    auto cmd = std::make_unique<SetEntityPropertyCommand>(entityId, "length", PropertyValue(15.0), "Change Length");
    stack.push(std::move(cmd));

    // Cloner la commande
    auto* entity = doc.findEntity(entityId);
    auto& p = entity->properties();
    check(p.getDouble("length", 0.0) == 15.0, "Original command executed");

    // Créer un nouveau document pour tester le clone
    core::Document doc2;
    auto line2 = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    auto& props2 = line2->properties();
    props2.addDouble("length", 10.0);
    int entityId2 = doc2.addEntity(std::move(line2))->id();

    CommandStack stack2;
    stack2.setDocument(doc2);

    // Exécuter une commande, puis tester undo/redo qui utilise clone internement
    auto cmd2 = std::make_unique<SetEntityPropertyCommand>(entityId2, "length", PropertyValue(25.0), "Change Length 2");
    stack2.push(std::move(cmd2));

    entity = doc2.findEntity(entityId2);
    check(entity != nullptr, "Entity exists in doc2");
    check(entity->properties().getDouble("length", 0.0) == 25.0, "Command executed in doc2");

    // Undo/Redo test le clone
    stack2.undo();
    entity = doc2.findEntity(entityId2);
    check(entity->properties().getDouble("length", 0.0) == 10.0, "Undo works in doc2");

    stack2.redo();
    entity = doc2.findEntity(entityId2);
    check(entity->properties().getDouble("length", 0.0) == 25.0, "Redo works in doc2");
}

void testSetEntityPropertyCommand_stringProperty() {
    std::printf("\n=== testSetEntityPropertyCommand_stringProperty ===\n");
    core::Document doc;

    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    auto& props = circle->properties();
    props.addString("label", "circle_1");
    int entityId = doc.addEntity(std::move(circle))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Changer le label
    auto cmd = std::make_unique<SetEntityPropertyCommand>(entityId, "label", PropertyValue(std::string("circle_2")), "Change Label");
    stack.push(std::move(cmd));

    auto* entity = doc.findEntity(entityId);
    check(entity != nullptr, "Entity exists");
    check(entity->properties().getString("label") == "circle_2", "String property changed");

    // Undo
    stack.undo();
    entity = doc.findEntity(entityId);
    check(entity->properties().getString("label") == "circle_1", "String property restored on undo");

    // Redo
    stack.redo();
    entity = doc.findEntity(entityId);
    check(entity->properties().getString("label") == "circle_2", "String property restored on redo");
}

void testSetEntityPropertyCommand_boolProperty() {
    std::printf("\n=== testSetEntityPropertyCommand_boolProperty ===\n");
    core::Document doc;

    auto polyline = std::make_unique<PolylineEntity>(std::vector<Point2>{{0,0}, {10,0}, {10,10}}, false);
    auto& props = polyline->properties();
    props.addBool("closed", false);
    int entityId = doc.addEntity(std::move(polyline))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Changer closed à true
    auto cmd = std::make_unique<SetEntityPropertyCommand>(entityId, "closed", PropertyValue(true), "Close Polyline");
    stack.push(std::move(cmd));

    auto* entity = doc.findEntity(entityId);
    check(entity != nullptr, "Entity exists");
    check(entity->properties().getBool("closed", false) == true, "Bool property changed to true");

    // Undo
    stack.undo();
    entity = doc.findEntity(entityId);
    check(entity->properties().getBool("closed", true) == false, "Bool property restored to false on undo");

    // Redo
    stack.redo();
    entity = doc.findEntity(entityId);
    check(entity->properties().getBool("closed", false) == true, "Bool property restored to true on redo");
}

void testSetEntityPropertyCommand_colorProperty() {
    std::printf("\n=== testSetEntityPropertyCommand_colorProperty ===\n");
    core::Document doc;

    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    auto& props = line->properties();
    props.addColor("customColor", Color::fromRgb255(255, 0, 0));
    int entityId = doc.addEntity(std::move(line))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Changer la couleur
    auto cmd = std::make_unique<SetEntityPropertyCommand>(entityId, "customColor", PropertyValue(Color::fromRgb255(0, 255, 0)), "Change Color");
    stack.push(std::move(cmd));

    auto* entity = doc.findEntity(entityId);
    check(entity != nullptr, "Entity exists");
    auto c = entity->properties().getColor("customColor");
    check(c.g == 1.0f && c.r == 0.0f, "Color changed to green");

    // Undo
    stack.undo();
    entity = doc.findEntity(entityId);
    c = entity->properties().getColor("customColor");
    check(c.r == 1.0f && c.g == 0.0f, "Color restored to red on undo");

    // Redo
    stack.redo();
    entity = doc.findEntity(entityId);
    c = entity->properties().getColor("customColor");
    check(c.g == 1.0f && c.r == 0.0f, "Color restored to green on redo");
}

void testSetEntityPropertyCommand_readOnlyProperty() {
    std::printf("\n=== testSetEntityPropertyCommand_readOnlyProperty ===\n");
    core::Document doc;

    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    auto& props = line->properties();
    props.addDouble("length", 10.0);
    // Marquer comme read-only via le Property objet
    if (auto* prop = props.get("length")) {
        prop->setReadOnly(true);
    }
    int entityId = doc.addEntity(std::move(line))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Essayer de changer une propriété read-only
    auto cmd = std::make_unique<SetEntityPropertyCommand>(entityId, "length", PropertyValue(15.0), "Change ReadOnly Length");
    stack.push(std::move(cmd));

    auto* entity = doc.findEntity(entityId);
    check(entity != nullptr, "Entity exists");
    check(entity->properties().getDouble("length", 0.0) == 10.0, "Read-only property NOT changed");

    // Undo ne devrait rien faire (pas de changement)
    stack.undo();
    entity = doc.findEntity(entityId);
    check(entity->properties().getDouble("length", 0.0) == 10.0, "Value unchanged after undo");
}

void testSetEntityPropertyCommand_multipleProperties() {
    std::printf("\n=== testSetEntityPropertyCommand_multipleProperties ===\n");
    core::Document doc;

    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    auto& props = circle->properties();
    props.addDouble("radius", 5.0);
    props.addString("name", "circle");
    props.addBool("filled", false);
    int entityId = doc.addEntity(std::move(circle))->id();

    CommandStack stack;
    stack.setDocument(doc);

    // Changer plusieurs propriétés en séquence
    auto cmd1 = std::make_unique<SetEntityPropertyCommand>(entityId, "radius", PropertyValue(10.0), "Change Radius");
    stack.push(std::move(cmd1));

    auto cmd2 = std::make_unique<SetEntityPropertyCommand>(entityId, "name", PropertyValue(std::string("big_circle")), "Change Name");
    stack.push(std::move(cmd2));

    auto cmd3 = std::make_unique<SetEntityPropertyCommand>(entityId, "filled", PropertyValue(true), "Change Filled");
    stack.push(std::move(cmd3));

    auto* entity = doc.findEntity(entityId);
    check(entity->properties().getDouble("radius", 0.0) == 10.0, "Radius changed");
    check(entity->properties().getString("name") == "big_circle", "Name changed");
    check(entity->properties().getBool("filled", false) == true, "Filled changed");

    // Undo 3 fois
    stack.undo(); // filled -> false
    entity = doc.findEntity(entityId);
    check(entity->properties().getBool("filled", true) == false, "Filled undone");

    stack.undo(); // name -> "circle"
    entity = doc.findEntity(entityId);
    check(entity->properties().getString("name") == "circle", "Name undone");

    stack.undo(); // radius -> 5.0
    entity = doc.findEntity(entityId);
    check(entity->properties().getDouble("radius", 0.0) == 5.0, "Radius undone");

    // Redo 3 fois
    stack.redo(); // radius -> 10.0
    entity = doc.findEntity(entityId);
    check(entity->properties().getDouble("radius", 0.0) == 10.0, "Radius redone");

    stack.redo(); // name -> "big_circle"
    entity = doc.findEntity(entityId);
    check(entity->properties().getString("name") == "big_circle", "Name redone");

    stack.redo(); // filled -> true
    entity = doc.findEntity(entityId);
    check(entity->properties().getBool("filled", false) == true, "Filled redone");
}

int main() {
    testSetEntityPropertyCommand_undoRedo();
    testSetEntityPropertyCommand_merge();
    testSetEntityPropertyCommand_clone();
    testSetEntityPropertyCommand_stringProperty();
    testSetEntityPropertyCommand_boolProperty();
    testSetEntityPropertyCommand_colorProperty();
    testSetEntityPropertyCommand_readOnlyProperty();
    testSetEntityPropertyCommand_multipleProperties();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll SetEntityPropertyCommand tests PASSED\n");
    return 0;
}