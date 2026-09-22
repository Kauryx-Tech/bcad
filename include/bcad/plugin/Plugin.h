#pragma once

#include "bcad/geometry/TypeId.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/commands/Command.h"
#include "bcad/commands/CommandRegistry.h"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::plugin {

// Export macro pour la bibliotheque hote (libbcad_plugin) : seul l'API
// publique est exportee (le reste est compile avec -fvisibility=hidden).
#if defined(_WIN32)
#  if defined(BCAD_PLUGIN_BUILDING)
#    define BCAD_PLUGIN_API __declspec(dllexport)
#  else
#    define BCAD_PLUGIN_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define BCAD_PLUGIN_API __attribute__((visibility("default")))
#else
#  define BCAD_PLUGIN_API
#endif

// Version of the plugin API - increment on breaking changes
constexpr int PLUGIN_API_VERSION = 1;

// Forward declaration
class PluginManager;

// Plugin metadata
struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    int apiVersion = PLUGIN_API_VERSION;
};

// Plugin entry point - called when plugin is loaded
// Return false to indicate load failure
using PluginInitFunc = bool (*)(PluginManager& manager);
using PluginShutdownFunc = void (*)();

// Opaque handle for loaded plugin
struct PluginHandle {
    void* handle = nullptr; // dlopen handle
    PluginInfo info;
    PluginInitFunc initFunc = nullptr;
    PluginShutdownFunc shutdownFunc = nullptr;
    bool loaded = false;
};

// Type aliases for plugin callbacks
using EntityFactory = std::function<std::unique_ptr<bcad::geom::Entity>(std::string_view)>;
using CommandFactory = std::function<std::unique_ptr<bcad::commands::Command>(const std::vector<std::string>&)>;

// Interface for plugin manager - allows plugins to register extensions
class BCAD_PLUGIN_API PluginManager {
public:
    virtual ~PluginManager() = default;

    // Register a new entity type from a plugin
    // Returns false if typeId already registered
    virtual bool registerEntityType(
        bcad::geom::TypeId typeId,
        const EntityFactory& factory
    ) = 0;

    // Register a custom command from a plugin
    virtual bool registerCommand(
        std::string_view commandName,
        const CommandFactory& factory
    ) = 0;

    // Load a plugin from a shared library path
    // Returns handle on success, null on failure
    virtual PluginHandle* loadPlugin(const std::string& path) = 0;

    // Unload a plugin
    virtual bool unloadPlugin(PluginHandle* handle) = 0;

    // Get all loaded plugins
    virtual std::vector<PluginHandle*> getLoadedPlugins() const = 0;
};

// Global plugin manager access
BCAD_PLUGIN_API PluginManager& pluginManager();

} // namespace bcad::plugin