#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/geometry/Point.h"
#include "bcad/properties/PropertyTypes.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>

namespace bcad::core {
class Document;
}

namespace bcad::properties {
class PropertyMap;
}

namespace bcad::events {

// Base event class
struct Event {
    virtual ~Event() = default;
};

// Document events
struct DocumentChanged : Event {
    enum class ChangeType { EntityAdded, EntityRemoved, EntityModified, Cleared };
    ChangeType type;
    core::Document* document;
    geom::Entity* entity = nullptr;
    geom::TypeId entityTypeId;
};

struct EntityAdded : Event {
    core::Document* document;
    geom::Entity* entity;

    explicit EntityAdded(core::Document* doc, geom::Entity* ent) : document(doc), entity(ent) {}
};

struct EntityRemoved : Event {
    core::Document* document;
    int entityId;
    geom::TypeId entityTypeId;

    EntityRemoved(core::Document* doc, int id, geom::TypeId typeId)
        : document(doc), entityId(id), entityTypeId(typeId) {}
};

struct EntityModified : Event {
    core::Document* document;
    geom::Entity* entity;
    geom::TypeId entityTypeId;

    EntityModified(core::Document* doc, geom::Entity* ent)
        : document(doc), entity(ent), entityTypeId(ent ? ent->typeId() : geom::TypeId{}) {}
};

struct DocumentCleared : Event {
    core::Document* document;

    explicit DocumentCleared(core::Document* doc) : document(doc) {}
};

// Layer events
struct LayerAdded : Event {
    core::Document* document;
    std::string layerName;
};

struct LayerRemoved : Event {
    core::Document* document;
    std::string layerName;
};

struct LayerModified : Event {
    core::Document* document;
    std::string layerName;
};

// Selection events
struct SelectionChanged : Event {
    core::Document* document;
    std::vector<geom::Entity*> selectedEntities;
};

struct EntitySelected : Event {
    core::Document* document;
    geom::Entity* entity;
};

struct EntityDeselected : Event {
    core::Document* document;
    geom::Entity* entity;
};

// Viewport/Tool events
struct ToolChanged : Event {
    enum class Tool { Select, Line, Circle, Arc, Polyline, Move, Copy, Rotate, Trim, Extend };
    Tool tool;
};

struct ViewportPan : Event {
    double dx, dy;
};

struct ViewportZoom : Event {
    double factor;
    geom::Point2 pivot;
};

struct ViewportFit : Event {
    geom::BoundingBox bounds;
};

// Property events
struct PropertyChanged : Event {
    core::Document* document;
    geom::Entity* entity;
    std::string propertyName;
    bcad::properties::PropertyValue oldValue;
    bcad::properties::PropertyValue newValue;

    PropertyChanged(core::Document* doc, geom::Entity* ent, std::string_view name, 
                    const bcad::properties::PropertyValue& oldVal, 
                    const bcad::properties::PropertyValue& newVal)
        : document(doc), entity(ent), propertyName(name), oldValue(oldVal), newValue(newVal) {}
};

struct PropertyBatchChanged : Event {
    core::Document* document;
    geom::Entity* entity;
    std::vector<std::string> propertyNames;
};

// Filter for event subscriptions
struct EventFilter {
    // Optional: only receive events for specific document
    core::Document* document = nullptr;

    // Optional: only receive events for specific entity types
    std::vector<geom::TypeId> entityTypes;

    // Optional: only receive events for specific entity IDs
    std::vector<int> entityIds;

    // Optional: only receive events for specific layer
    std::string layerName;

