#include "bcad/plugin/PluginRegistry.h"
#include <mutex>

namespace bcad::plugin {

namespace {
std::unordered_map<std::string, std::unique_ptr<IPlugin>>& getMap() {
    static std::unordered_map<std::string, std::unique_ptr<IPlugin>> map;
    return map;
}

std::mutex& getMutex() {
    static std::mutex m;
    return m;
}
} // namespace

std::unordered_map<std::string, std::unique_ptr<IPlugin>>& PluginRegistry::map() {
    return getMap();
}

void PluginRegistry::registerPlugin(std::unique_ptr<IPlugin> plugin) {
    if (!plugin) return;
    std::lock_guard lock(getMutex());
    std::string id = plugin->id();
    getMap().emplace(id, std::move(plugin));
}

void PluginRegistry::unregisterPlugin(const std::string& id) {
    std::lock_guard lock(getMutex());
    getMap().erase(id);
}

IPlugin* PluginRegistry::find(const std::string& id) {
    std::lock_guard lock(getMutex());
    auto it = getMap().find(id);
    return it != getMap().end() ? it->second.get() : nullptr;
}

std::vector<IPlugin*> PluginRegistry::all() {
    std::lock_guard lock(getMutex());
    std::vector<IPlugin*> result;
    result.reserve(getMap().size());
    for (auto& [id, plugin] : getMap()) {
        result.push_back(plugin.get());
    }
    return result;
}

std::vector<std::string> PluginRegistry::allIds() {
    std::lock_guard lock(getMutex());
    std::vector<std::string> result;
    result.reserve(getMap().size());
    for (auto& [id, plugin] : getMap()) {
        result.push_back(id);
    }
    return result;
}

bool PluginRegistry::contains(const std::string& id) {
    std::lock_guard lock(getMutex());
    return getMap().find(id) != getMap().end();
}

} // namespace bcad::plugin