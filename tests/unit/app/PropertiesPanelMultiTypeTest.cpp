// Phase 6 : PropertiesPanel multi-type + sélection multiple
// Tests unitaires de la logique de reconstruction des éditeurs de propriétés
// (sans interface graphique complète).

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::geom;
using namespace bcad::properties;

// Vérifie que PropertyMap supporte tous les types pour rebuildPropertyEditors
void testPropertyMapAllTypes() {
    std::cout << "=== testPropertyMapAllTypes ===\n";

    PropertyMap pm;

    // String
    pm.addString("name", "test");
    if (auto* prop = pm.get("name")) {
        prop->setUnit("");
        prop->setDescription("Name");
        prop->setReadOnly(false);
    }
    assert(pm.getString("name") == "test");
    pm.setString("name", "updated");
    assert(pm.getString("name") == "updated");

    // Enum
    pm.addEnum("status", 1, {"Draft", "Approved", "Archived"});
    if (auto* prop = pm.get("status")) {
        prop->setUnit("");
        prop->setDescription("Status");
        prop->setReadOnly(false);
    }
    assert(pm.getEnum("status") == 1);
    pm.setEnum("status", 2);
    assert(pm.getEnum("status") == 2);

    // Double avec range
    pm.addDouble("length", 10.0);
    if (auto* prop = pm.get("length")) {
        prop->setUnit("m");
        prop->setDescription("Length");
        prop->setReadOnly(false);
        prop->setRange(0.0, 100.0);
    }
    assert(pm.getDouble("length", 0.0) == 10.0);
    pm.setDouble("length", 15.5);
    assert(pm.getDouble("length", 0.0) == 15.5);

    // Int avec range
    pm.addInt("count", 5);
    if (auto* prop = pm.get("count")) {
        prop->setUnit("");
        prop->setDescription("Count");
        prop->setReadOnly(false);
        prop->setRange(0.0, 100.0);
    }
    assert(pm.getInt("count", 0) == 5);
    pm.setInt("count", 10);
    assert(pm.getInt("count", 0) == 10);

    // Bool
    pm.addBool("visible", true);
    if (auto* prop = pm.get("visible")) {
        prop->setUnit("");
        prop->setDescription("Visible");
        prop->setReadOnly(false);
    }
    assert(pm.getBool("visible", false) == true);
    pm.setBool("visible", false);
    assert(pm.getBool("visible", true) == false);

    // Color
    pm.addColor("customColor", Color::fromRgb255(255, 0, 0));
    if (auto* prop = pm.get("customColor")) {
        prop->setUnit("");
        prop->setDescription("Custom Color");
        prop->setReadOnly(false);
    }
    auto c = pm.getColor("customColor");
    assert(c.r == 1.0f && c.g == 0.0f);
    pm.setColor("customColor", Color::fromRgb255(0, 255, 0));
    c = pm.getColor("customColor");
    assert(c.g == 1.0f && c.r == 0.0f);

    // Read-only
    pm.addDouble("area", 50.0);
    if (auto* prop = pm.get("area")) {
        prop->setUnit("m²");
        prop->setDescription("Area");
        prop->setReadOnly(true);
    }
    assert(pm.get("area")->isReadOnly() == true);

    // Liste des noms triée
    auto names = pm.listNames();
    std::sort(names.begin(), names.end());
    std::vector<std::string> expected = {"area", "count", "customColor", "length", "name", "status", "visible"};
    assert(names == expected);

    std::cout << "PropertyMap all types test PASSED\n";
}

// Simule la logique de sélection multiple du PropertiesPanel
void testMultiSelectionLogic() {
    std::cout << "\n=== testMultiSelectionLogic ===\n";

    core::Document doc;
    doc.layerManager().createLayer("Layer1", Color::fromRgb255(255, 0, 0));
    doc.layerManager().createLayer("Layer2", Color::fromRgb255(0, 255, 0));

    // Créer des entités sur différents calques
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    line->setLayer("Layer1");
    int lineId = doc.addEntity(std::move(line))->id();

    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    circle->setLayer("Layer2");
    int circleId = doc.addEntity(std::move(circle))->id();

    // Sélectionner les deux
    auto* lineEnt = doc.findEntity(lineId);
    auto* circleEnt = doc.findEntity(circleId);
    lineEnt->selected = true;
    circleEnt->selected = true;

    // Logique de détection calques mixtes (copiée de PropertiesPanel::refresh)
    std::vector<geom::Entity*> selected = {lineEnt, circleEnt};
    std::string firstLayer = selected.front()->layer();
    bool mixedLayer = false;
    for (geom::Entity* e : selected) {
        if (e->layer() != firstLayer) {
            mixedLayer = true;
            break;
        }
    }
    assert(mixedLayer == true);

    // Logique de détection couleurs mixtes
    auto colorEquals = [](const std::optional<Color>& a, const std::optional<Color>& b) -> bool {
        if (a.has_value() != b.has_value()) return false;
        if (!a) return true;
        return a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
    };

    std::optional<Color> firstOverride = selected.front()->colorOverride();
    bool mixedColor = false;
    for (geom::Entity* e : selected) {
        if (!colorEquals(e->colorOverride(), firstOverride)) {
            mixedColor = true;
            break;
        }
    }
    // Les deux n'ont pas d'override -> mixedColor = false (c'est "Par calque" pour les deux)
    assert(mixedColor == false);

    // Maintenant donner un override différent à l'un
    lineEnt->setColorOverride(Color::fromRgb255(255, 0, 0));
    firstOverride = selected.front()->colorOverride();
    mixedColor = false;
    for (geom::Entity* e : selected) {
        if (!colorEquals(e->colorOverride(), firstOverride)) {
            mixedColor = true;
            break;
        }
    }
    assert(mixedColor == true);

    // Test avec même calque, même override
    circleEnt->setColorOverride(Color::fromRgb255(255, 0, 0));
    lineEnt->setLayer("Layer1");
    circleEnt->setLayer("Layer1");
    mixedLayer = false;
    firstLayer = selected.front()->layer();
    for (geom::Entity* e : selected) {
        if (e->layer() != firstLayer) {
            mixedLayer = true;
            break;
        }
    }
    assert(mixedLayer == false);

    firstOverride = selected.front()->colorOverride();
    mixedColor = false;
    for (geom::Entity* e : selected) {
        if (!colorEquals(e->colorOverride(), firstOverride)) {
            mixedColor = true;
            break;
        }
    }
    assert(mixedColor == false);

    std::cout << "Multi-selection logic test PASSED\n";
}