    bool matches(const Event& event) const {
        // Check document filter
        if (document) {
            const core::Document* eventDoc = getDocument(event);
            if (eventDoc != document) return false;
        }

        // Check entity type filter
        if (!entityTypes.empty()) {
            geom::TypeId eventTypeId = getEntityTypeId(event);
            if (!eventTypeId || 
                std::find(entityTypes.begin(), entityTypes.end(), eventTypeId) == entityTypes.end()) {
                return false;
            }
        }

        // Check entity ID filter
        if (!entityIds.empty()) {
            int eventEntityId = getEntityId(event);
            if (eventEntityId < 0 || 
                std::find(entityIds.begin(), entityIds.end(), eventEntityId) == entityIds.end()) {
                return false;
            }
        }

        // Check layer filter
        if (!layerName.empty()) {
            std::string eventLayer = getLayerName(event);
            if (eventLayer != layerName) return false;
        }

        return true;
    }

private:
    static const core::Document* getDocument(const Event& event) {
        if (auto* e = dynamic_cast<const DocumentChanged*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const EntityAdded*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const EntityRemoved*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const EntityModified*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const DocumentCleared*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const LayerAdded*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const LayerRemoved*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const LayerModified*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const SelectionChanged*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const EntitySelected*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const EntityDeselected*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const PropertyChanged*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const PropertyBatchChanged*>(&event)) return e->document;
        if (auto* e = dynamic_cast<const ToolChanged*>(&event)) return nullptr; // Global
        if (auto* e = dynamic_cast<const ViewportPan*>(&event)) return nullptr; // Global
        if (auto* e = dynamic_cast<const ViewportZoom*>(&event)) return nullptr; // Global
        if (auto* e = dynamic_cast<const ViewportFit*>(&event)) return nullptr; // Global
        return nullptr;
    }

    static geom::TypeId getEntityTypeId(const Event& event) {
        if (auto* e = dynamic_cast<const EntityAdded*>(&event)) return e->entity ? e->entity->typeId() : geom::TypeId{};
        if (auto* e = dynamic_cast<const EntityRemoved*>(&event)) return e->entityTypeId;
        if (auto* e = dynamic_cast<const EntityModified*>(&event)) return e->entityTypeId;
        if (auto* e = dynamic_cast<const EntitySelected*>(&event)) return e->entity ? e->entity->typeId() : geom::TypeId{};
        if (auto* e = dynamic_cast<const EntityDeselected*>(&event)) return e->entity ? e->entity->typeId() : geom::TypeId{};
        if (auto* e = dynamic_cast<const PropertyChanged*>(&event)) return e->entity ? e->entity->typeId() : geom::TypeId{};
        if (auto* e = dynamic_cast<const PropertyBatchChanged*>(&event)) return e->entity ? e->entity->typeId() : geom::TypeId{};
        return geom::TypeId{};
    }

    static int getEntityId(const Event& event) {
        if (auto* e = dynamic_cast<const EntityAdded*>(&event)) return e->entity ? e->entity->id() : -1;
        if (auto* e = dynamic_cast<const EntityRemoved*>(&event)) return e->entityId;
        if (auto* e = dynamic_cast<const EntityModified*>(&event)) return e->entity ? e->entity->id() : -1;
        if (auto* e = dynamic_cast<const EntitySelected*>(&event)) return e->entity ? e->entity->id() : -1;
        if (auto* e = dynamic_cast<const EntityDeselected*>(&event)) return e->entity ? e->entity->id() : -1;
        if (auto* e = dynamic_cast<const PropertyChanged*>(&event)) return e->entity ? e->entity->id() : -1;
        if (auto* e = dynamic_cast<const PropertyBatchChanged*>(&event)) return e->entity ? e->entity->id() : -1;
        return -1;
    }

    static std::string getLayerName(const Event& event) {
        if (auto* e = dynamic_cast<const EntityAdded*>(&event)) return e->entity ? e->entity->layer() : "";
        if (auto* e = dynamic_cast<const EntityRemoved*>(&event)) return e->entityId >= 0 ? "removed" : "";
        if (auto* e = dynamic_cast<const EntityModified*>(&event)) return e->entity ? e->entity->layer() : "";
        if (auto* e = dynamic_cast<const LayerAdded*>(&event)) return e->layerName;
        if (auto* e = dynamic_cast<const LayerRemoved*>(&event)) return e->layerName;
        if (auto* e = dynamic_cast<const LayerModified*>(&event)) return e->layerName;
        if (auto* e = dynamic_cast<const PropertyChanged*>(&event)) return e->entity ? e->entity->layer() : "";
        if (auto* e = dynamic_cast<const PropertyBatchChanged*>(&event)) return e->entity ? e->entity->layer() : "";
        return "";
    }
};

// Typed event bus using type erasure for subscribers
class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(EventBus&&) noexcept = default;
    EventBus& operator=(EventBus&&) noexcept = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    using Callback = std::function<void(const Event&)>;
    using SubscriptionId = std::size_t;

