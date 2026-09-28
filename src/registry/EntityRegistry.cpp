#include "bcad/registry/EntityRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/RadialDimensionEntity.h"
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

// Split CSV params into parts
std::vector<std::string> splitParams(std::string_view params) {
    std::vector<std::string> parts;
    std::string part;
    for (char c : params) {
        if (c == ',') {
            parts.push_back(part);
            part.clear();
        } else {
            part += c;
        }
    }
    if (!part.empty()) parts.push_back(part);
    return parts;
}
} // namespace

std::unordered_map<std::string, EntityMetadata>& EntityRegistry::map() {
    return getMap();
}

void EntityRegistry::registerType(geom::TypeId typeId, std::string_view displayName, FactoryFn factory) {
    registerType(typeId, displayName, std::move(factory), nullptr);
}

void EntityRegistry::registerType(geom::TypeId typeId, std::string_view displayName,
                                  FactoryFn factory, EntityParamsFactory paramsFactory) {
    std::lock_guard lock(getMutex());
    EntityMetadata meta{typeId, std::string(displayName), std::move(factory), std::move(paramsFactory)};
    getMap().emplace(typeId.value, std::move(meta));
}

void EntityRegistry::registerType(geom::TypeId typeId, std::string_view displayName,
                                  EntityParamsFactory paramsFactory) {
    registerType(typeId, displayName, nullptr, std::move(paramsFactory));
}

const EntityMetadata* EntityRegistry::find(geom::TypeId typeId) {
    std::lock_guard lock(getMutex());
    auto it = getMap().find(typeId.value);
    return it != getMap().end() ? &it->second : nullptr;
}

std::unique_ptr<geom::Entity> EntityRegistry::create(geom::TypeId typeId) {
    std::lock_guard lock(getMutex());
    auto it = getMap().find(typeId.value);
    if (it != getMap().end()) {
        if (it->second.factory) {
            return it->second.factory();
        }
        if (it->second.paramsFactory) {
            return it->second.paramsFactory("");
        }
    }
    return nullptr;
}

std::unique_ptr<geom::Entity> EntityRegistry::create(geom::TypeId typeId, std::string_view params) {
    std::lock_guard lock(getMutex());
    auto it = getMap().find(typeId.value);
    if (it != getMap().end()) {
        if (it->second.paramsFactory) {
            return it->second.paramsFactory(params);
        }
        if (it->second.factory) {
            return it->second.factory();
        }
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

bool EntityRegistry::unregisterType(geom::TypeId typeId) {
    std::lock_guard lock(getMutex());
    return getMap().erase(typeId.value) > 0;
}

bool EntityRegistry::hasType(geom::TypeId typeId) {
    return contains(typeId);
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
    registerType(geom::TypeId_Text, "Text",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::TextEntity()); });
    registerType(geom::TypeId_LinearDimension, "LinearDimension",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::LinearDimensionEntity()); });
    registerType(geom::TypeId_AlignedDimension, "AlignedDimension",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::AlignedDimensionEntity()); });
    registerType(geom::TypeId_AngularDimension, "AngularDimension",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::AngularDimensionEntity()); });
    registerType(geom::TypeId_RadiusDimension, "RadiusDimension",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::RadialDimensionEntity(geom::Point2{0,0}, geom::Point2{1,0}, geom::RadialDimensionEntity::RadialType::Radius, geom::Point2{0,0})); });
    registerType(geom::TypeId_DiameterDimension, "DiameterDimension",
        []() -> std::unique_ptr<geom::Entity> { return std::unique_ptr<geom::Entity>(new geom::RadialDimensionEntity(geom::Point2{0,0}, geom::Point2{1,0}, geom::RadialDimensionEntity::RadialType::Diameter, geom::Point2{0,0})); });
}

} // namespace bcad::registry
