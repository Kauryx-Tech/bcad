#pragma once

#include "bcad/plugin/PluginRegistry.h"
#include "bcad/plugin/Workbench.h"
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
    std::vector<std::string> entityTypes;
    std::vector<std::string> commandNames;
    // Workbenches declares par ce plugin : leurs instances vivent dans le DSO
    // du plugin, l'hote les detruit avant dlclose.
    std::vector<std::string> workbenchIds;
    // Validateurs declares par ce plugin : meme regle de vie que les workbenches.
    std::vector<std::string> validatorIds;
    // Exporteurs de fichiers declares par ce plugin : meme regle de vie.
    std::vector<std::string> fileExporterIds;
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

    // Repertoire a scanner (aucun nom de metier : l'hote donne des chemins,
    // jamais des noms de module). Ordre d'ajout = ordre de priorite.
    virtual void addSearchDirectory(const std::string& directory) = 0;

    // Modules candidats trouves dans les repertoires ajoutes et dans
    // $BCAD_PLUGIN_PATH (cette variable designe un fichier OU un repertoire).
    // Dedup par chemin canonique ; le scan n'ouvre aucun module.
    virtual std::vector<std::string> discoverPluginPaths() const = 0;

    // Charge tous les candidats. Les non-modules (symbole `bcad_plugin_init`
    // absent, ABI differente) sont echoues sans faire echouer le reste.
    virtual std::vector<PluginHandle*> loadAllDiscovered() = 0;
};

// Global plugin manager access
BCAD_PLUGIN_API PluginManager& pluginManager();

} // namespace bcad::plugin