    // Subscribe to a specific event type with optional filter
    template <typename EventType>
    SubscriptionId subscribe(std::function<void(const EventType&)> handler, EventFilter filter = {}) {
        static_assert(std::is_base_of_v<Event, EventType>, "EventType must derive from Event");
        
        SubscriptionId id = nextId_++;
        auto wrapper = [handler, filter](const Event& e) {
            if (filter.matches(e)) {
                if (auto* typed = dynamic_cast<const EventType*>(&e)) {
                    handler(*typed);
                }
            }
        };
        subscriptions_[typeName<EventType>()].push_back({id, wrapper});
        idToType_[id] = typeName<EventType>();
        idToFilter_[id] = filter;
        return id;
    }

    // Subscribe to all events
    SubscriptionId subscribeAll(std::function<void(const Event&)> handler) {
        SubscriptionId id = nextId_++;
        allSubscriptions_.push_back({id, std::move(handler)});
        idToType_[id] = "ALL";
        return id;
    }

    // Unsubscribe
    void unsubscribe(SubscriptionId id) {
        auto it = idToType_.find(id);
        if (it == idToType_.end()) return;

        std::string typeName = it->second;
        idToType_.erase(it);
        idToFilter_.erase(id);

        if (typeName == "ALL") {
            removeFromVector(allSubscriptions_, id);
        } else {
            auto subIt = subscriptions_.find(typeName);
            if (subIt != subscriptions_.end()) {
                removeFromVector(subIt->second, id);
                if (subIt->second.empty()) subscriptions_.erase(subIt);
            }
        }
    }

    // Publish an event
    template <typename EventType>
    void publish(const EventType& event) {
        static_assert(std::is_base_of_v<Event, EventType>, "EventType must derive from Event");
        
        // Specific subscribers
        auto it = subscriptions_.find(typeName<EventType>());
        if (it != subscriptions_.end()) {
            for (const auto& sub : it->second) {
                sub.callback(event);
            }
        }

        // All subscribers
        for (const auto& sub : allSubscriptions_) {
            sub.callback(event);
        }
    }

    // Clear all subscriptions
    void clear() {
        subscriptions_.clear();
        allSubscriptions_.clear();
        idToType_.clear();
        idToFilter_.clear();
    }

    // Get singleton instance
    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

private:
    struct Subscription {
        SubscriptionId id;
        std::function<void(const Event&)> callback;
    };

    std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
    std::vector<Subscription> allSubscriptions_;
    std::unordered_map<SubscriptionId, std::string> idToType_;
    std::unordered_map<SubscriptionId, EventFilter> idToFilter_;
    SubscriptionId nextId_ = 1;

    template <typename T>
    static std::string typeName() {
        return typeid(T).name();
    }

    static void removeFromVector(std::vector<Subscription>& vec, SubscriptionId id) {
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [id](const Subscription& s) { return s.id == id; }),
                  vec.end());
    }
};

// RAII subscription guard
class SubscriptionGuard {
public:
    SubscriptionGuard(EventBus::SubscriptionId id = 0) : id_(id) {}
    ~SubscriptionGuard() { if (id_) EventBus::instance().unsubscribe(id_); }
    
    SubscriptionGuard(const SubscriptionGuard&) = delete;
    SubscriptionGuard& operator=(const SubscriptionGuard&) = delete;
    
    SubscriptionGuard(SubscriptionGuard&& other) noexcept : id_(other.id_) { other.id_ = 0; }
    SubscriptionGuard& operator=(SubscriptionGuard&& other) noexcept {
        if (id_) EventBus::instance().unsubscribe(id_);
        id_ = other.id_;
        other.id_ = 0;
        return *this;
    }

private:
    EventBus::SubscriptionId id_;
};

} // namespace bcad::events