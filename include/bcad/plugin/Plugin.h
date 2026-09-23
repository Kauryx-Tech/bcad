#pragma once

#include "bcad/plugin/PluginRegistry.h"
#include <string>
#include <vector>

namespace bcad::plugin {

// Plugin entry point (ADR-005) - exported by the plugin:
//   extern "C" bool bcad_plugin_init(PluginRegistry& reg) { ... return true; }
// Le plugin remplit reg.info() et enregistre ses extensions via reg.
// Retourner false fait echouer le chargement.
using PluginInitFunc = bool (*)(PluginRegistry& reg);

// Optional teardown hook exported by the plugin.
using PluginShutdownFunc = void (*)();

// Optional integer-returning hook for early ABI version gate.
using PluginVersionCheckFunc = int (*)();

// Opaque handle for loaded plugin
struct PluginHandle {
    void* handle = nullptr; // dlopen handle
    PluginInfo info;
    PluginInitFunc initFunc = nullptr;
    PluginShutdownFunc shutdownFunc = nullptr;
    bool loaded = false;
    // TypeIds serializer enregistres par ce plugin : retires par l'hote avant
    // dlclose (leur code/instances vivent dans le DSO du plugin).
    std::vector<std::string> serializerTypes;
};

// Lifecycle manager for plugins (host side). Le chargement est fait via
// dlopen/LoadLibrary et verifie le symbole `bcad_plugin_init`.
class BCAD_PLUGIN_API PluginManager {
public:
    virtual ~PluginManager() = default;

    // Load a plugin from a shared library path.
    // Returns handle on success, null on failure.
    virtual PluginHandle* loadPlugin(const std::string& path) = 0;

    // Unload a plugin (calls bcad_plugin_shutdown then dlclose).
    virtual bool unloadPlugin(PluginHandle* handle) = 0;

    // Get all loaded plugins
    virtual std::vector<PluginHandle*> getLoadedPlugins() const = 0;
};

// Global plugin manager access
BCAD_PLUGIN_API PluginManager& pluginManager();

} // namespace bcad::plugin