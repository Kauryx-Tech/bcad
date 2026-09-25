#include "bcad/plugin/Plugin.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <dlfcn.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

namespace bcad::plugin {

namespace {

// Un module candidat est soit une bibliotheque partagee, soit le fichier SANS
// suffixe pose par CMake dans l'arbre de build (le plugin cadastral est
// compile avec PREFIX "" SUFFIX ""). Seul `libbcad_plugin` est exclu : c'est la
// bibliotheque de mediation de l'hote, pas un module.
bool looksLikePluginModule(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    const std::string stem = path.stem().string();
    if (stem == "libbcad_plugin" || stem == "bcad_plugin") {
        return false;
    }
    static constexpr std::array<const char*, 3> suffixes{{".so", ".dll", ".dylib"}};
    for (const char* suffix : suffixes) {
        const auto len = std::strlen(suffix);
        if (name.size() > len && name.compare(name.size() - len, len, suffix) == 0) {
            return true;
        }
    }
    return name.rfind("bcad_", 0) == 0 && path.extension().empty();
}

class PluginManagerImpl : public PluginManager {
public:
    ~PluginManagerImpl() override {
        // Pas de dlclose ici. unloadPlugin() doit nettoyer des registres
        // (SerializerRegistry, WorkbenchRegistry, ValidatorRegistry) qui sont des
        // statiques de fonction : leur ordre de destruction vis-a-vis du manager
        // n'est pas defini, et les atteindre a ce moment-la revient a appeler des
        // methodes sur des objets detruits - SEGV a la sortie de tout programme qui
        // charge un plugin sans le decharger explicitement, ce qui est le cas de
        // l'application. Fermer les modules a cet instant n'apporte rien a un
        // processus qui finit ; unloadPlugin() garde tout son sens en cours
        // d'execution, ou les registres sont vivants.
        for (auto& [path, pluginHandle] : plugins_) {
            pluginHandle.loaded = false;
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
            const char* error = dlerror();
            std::cerr << "bcad[plugin]: impossible de charger '" << path
                      << "' : " << (error ? error : "erreur inconnue") << "\n";
            return nullptr;
        }

        // Get plugin entry points
        auto initFunc = reinterpret_cast<PluginInitFunc>(dlsym(dlHandle, "bcad_plugin_init"));
        auto shutdownFunc = reinterpret_cast<PluginShutdownFunc>(dlsym(dlHandle, "bcad_plugin_shutdown"));
        auto apiVersionFunc = reinterpret_cast<PluginVersionCheckFunc>(dlsym(dlHandle, "bcad_plugin_api_version"));

        if (!initFunc) {
            std::cerr << "bcad[plugin]: '" << path
                      << "' : symbole `bcad_plugin_init` absent ; module non rejete\n";
            dlclose(dlHandle);
            return nullptr;
        }

        // Optional early ABI version gate
        int pluginApiVersion = PLUGIN_API_VERSION;
        if (apiVersionFunc) {
            pluginApiVersion = apiVersionFunc();
        }
        if (pluginApiVersion != PLUGIN_API_VERSION) {
            std::cerr << "bcad[plugin]: '" << path << "' : API plugin v"
                      << pluginApiVersion << " != v" << PLUGIN_API_VERSION
                      << " attendu (ADR-011 : pas de garantie ABI inter-versions, "
                         "recompiler le module avec le SDK courant)\n";
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
            for (const auto& id : registry.registeredValidatorIds())
                ValidatorRegistry::instance().unregisterValidator(id);
            for (const auto& id : registry.registeredWorkbenchIds())
                WorkbenchRegistry::instance().unregisterWorkbench(id);
            for (const auto& typeId : registry.registeredSerializerTypeIds())
                bcad::serialization::SerializerRegistry::remove(bcad::geom::TypeId{typeId});
            for (const auto& command : registry.registeredCommandNames())
                bcad::commands::CommandRegistry::instance().unregisterCommand(command);
            for (const auto& typeId : registry.registeredEntityTypeIds())
                bcad::registry::EntityRegistry::unregisterType(bcad::geom::TypeId{typeId});
            dlclose(dlHandle);
            return nullptr;
        }

        // Publish the loaded plugin
        PluginHandle pluginHandle;
        pluginHandle.handle = dlHandle;
        pluginHandle.info = registry.info();
        pluginHandle.initFunc = initFunc;
        pluginHandle.shutdownFunc = shutdownFunc;
        pluginHandle.serializerTypes = registry.registeredSerializerTypeIds();
        pluginHandle.entityTypes = registry.registeredEntityTypeIds();
        pluginHandle.commandNames = registry.registeredCommandNames();
        pluginHandle.workbenchIds = registry.registeredWorkbenchIds();
        pluginHandle.validatorIds = registry.registeredValidatorIds();

        auto [newIt, inserted] = plugins_.emplace(path, std::move(pluginHandle));
        newIt->second.loaded = true;
        // Trace de succes : sans elle, un module absent et un module charge ne
        // se distinguent nulle part dans un terminal.
        std::clog << "bcad[plugin]: charge '" << newIt->second.info.name << "' depuis " << path
                  << " (" << newIt->second.commandNames.size() << " commande(s), "
                  << newIt->second.entityTypes.size() << " type(s), "
                  << newIt->second.workbenchIds.size() << " workbench, "
                  << newIt->second.validatorIds.size() << " validateur(s))\n";
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

        // Retirer les workbenches du plugin PENDANT que le DSO est charge :
        // leurs instances et vtables vivent dans le plugin (meme regle que les
        // serializers). Les panneaux copies par l'hote au chargement ne
        // pointent plus dans le DSO, ils restent valides apres dlclose.
        for (const auto& id : pluginHandle->validatorIds) {
            ValidatorRegistry::instance().unregisterValidator(id);
        }
        pluginHandle->validatorIds.clear();

        for (const auto& id : pluginHandle->workbenchIds) {
            WorkbenchRegistry::instance().unregisterWorkbench(id);
        }
        pluginHandle->workbenchIds.clear();

        // Retirer les serializers du plugin PENDANT que le DSO est charge :
        // leurs instances et vtables vivent dans le plugin, les detruire apres
        // dlclose (teardown de l'hote) executait du code plugin -> SEGV.
        for (const auto& typeId : pluginHandle->serializerTypes) {
            bcad::serialization::SerializerRegistry::remove(bcad::geom::TypeId{typeId});
        }
        pluginHandle->serializerTypes.clear();
        for (const auto& command : pluginHandle->commandNames) {
            bcad::commands::CommandRegistry::instance().unregisterCommand(command);
        }
        pluginHandle->commandNames.clear();
        for (const auto& typeId : pluginHandle->entityTypes) {
            bcad::registry::EntityRegistry::unregisterType(bcad::geom::TypeId{typeId});
        }
        pluginHandle->entityTypes.clear();

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

    void addSearchDirectory(const std::string& directory) override {
        if (directory.empty()) {
            return;
        }
        std::lock_guard lock(mutex_);
        searchDirs_.push_back(directory);
    }

    std::vector<std::string> discoverPluginPaths() const override {
        std::lock_guard lock(mutex_);

        // $BCAD_PLUGIN_PATH a la priorite : un fichier designe, ou un repertoire
        // a scanner.
        std::vector<std::string> directories = searchDirs_;
        std::vector<std::string> explicitFiles;
        if (const char* configured = std::getenv("BCAD_PLUGIN_PATH"); configured && *configured) {
            std::error_code ec;
            const std::filesystem::path path{configured};
            if (std::filesystem::is_directory(path, ec)) {
                directories.insert(directories.begin(), path.string());
            } else if (std::filesystem::is_regular_file(path, ec)) {
                explicitFiles.push_back(path.string());
            }
        }

        std::vector<std::string> result;
        std::set<std::string> seen;
        auto consider = [&result, &seen](const std::filesystem::path& candidate) {
            std::error_code ec;
            std::string key = std::filesystem::canonical(candidate, ec).string();
            if (ec) {
                key = candidate.string();
            }
            if (seen.insert(key).second) {
                result.push_back(candidate.string());
            }
        };

        for (const auto& file : explicitFiles) {
            consider(file);
        }
        for (const auto& directory : directories) {
            std::error_code ec;
            if (!std::filesystem::is_directory(directory, ec)) {
                continue;
            }
            // Une profondeur : les repertoires de modules ne sont pas imbriques.
            // L'ordre d'iteration d'un systeme de fichiers n'est pas defini or
            // l'ordre de chargement determine celui des menus : on trie.
            std::vector<std::filesystem::path> modules;
            for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
                if (ec) {
                    break; // repertoire illisible : passe au candidat suivant
                }
                if (entry.is_regular_file(ec) && looksLikePluginModule(entry.path())) {
                    modules.push_back(entry.path());
                }
            }
            std::sort(modules.begin(), modules.end(),
                      [](const std::filesystem::path& a, const std::filesystem::path& b) {
                          return a.string() < b.string();
                      });
            for (const auto& module : modules) {
                consider(module);
            }
        }
        return result;
    }

