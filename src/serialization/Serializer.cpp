#include "bcad/serialization/Serializer.h"
#include <mutex>
#include <unordered_map>

namespace bcad::serialization {

namespace {
std::unordered_map<std::string, std::unique_ptr<IEntitySerializer>>& getMap() {
    static std::unordered_map<std::string, std::unique_ptr<IEntitySerializer>> map;
    return map;
}

std::mutex& getMutex() {
    static std::mutex m;
    return m;
}
} // namespace

std::unordered_map<std::string, std::unique_ptr<IEntitySerializer>>& SerializerRegistry::map() {
    return getMap();
}

std::mutex& SerializerRegistry::mutex() {
    return getMutex();
}

void SerializerRegistry::registerSerializer(std::unique_ptr<IEntitySerializer> serializer) {
    std::lock_guard lock(mutex());
    auto typeId = serializer->typeId();
    if (typeId) {
        getMap().emplace(typeId.value, std::move(serializer));
    }
}

const IEntitySerializer* SerializerRegistry::find(geom::TypeId typeId) {
    std::lock_guard lock(mutex());
    auto it = getMap().find(typeId.value);
    return it != getMap().end() ? it->second.get() : nullptr;
}

bool SerializerRegistry::contains(geom::TypeId typeId) {
    std::lock_guard lock(mutex());
    return getMap().find(typeId.value) != getMap().end();
}

void SerializerRegistry::remove(geom::TypeId typeId) {
    std::lock_guard lock(mutex());
    getMap().erase(typeId.value);
}

std::vector<geom::TypeId> SerializerRegistry::registeredTypes() {
    std::lock_guard lock(mutex());
    std::vector<geom::TypeId> result;
    result.reserve(getMap().size());
    for (const auto& [key, _] : getMap()) {
        result.emplace_back(key);
    }
    return result;
}

void SerializerRegistry::initializeNativeSerializers() {
    // This function is implemented in NativeSerializers.cpp to avoid circular dependencies
    extern void registerNativeSerializers();
    registerNativeSerializers();
}

} // namespace bcad::serialization