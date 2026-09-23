#include "bcad/plugin/Plugin.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <dlfcn.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace bcad::plugin {

namespace {

class PluginManagerImpl : public PluginManager {
public:
    ~PluginManagerImpl() override {
        // Unload all plugins on destruction
        for (auto& [path, pluginHandle] : plugins_) {
            if (pluginHandle.loaded) {
                unloadPlugin(&pluginHandle);
            }
        }
    }

    bcad::plugin::PluginHandle* loadPlugin(const std::string& path) override {
        std::unique_lock lock(mutex_);

        // Check if already loaded
        auto it = plugins_.find(path);
        if (it != plugins_.end()) {
            return it->second.loaded ? &it->second : nullptr;
        }

        // Load shared library
        void* dlHandle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!dlHandle) {
            return nullptr;
        }

        // Get plugin entry points
        auto initFunc = reinterpret_cast<PluginInitFunc>(dlsym(dlHandle, "bcad_plugin_init"));
        auto shutdownFunc = reinterpret_cast<PluginShutdownFunc>(dlsym(dlHandle, "bcad_plugin_shutdown"));
        auto apiVersionFunc = reinterpret_cast<PluginVersionCheckFunc>(dlsym(dlHandle, "bcad_plugin_api_version"));

        if (!initFunc) {
            dlclose(dlHandle);
            return nullptr;
        }

        // Optional early ABI version gate
        int pluginApiVersion = PLUGIN_API_VERSION;
        if (apiVersionFunc) {
            pluginApiVersion = apiVersionFunc();
        }
        if (pluginApiVersion != PLUGIN_API_VERSION) {
            dlclose(dlHandle);
            return nullptr;
        }

        // Appeler le code du plugin sans tenir le mutex : bcad_plugin_init
        // (et les enregistrements) peut re-entrer dans PluginManager.
        PluginRegistry registry;
        lock.unlock();
        bool initResult = initFunc(registry);
        lock.lock();

        if (!initResult) {
            dlclose(dlHandle);
            return nullptr;
        }

        // Publish the loaded plugin
        PluginHandle pluginHandle;
        pluginHandle.handle = dlHandle;
        pluginHandle.info = registry.info();
        pluginHandle.initFunc = initFunc;
        pluginHandle.shutdownFunc = shutdownFunc;

        auto [newIt, inserted] = plugins_.emplace(path, std::move(pluginHandle));
        newIt->second.loaded = true;
        return &newIt->second;
    }

    bool unloadPlugin(PluginHandle* pluginHandle) override {
        if (!pluginHandle || !pluginHandle->loaded || !pluginHandle->handle) {
            return false;
        }

        // Call shutdown without holding the mutex : le plugin peut re-entrer
        // dans PluginManager depuis shutdown().
        if (pluginHandle->shutdownFunc) {
            pluginHandle->shutdownFunc();
        }

        std::lock_guard lock(mutex_);

        // Close library
        dlclose(pluginHandle->handle);
        pluginHandle->handle = nullptr;
        pluginHandle->loaded = false;

        // Remove from map
        for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
            if (&it->second == pluginHandle) {
                plugins_.erase(it);
                break;
            }
        }

        return true;
    }

    std::vector<PluginHandle*> getLoadedPlugins() const override {
        std::lock_guard lock(mutex_);
        std::vector<PluginHandle*> result;
        result.reserve(plugins_.size());
        for (const auto& [path, pluginHandle] : plugins_) {
            if (pluginHandle.loaded) {
                result.push_back(const_cast<PluginHandle*>(&pluginHandle));
            }
        }
        return result;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PluginHandle> plugins_;
};

} // namespace

// --- PluginRegistry (ADR-005) ---
// Implemente dans libbcad_plugin (hote) : les registrations aboutissent dans
// l'unique instance des registres globaux portee par ce DSO.

bool PluginRegistry::registerEntityType(bcad::geom::TypeId typeId, const EntityFactory& factory) {
    if (bcad::registry::EntityRegistry::contains(typeId)) {
        return false; // Already registered
    }
    // L'hote re-emballe le pointeur de fonction du plugin dans un std::function
    // cree ICI (manager/invoker definis dans libbcad_plugin, jamais decharge) :
    // le registre global ne detient aucune closure du plugin (SEGV au teardown
    // si le plugin a deja ete decharge via dlclose).
    bcad::registry::EntityRegistry::registerType(typeId, typeId.value,
        [factory](std::string_view params) { return factory(params); });
    return true;
}

bool PluginRegistry::registerCommand(std::string_view commandName, const CommandFactory& factory) {
    auto& cmdReg = bcad::commands::CommandRegistry::instance();
    if (cmdReg.hasCommand(commandName)) {
        return false; // Already registered
    }
    // Re-emballage cote hote (meme raison que registerEntityType).
    return cmdReg.registerCommand(commandName,
        [factory](const std::vector<std::string>& args) { return factory(args); });
}

bool PluginRegistry::registerSerializer(std::unique_ptr<bcad::serialization::IEntitySerializer> serializer) {
    if (!serializer) {
        return false;
    }
    bcad::serialization::SerializerRegistry::registerSerializer(std::move(serializer));
    return true;
}

PluginManager& pluginManager() {
    static class PluginManagerImpl instance;
    return instance;
}

} // namespace bcad::plugin