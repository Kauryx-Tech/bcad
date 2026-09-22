#pragma once

#include "bcad/plugin/IPlugin.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace bcad::plugin {

// Registry of loaded plugins.
class PluginRegistry {
public:
    // Register a plugin (called by PluginManager after loading).
    static void registerPlugin(std::unique_ptr<IPlugin> plugin);

    // Unregister a plugin by ID.
    static void unregisterPlugin(const std::string& id);

    // Find a plugin by ID.
    static IPlugin* find(const std::string& id);

    // Get all registered plugins.
    static std::vector<IPlugin*> all();

    // Get all plugin IDs.
    static std::vector<std::string> allIds();

    // Check if a plugin is registered.
    static bool contains(const std::string& id);

private:
    static std::unordered_map<std::string, std::unique_ptr<IPlugin>>& map();
};

} // namespace bcad::plugin