// Test des entités avec différents types de propriétés
void testEntityPropertyVariation() {
    std::cout << "\n=== testEntityPropertyVariation ===\n";

    core::Document doc;

    // Line : propriétés géométriques + custom
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    line->properties().addDouble("customLength", 10.0);
    line->properties().addString("label", "line1");
    int lineId = doc.addEntity(std::move(line))->id();

    // Circle : propriétés géométriques + custom
    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    circle->properties().addDouble("radius", 5.0);
    circle->properties().addBool("filled", false);
    int circleId = doc.addEntity(std::move(circle))->id();

    // Polyline : propriétés géométriques + custom
    auto poly = std::make_unique<PolylineEntity>(std::vector<Point2>{{0,0}, {10,0}, {10,10}}, false);
    poly->properties().addInt("vertexCount", 3);
    poly->properties().addEnum("type", 0, {"Boundary", "Construction", "Annotation"});
    int polyId = doc.addEntity(std::move(poly))->id();

    // Vérifier que chaque entité a ses propriétés
    auto* lineEnt = doc.findEntity(lineId);
    assert(lineEnt->properties().has("customLength"));
    assert(lineEnt->properties().has("label"));
    assert(lineEnt->properties().getDouble("customLength", 0.0) == 10.0);

    auto* circleEnt = doc.findEntity(circleId);
    assert(circleEnt->properties().has("radius"));
    assert(circleEnt->properties().has("filled"));
    assert(circleEnt->properties().getDouble("radius", 0.0) == 5.0);
    assert(circleEnt->properties().getBool("filled", true) == false);

    auto* polyEnt = doc.findEntity(polyId);
    assert(polyEnt->properties().has("vertexCount"));
    assert(polyEnt->properties().has("type"));
    assert(polyEnt->properties().getInt("vertexCount", 0) == 3);
    assert(polyEnt->properties().getEnum("type") == 0);

    // Sélection unique -> rebuildPropertyEditors affiche les propriétés
    // (logique testée via PropertyMap ci-dessus)

    std::cout << "Entity property variation test PASSED\n";
}

// Test que PropertyChanged est publié pour chaque type
void testPropertyChangedForAllTypes() {
    std::cout << "\n=== testPropertyChangedForAllTypes ===\n";

    core::Document doc;
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    line->properties().addString("name", "original");
    line->properties().addDouble("length", 10.0);
    line->properties().addBool("active", true);
    int entityId = doc.addEntity(std::move(line))->id();

    auto* entity = doc.findEntity(entityId);
    events::EventBus& bus = events::EventBus::instance();
    bus.clear();

    int eventCount = 0;
    std::string lastName;
    PropertyValue lastOld, lastNew;

    auto sub = bus.subscribe<events::PropertyChanged>(
        [&](const events::PropertyChanged& e) {
            ++eventCount;
            lastName = e.propertyName;
            lastOld = e.oldValue;
            lastNew = e.newValue;
        }
    );

    // String
    bus.publish(events::PropertyChanged(&doc, entity, "name", PropertyValue("original"), PropertyValue("updated")));
    assert(eventCount == 1);
    assert(lastName == "name");
    assert(std::get<std::string>(lastOld) == "original");
    assert(std::get<std::string>(lastNew) == "updated");

    // Double
    bus.publish(events::PropertyChanged(&doc, entity, "length", PropertyValue(10.0), PropertyValue(15.0)));
    assert(eventCount == 2);
    assert(lastName == "length");
    assert(std::get<double>(lastOld) == 10.0);
    assert(std::get<double>(lastNew) == 15.0);

    // Bool
    bus.publish(events::PropertyChanged(&doc, entity, "active", PropertyValue(true), PropertyValue(false)));
    assert(eventCount == 3);
    assert(lastName == "active");
    assert(std::get<bool>(lastOld) == true);
    assert(std::get<bool>(lastNew) == false);

    bus.unsubscribe(sub);
    std::cout << "PropertyChanged for all types test PASSED\n";
}

int main() {
    testPropertyMapAllTypes();
    testMultiSelectionLogic();
    testEntityPropertyVariation();
    testPropertyChangedForAllTypes();

    std::cout << "\nAll PropertiesPanel multi-type / multi-selection tests PASSED\n";
    return 0;
}