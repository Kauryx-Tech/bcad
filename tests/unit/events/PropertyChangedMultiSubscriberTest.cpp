// Phase 6 : PropertyChanged multi-abonnés
// Vérifie que l'EventBus notifie plusieurs abonnés pour le même événement PropertyChanged.

#include "bcad/events/EventBus.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Point.h"
#include "bcad/properties/PropertyMap.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace bcad;
using namespace bcad::events;
using namespace bcad::geom;
using namespace bcad::properties;

int main() {
    std::cout << "=== PropertyChanged multi-subscriber test ===\n";

    // Setup
    core::Document doc;
    EventBus& bus = EventBus::instance();
    bus.clear();

    // Créer une entité avec une propriété
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    int entityId = doc.addEntity(std::move(line))->id();

    // Compteurs d'appels pour chaque abonné
    int subscriber1Count = 0;
    int subscriber2Count = 0;
    int subscriber3Count = 0;
    std::string lastPropName1, lastPropName2, lastPropName3;
    PropertyValue lastOldVal1, lastNewVal1;
    PropertyValue lastOldVal2, lastNewVal2;
    PropertyValue lastOldVal3, lastNewVal3;

    // Abonné 1 : filtre sur l'entité
    auto sub1 = bus.subscribe<PropertyChanged>(
        [&](const PropertyChanged& e) {
            ++subscriber1Count;
            lastPropName1 = e.propertyName;
            lastOldVal1 = e.oldValue;
            lastNewVal1 = e.newValue;
        },
        EventFilter{.entityIds = {entityId}}
    );

    // Abonné 2 : filtre sur le type d'entité
    auto sub2 = bus.subscribe<PropertyChanged>(
        [&](const PropertyChanged& e) {
            ++subscriber2Count;
            lastPropName2 = e.propertyName;
            lastOldVal2 = e.oldValue;
            lastNewVal2 = e.newValue;
        },
        EventFilter{.entityTypes = {TypeId_Line}}
    );

    // Abonné 3 : sans filtre (reçoit tout)
    auto sub3 = bus.subscribe<PropertyChanged>(
        [&](const PropertyChanged& e) {
            ++subscriber3Count;
            lastPropName3 = e.propertyName;
            lastOldVal3 = e.oldValue;
            lastNewVal3 = e.newValue;
        }
    );

    // Changer une propriété via PropertyMap (simule ce que fait SetEntityPropertyCommand)
    auto* entity = doc.findEntity(entityId);
    auto& props = entity->properties();
    props.addDouble("length", 10.0);  // propriété initiale

    // Publier un PropertyChanged manuellement (comme le fait SetEntityPropertyCommand)
    PropertyValue oldVal = PropertyValue(10.0);
    PropertyValue newVal = PropertyValue(15.0);
    bus.publish(PropertyChanged(&doc, entity, "length", oldVal, newVal));

    // Vérifier que les 3 abonnés ont été notifiés
    assert(subscriber1Count == 1 && "Subscriber 1 (entity filter) should receive 1 event");
    assert(subscriber2Count == 1 && "Subscriber 2 (type filter) should receive 1 event");
    assert(subscriber3Count == 1 && "Subscriber 3 (no filter) should receive 1 event");

    assert(lastPropName1 == "length" && "Property name correct for sub1");
    assert(lastPropName2 == "length" && "Property name correct for sub2");
    assert(lastPropName3 == "length" && "Property name correct for sub3");

    // Vérifier les valeurs
    auto getDouble = [](const PropertyValue& v) -> double {
        if (auto* d = std::get_if<double>(&v)) return *d;
        return 0.0;
    };
    assert(getDouble(lastOldVal1) == 10.0 && "Old value correct");
    assert(getDouble(lastNewVal1) == 15.0 && "New value correct");

    // Deuxième changement de propriété
    oldVal = PropertyValue(15.0);
    newVal = PropertyValue(20.0);
    bus.publish(PropertyChanged(&doc, entity, "length", oldVal, newVal));

    assert(subscriber1Count == 2 && "Subscriber 1 should receive 2nd event");
    assert(subscriber2Count == 2 && "Subscriber 2 should receive 2nd event");
    assert(subscriber3Count == 2 && "Subscriber 3 should receive 2nd event");

    // Test de désabonnement
    bus.unsubscribe(sub2);
    oldVal = PropertyValue(20.0);
    newVal = PropertyValue(25.0);
    bus.publish(PropertyChanged(&doc, entity, "length", oldVal, newVal));

    assert(subscriber1Count == 3 && "Subscriber 1 should receive 3rd event");
    assert(subscriber2Count == 2 && "Subscriber 2 should NOT receive after unsubscribe");
    assert(subscriber3Count == 3 && "Subscriber 3 should receive 3rd event");

    // Test avec une autre entité (ne devrait pas déclencher sub1 qui filtre sur entityId)
    // TypeId_Line != TypeId_Circle, donc sub2 ne devrait pas recevoir non plus
    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    int circleId = doc.addEntity(std::move(circle))->id();
    auto* circleEntity = doc.findEntity(circleId);

    oldVal = PropertyValue(0.0);
    newVal = PropertyValue(100.0);
    bus.publish(PropertyChanged(&doc, circleEntity, "radius", oldVal, newVal));

    assert(subscriber1Count == 3 && "Subscriber 1 (entity filter) should NOT receive circle event");
    assert(subscriber2Count == 2 && "Subscriber 2 (type filter: TypeId_Line) should NOT receive circle event (TypeId_Circle)");
    assert(subscriber3Count == 4 && "Subscriber 3 (no filter) should receive circle event");

    // Nettoyage
    bus.unsubscribe(sub1);
    bus.unsubscribe(sub3);

    std::cout << "All multi-subscriber tests PASSED\n";
    return 0;
}