#pragma once

#include "bcad/plugin/PluginRegistry.h"
#include "bcad/plugin/IPlugin.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <filesystem>

namespace bcad::plugin {

// Manages plugin loading/unloading via dlopen/LoadLibrary.
class PluginManager {
public:
    struct PluginHandle {
        void* handle = nullptr;
        std::string path;
        IPlugin* plugin = nullptr;
        std::string loadedVersion;
        SandboxLevel sandboxLevel;
    };

    using Handles = std::unordered_map<std::string, std::unique_ptr<PluginHandle>>;

    // Load a plugin from a shared library file.
    // Returns true on success, false on failure.
    // The plugin is automatically registered if initialization succeeds.
    static bool loadPlugin(const std::string& path);

    // Load a plugin with version negotiation and sandbox configuration.
    // Returns true on success, false on failure.
    static bool loadPlugin(const std::string& path, 
                           const PluginVersion& hostVersion,
                           SandboxLevel maxSandboxLevel = SandboxLevel::Full);

    // Unload a plugin by ID.
    static bool unloadPlugin(const std::string& id);

    // Unload all plugins.
    static void unloadAll();

    // Get all loaded plugin IDs.
    static std::vector<std::string> loadedPluginIds();

    // Check if a plugin is loaded.
    static bool isLoaded(const std::string& id);

    // Get the file path of a loaded plugin.
    static std::string pluginPath(const std::string& id);

    // Get a plugin instance by ID.
    static IPlugin* getPlugin(const std::string& id);

    // Check if a plugin's requested capabilities are granted.
    static bool checkCapabilities(const PluginCapabilities& requested,
                                  const PluginCapabilities& granted);

    // Get the effective sandbox level for a plugin.
    static SandboxLevel getEffectiveSandboxLevel(const PluginCapabilities& requested,
                                                  SandboxLevel maxAllowed);

private:
    static Handles& getHandles();
    static std::mutex& getMutex();
    static std::string makeAbsolutePath(const std::string& path);
    static PluginVersion getHostVersion();
};

} // namespace bcad::plugin