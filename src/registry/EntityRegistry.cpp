#include "bcad/registry/EntityRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include <mutex>

namespace bcad::registry {

namespace {
std::unordered_map<std::string, EntityMetadata>& getMap() {
    static std::unordered_map<std::string, EntityMetadata> map;
    return map;
}

std::mutex& getMutex() {
    static std::mutex m;
    return m;
}
} // namespace

std::unordered_map<std::string, EntityMetadata>& EntityRegistry::map() {
    return getMap();
}

void EntityRegistry::registerType(geom::TypeId typeId, std::string_view displayName, FactoryFn factory) {
    std::lock_guard lock(getMutex());
    EntityMetadata meta{typeId, std::string(displayName), std::move(factory)};
    getMap().emplace(typeId.value, std::move(meta));
}

const EntityMetadata* EntityRegistry::find(geom::TypeId typeId) {
    std::lock_guard lock(getMutex());
    auto it = getMap().find(typeId.value);
    return it != getMap().end() ? &it->second : nullptr;
}

std::unique_ptr<geom::Entity> EntityRegistry::create(geom::TypeId typeId) {
    std::lock_guard lock(getMutex());
    auto it = getMap().find(typeId.value);
    if (it != getMap().end() && it->second.factory) {
        return it->second.factory();
    }
    return nullptr;
}

std::vector<EntityMetadata> EntityRegistry::all() {
    std::lock_guard lock(getMutex());
    std::vector<EntityMetadata> result;
    result.reserve(getMap().size());
    for (const auto& [key, meta] : getMap()) {
        result.push_back(meta);
    }
    return result;
}

bool EntityRegistry::contains(geom::TypeId typeId) {
    std::lock_guard lock(getMutex());
    return getMap().find(typeId.value) != getMap().end();
}

const EntityMetadata* EntityRegistry::findByName(std::string_view displayName) {
    std::lock_guard lock(getMutex());
    for (const auto& [key, meta] : getMap()) {
        if (meta.displayName == displayName) {
            return &meta;
        }
    }
    return nullptr;
}

std::vector<geom::TypeId> EntityRegistry::allTypeIds() {
    std::lock_guard lock(getMutex());
    std::vector<geom::TypeId> result;
    result.reserve(getMap().size());
    for (const auto& [key, meta] : getMap()) {
        result.push_back(meta.typeId);
    }
    return result;
}

void EntityRegistry::registerNativeTypes() {
    // Force registration of all native entity types
    // This calls the same registration logic as the static registrar
    registerType(geom::TypeId_Point, "Point",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::PointEntity()); });
    registerType(geom::TypeId_Line, "Line",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::LineEntity()); });
    registerType(geom::TypeId_Circle, "Circle",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::CircleEntity()); });
    registerType(geom::TypeId_Arc, "Arc",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::ArcEntity()); });
    registerType(geom::TypeId_Polyline, "Polyline",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::PolylineEntity()); });
}

} // namespace bcad::registry