    std::vector<PluginHandle*> loadAllDiscovered() override {
        std::vector<PluginHandle*> loaded;
        for (const auto& path : discoverPluginPaths()) {
            if (auto* handle = loadPlugin(path)) {
                loaded.push_back(handle);
            }
        }
        return loaded;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PluginHandle> plugins_;
    std::vector<std::string> searchDirs_;
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
    entityTypeIds_.push_back(typeId.value);
    return true;
}

bool PluginRegistry::registerCommand(std::string_view commandName, const CommandFactory& factory) {
    auto& cmdReg = bcad::commands::CommandRegistry::instance();
    if (cmdReg.hasCommand(commandName)) {
        return false; // Already registered
    }
    // Re-emballage cote hote (meme raison que registerEntityType).
    bool registered = cmdReg.registerCommand(commandName,
        [factory](const std::vector<std::string>& args) { return factory(args); });
    if (registered) commandNames_.emplace_back(commandName);
    return registered;
}

bool PluginRegistry::registerSerializer(std::unique_ptr<bcad::serialization::IEntitySerializer> serializer) {
    if (!serializer) {
        return false;
    }
    bcad::geom::TypeId typeId = serializer->typeId();
    if (!typeId || bcad::serialization::SerializerRegistry::contains(typeId)) {
        return false; // TypeId invalide ou deja traite
    }
    // L'instance est creee dans le DSO du plugin (vtable/operator delete du
    // plugin) : l'hote la porte et la detruit. Pour que la destruction ne
    // execute jamais de code plugin apres dlclose, l'hote RETIRE ces
    // serializers au dechargement (unloadPlugin), pendant que le DSO est
    // encore charge. Le TypeId est rapporte au PluginManager via
    // registeredSerializerTypeIds().
    bcad::serialization::SerializerRegistry::registerSerializer(std::move(serializer));
    serializerTypeIds_.push_back(typeId.value);
    return true;
}

bool PluginRegistry::registerWorkbench(std::unique_ptr<IWorkbench> workbench) {
    if (!workbench || workbench->id().empty()) {
        return false;
    }
    // Meme regle de vie que les serializers : l'objet est construit dans le DSO
    // du plugin (vtable et destructeur chez lui), donc l'hote le detruit au
    // dechargement, AVANT dlclose (voir unloadPlugin).
    auto& registry = WorkbenchRegistry::instance();
    if (registry.find(workbench->id())) {
        return false; // Already registered
    }
    const std::string id = workbench->id();
    registry.registerWorkbench(std::move(workbench));
    workbenchIds_.push_back(id);
    return true;
}

bool PluginRegistry::registerValidator(std::unique_ptr<IValidator> validator) {
    if (!validator || validator->id().empty()) {
        return false;
    }
    // Meme regle de vie que les workbenches et les serializers : l'objet est
    // construit dans le DSO du plugin (vtable et destructeur chez lui), donc
    // l'hote le detruit au dechargement, AVANT dlclose (voir unloadPlugin).
    auto& registry = ValidatorRegistry::instance();
    if (registry.find(validator->id())) {
        return false; // Already registered
    }
    const std::string id = validator->id();
    registry.registerValidator(std::move(validator));
    validatorIds_.push_back(id);
    return true;
}

// --- WorkbenchRegistry ---
// Singleton porte par l'hote : un seul exemplaire quel que soit le DSO qui
// enregistre (meme mediation que EntityRegistry/CommandRegistry/SerializerRegistry).
WorkbenchRegistry& WorkbenchRegistry::instance() {
    static WorkbenchRegistry registry;
    return registry;
}

bool WorkbenchRegistry::registerWorkbench(std::unique_ptr<IWorkbench> workbench) {
    if (!workbench || find(workbench->id())) {
        return false;
    }
    entries_.push_back(std::move(workbench));
    return true;
}

void WorkbenchRegistry::unregisterWorkbench(const std::string& id) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if ((*it)->id() == id) {
            entries_.erase(it);
            return;
        }
    }
}

void WorkbenchRegistry::clear() {
    entries_.clear();
}

std::vector<const IWorkbench*> WorkbenchRegistry::workbenches() const {
    std::vector<const IWorkbench*> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) {
        result.push_back(entry.get());
    }
    return result;
}

