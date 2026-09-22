#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/TypeId.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace bcad::geom {

// Factory function signature for creating entities from serialized parameters
// params format is the same as serializeParams() output
using EntityFactory = std::function<std::unique_ptr<Entity>(std::string_view params)>;

// Factory function signature for creating entities from parameter parts (already parsed)
using EntityFactoryParsed = std::function<std::unique_ptr<Entity>(const std::vector<std::string>& parts)>;

// Registry for entity types. Allows plugins to register new entity types
// without modifying Core. Maps TypeId to factory functions.
class EntityRegistry {
public:
    EntityRegistry() = default;
    ~EntityRegistry() = default;
    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;
    EntityRegistry(EntityRegistry&&) = default;
    EntityRegistry& operator=(EntityRegistry&&) = default;

    // Register a native entity type with a factory that parses CSV parameters
    // Called once at startup for native types, and by plugins for custom types.
    void registerType(TypeId typeId, EntityFactory factory);

    // Register with pre-parsed parts (more efficient for complex types)
    void registerTypeParsed(TypeId typeId, EntityFactoryParsed factory);

    // Check if a type is registered
    bool hasType(TypeId typeId) const;

    // Create an entity from its TypeId and serialized parameters
    // Returns null if type not registered or params invalid
    std::unique_ptr<Entity> create(TypeId typeId, std::string_view params) const;

    // Create from pre-parsed parts
    std::unique_ptr<Entity> createParsed(TypeId typeId, const std::vector<std::string>& parts) const;

    // Get all registered type IDs (for UI, debugging, etc.)
    std::vector<TypeId> getRegisteredTypes() const;

    // Singleton access (initialized at program start)
    static EntityRegistry& instance();

private:
    struct Entry {
        EntityFactory factory;
        EntityFactoryParsed factoryParsed;
    };

    std::unordered_map<TypeId, Entry> factories_;
};

// Register all native BCAD entity types (Point, Line, Circle, Arc, Polyline).
// Must be called once at application startup before any deserialization.
void registerNativeEntityTypes();

} // namespace bcad::geom