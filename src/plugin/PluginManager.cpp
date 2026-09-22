#include "bcad/plugin/Plugin.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/commands/Command.h"
#include "bcad/commands/CommandRegistry.h"
#include <dlfcn.h>
#include <filesystem>
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

    bool registerEntityType(
        bcad::geom::TypeId typeId,
        const EntityFactory& factory
    ) override {
        std::lock_guard lock(mutex_);
        
        // Register with EntityRegistry
        if (bcad::registry::EntityRegistry::contains(typeId)) {
            return false; // Already registered
        }
        bcad::registry::EntityRegistry::registerType(typeId, typeId.value,
            static_cast<bcad::registry::EntityParamsFactory>(factory));
        return true;
    }

    bool registerCommand(
        std::string_view commandName,
        const CommandFactory& factory
    ) override {
        std::lock_guard lock(mutex_);
        
        // Register with CommandRegistry
        auto& cmdReg = bcad::commands::CommandRegistry::instance();
        if (cmdReg.hasCommand(commandName)) {
            return false; // Already registered
        }
        return cmdReg.registerCommand(commandName, factory);
    }

    bcad::plugin::PluginHandle* loadPlugin(const std::string& path) override {
        std::lock_guard lock(mutex_);
        
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
        auto infoFunc = reinterpret_cast<const PluginInfo* (*)()>(dlsym(dlHandle, "bcad_plugin_info"));
        auto apiVersionFunc = reinterpret_cast<int (*)()>(dlsym(dlHandle, "bcad_plugin_api_version"));

        if (!initFunc || !infoFunc) {
            dlclose(dlHandle);
            return nullptr;
        }

        // Check API version compatibility
        int pluginApiVersion = PLUGIN_API_VERSION;
        if (apiVersionFunc) {
            pluginApiVersion = apiVersionFunc();
        }
        if (pluginApiVersion != PLUGIN_API_VERSION) {
            dlclose(dlHandle);
            return nullptr;
        }

        // Get plugin info
        const PluginInfo* info = infoFunc();
        if (!info) {
            dlclose(dlHandle);
            return nullptr;
        }

        // Create handle
        PluginHandle pluginHandle;
        pluginHandle.handle = dlHandle;
        pluginHandle.info = *info;
        pluginHandle.initFunc = initFunc;
        pluginHandle.shutdownFunc = shutdownFunc;
        pluginHandle.loaded = false;

        // Store and initialize
        auto [newIt, inserted] = plugins_.emplace(path, std::move(pluginHandle));
        PluginHandle& pluginHandleRef = newIt->second;

        // Call init function
        if (!initFunc(*this)) {
            // Init failed
            dlclose(pluginHandleRef.handle);
            plugins_.erase(newIt);
            return nullptr;
        }

        pluginHandleRef.loaded = true;
        return &newIt->second;
    }

    bool unloadPlugin(PluginHandle* pluginHandle) override {
        std::lock_guard lock(mutex_);
        
        if (!pluginHandle || !pluginHandle->loaded || !pluginHandle->handle) {
            return false;
        }

        // Call shutdown function
        if (pluginHandle->shutdownFunc) {
            pluginHandle->shutdownFunc();
        }

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

PluginManager& pluginManager() {
    static class PluginManagerImpl instance;
    return instance;
}

} // namespace bcad::plugin