const IWorkbench* WorkbenchRegistry::find(std::string_view id) const {
    for (const auto& entry : entries_) {
        if (entry->id() == id) {
            return entry.get();
        }
    }
    return nullptr;
}

// --- ValidatorRegistry ---
// Singleton porte par l'hote : un seul exemplaire quel que soit le DSO qui
// enregistre (meme mediation que WorkbenchRegistry).
ValidatorRegistry& ValidatorRegistry::instance() {
    static ValidatorRegistry registry;
    return registry;
}

bool ValidatorRegistry::registerValidator(std::unique_ptr<IValidator> validator) {
    if (!validator || find(validator->id())) {
        return false;
    }
    entries_.push_back(std::move(validator));
    return true;
}

void ValidatorRegistry::unregisterValidator(const std::string& id) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if ((*it)->id() == id) {
            entries_.erase(it);
            return;
        }
    }
}

void ValidatorRegistry::clear() {
    entries_.clear();
}

std::vector<const IValidator*> ValidatorRegistry::validators() const {
    std::vector<const IValidator*> result;
    result.reserve(entries_.size());
    for (const auto& entry : entries_) {
        result.push_back(entry.get());
    }
    return result;
}

const IValidator* ValidatorRegistry::find(std::string_view id) const {
    for (const auto& entry : entries_) {
        if (entry->id() == id) {
            return entry.get();
        }
    }
    return nullptr;
}

PluginManager& pluginManager() {
    static class PluginManagerImpl instance;
    return instance;
}

} // namespace bcad::plugin