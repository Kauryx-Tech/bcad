# Système d'événements BCAD

> Bus d'événements découplé. Les plugins s'abonnent sans modifier le Core.

## 1. Principe

Le Core expose un bus d'événements typé. Toute modification du Document émet un événement. Les plugins s'abonnent via une interface publique.

## 2. Architecture

```cpp
namespace bcad::events {

class Event {
public:
    virtual ~Event() = default;
    virtual std::string name() const = 0;     // "EntityAdded", ...
    virtual std::chrono::system_clock::time_point timestamp() const;
};

class EventBus {
public:
    // Abonnement typé
    template<typename EventT, typename HandlerT>
    SubscriptionId subscribe(HandlerT&& handler);

    // Désabonnement
    void unsubscribe(SubscriptionId id);
    void unsubscribeAll();

    // Émission
    template<typename EventT>
    void publish(const EventT& event);

    // Traitement différé
    void flush();
    bool isEmpty() const;
    size_t queueSize() const;
};

}
```

## 3. Événements standards

```cpp
namespace bcad::events {

struct EntityAddedEvent : public Event {
    static constexpr const char* kName = "EntityAdded";
    document::EntityId entityId;
    document::TypeId typeId;
};

struct EntityRemovedEvent : public Event {
    static constexpr const char* kName = "EntityRemoved";
    document::EntityId entityId;
};

struct EntityModifiedEvent : public Event {
    static constexpr const char* kName = "EntityModified";
    document::EntityId entityId;
    geom::BoundingBox3 oldBounds;
    geom::BoundingBox3 newBounds;
};

struct PropertyChangedEvent : public Event {
    static constexpr const char* kName = "PropertyChanged";
    document::EntityId entityId;
    std::string propertyName;
    std::any oldValue;
    std::any newValue;
};

struct LayerAddedEvent : public Event { static constexpr const char* kName = "LayerAdded"; ... };
struct LayerRemovedEvent : public Event { ... };
struct LayerModifiedEvent : public Event { ... };

struct SelectionChangedEvent : public Event { ... };

struct CommandStartedEvent : public Event { ... };
struct CommandFinishedEvent : public Event { ... };

struct TransactionCommittedEvent : public Event { ... };
struct TransactionRolledBackEvent : public Event { ... };

struct DocumentChangedEvent : public Event { ... };
struct DocumentSavedEvent : public Event { ... };
struct DocumentLoadedEvent : public Event { ... };

}
```

## 4. Abonnement depuis un plugin

```cpp
extern "C" void bcad_plugin_init(PluginRegistry& reg) {
    auto& bus = reg.eventBus();

    bus.subscribe<events::EntityAddedEvent>([&](const auto& e) {
        if (e.typeId.toString() == "architecture:wall") {
            // Réaction à l'ajout d'un mur
        }
    });

    bus.subscribe<events::PropertyChangedEvent>([&](const auto& e) {
        if (e.propertyName == "fireRating") {
            // Recalculer les charges thermiques
        }
    });
}
```

## 5. File d'événements

Les événements peuvent être émis en mode synchrone ou asynchrone :

```cpp
enum class EventMode {
    Sync,        // Handler appelé immédiatement
    Queued,      // Mis en file, traité sur flush()
    AsyncThread  // Sur un thread dédié
};

class EventBus {
public:
    void setMode(EventMode mode);
    EventMode mode() const;
};
```

## 6. Performance

- **Pas de hiérarchie d'événements lourde** : un Event est léger (POD-like)
- **Pas d'allocation par event** : la plupart des événements sont des structs avec données inline
- **Dispatch O(1)** : la table de hash des handlers typés
- **Pas de hiérarchie de priorités** : tous les handlers sont appelés dans l'ordre d'abonnement

## 7. Filtres

```cpp
// Filtre par type d'entité
auto id = bus.subscribe<events::EntityAddedEvent>(
    [&](const auto& e) { /* ... */ },
    [](const auto& e) { return e.typeId.namespace_ == "architecture"; }
);
```

## 8. Découplage avec Qt

Le Core expose un bus pur C++. L'application Qt peut connecter les signaux Qt au bus Core via un adaptateur :

```cpp
// app/EventBridge.cpp
void EventBridge::connectToQt(events::EventBus& bus) {
    bus.subscribe<events::EntityAddedEvent>([this](const auto& e) {
        emit entityAdded(toQt(e.entityId));
    });
}
```

## 9. Règles

1. Le bus est pur C++, pas de signal/slot Qt
2. Les événements sont typés (template EventT)
3. Les plugins s'abonnent via l'API publique
4. Les handlers sont des std::function
5. Le mode synchrone/async est configurable
6. Pas de hiérarchie d'événements trop profonde