# Architecture des plugins BCAD

> [!IMPORTANT]
>
> ## Statut : ARCHITECTURE IMPLEMENTEE (ABI ADR-005)
>
> Le systeme de plugins existe : `PluginManager` (dlopen), symbole
> `bcad_plugin_init(PluginRegistry&)` (ADR-005), PluginRegistry concret
> (metadonnees + enregistrement des extensions), preuve avec un plugin externe
> minimal (`examples/sdk_proof`, test `sdk_external_test`). Ce document décrit
> la cible ; l'ecart implementation est signale dans chaque section ou
> necessaire (voir aussi `MIGRATION_PLAN.md`, Phase 10).

> Système de plugins dynamique. Chargement, découverte, cycle de vie.

## 1. Principe

Un plugin BCAD est une **bibliothèque dynamique** (`.so`/`.dll`/`.dylib`) qui :
1. Est compilée séparément du Core
2. Trouve BCAD via `find_package(BCAD CONFIG REQUIRED)`
3. Exporte un symbole `bcad_plugin_init(PluginRegistry&)`
4. Enregistre ses entités, commandes, serializers via le `PluginRegistry`

> **Ecart implementation (ADR-005 concret) :** le `PluginRegistry` expose
> `registerEntityType`/`registerCommand`/`registerSerializer` implementes par
> l'hote (libbcad_plugin), plutot qu'un acces direct aux registres globaux.
> Cela garantit que toutes les registrations aboutissent dans l'unique
> instance des registres, quel que soit le DSO du plugin.

## 2. Architecture

```
┌──────────────────────────────────────┐
│          BCAD Application             │
│  ┌──────────────────────────────┐    │
│  │       PluginManager           │    │
│  │  - discovery                  │    │
│  │  - loading                    │    │
│  │  - lifecycle                  │    │
│  └──────────────────────────────┘    │
└──────────────────┬───────────────────┘
                   │ dlopen
       ┌───────────┼───────────┐
       ▼           ▼           ▼
   architecture  civil    mechanical
     .so          .so      .so
```

## 3. Plugin Interface

```cpp
// include/bcad/plugin/IPlugin.h
namespace bcad::plugin {

struct PluginInfo {
    std::string name;            // "architecture"
    std::string version;        // "1.0.0"
    std::string author;          // "..."
    std::string description;    // "..."
    std::string requiresBCAD;   // "1.0.0"
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    virtual void registerTypes(registry::EntityRegistry& entities,
                               registry::CommandRegistry& commands,
                               registry::SerializerRegistry& serializers,
                               events::EventBus& events) = 0;
};

}
```

## 4. PluginRegistry

```cpp
namespace bcad::plugin {

class PluginRegistry {
public:
    plugin::PluginInfo& info();

    registry::EntityRegistry& entityRegistry();
    registry::CommandRegistry& commandRegistry();
    registry::SerializerRegistry& serializerRegistry();
    events::EventBus& eventBus();
    document::Document* document();

private:
    plugin::PluginInfo info_;
    // pointeurs vers les registres globaux
};

}
```

## 5. Symbole d'entrée

```cpp
// Plugin.cpp
#include <bcad/sdk.h>

extern "C" void bcad_plugin_init(PluginRegistry& reg) {
    reg.info().name = "architecture";
    reg.info().version = "1.0.0";
    reg.info().description = "Architecture domain entities";
    reg.info().requiresBCAD = "1.0.0";

    reg.entityRegistry().registerType<WallEntity>();
    reg.entityRegistry().registerType<DoorEntity>();

    reg.commandRegistry().registerCommand<CreateWallCommand>("CreateWall");

    reg.serializerRegistry().registerSerializer(std::make_unique<WallSerializer>());

    reg.eventBus().subscribe<events::EntityAddedEvent>([](const auto& e) {
        // ...
    });
}
```

## 6. PluginManager

### 6.1 Interface

```cpp
namespace bcad::plugin {

class PluginManager {
public:
    // Découverte
    std::vector<PluginInfo> discover(const std::vector<std::filesystem::path>& searchPaths) const;

    // Chargement
    std::shared_ptr<LoadedPlugin> load(const std::filesystem::path& libraryPath);
    void unload(LoadedPlugin* plugin);

    // Cycle de vie
    void initializeAll();
    void shutdownAll();

    // Liste
    std::vector<LoadedPlugin*> loaded() const;
    LoadedPlugin* find(const std::string& name) const;

    // Configuration
    void setSearchPaths(const std::vector<std::filesystem::path>& paths);
    void setEnabled(const std::string& name, bool enabled);

    // Events
    events::EventBus& eventBus();

private:
    // ... implémentation
};

}
```

### 6.2 Cycle de vie

```
1. Découverte (search paths)
2. Chargement (dlopen)
3. Validation (symbole bcad_plugin_init présent)
4. Vérification de version (PluginInfo::requiresBCAD)
5. Appel à bcad_plugin_init(PluginRegistry&)
6. Initialisation du plugin (peut créer un état)
7. Plugin actif
8. Shutdown (plugin peut libérer ses ressources)
9. Unload (dlclose)
```

## 7. Manifeste

Un plugin peut fournir un manifeste `plugin.json` à côté de la bibliothèque :

```json
{
  "name": "architecture",
  "version": "1.0.0",
  "author": "BCAD Team",
  "description": "Architecture domain entities",
  "library": "libbcad-architecture-plugin",
  "requires": {
    "BCAD": "1.0.0"
  },
  "entry": "bcad_plugin_init"
}
```

**Avantage :** le PluginManager peut découvrir et décrire le plugin avant de le charger.

## 8. Mécanismes de chargement par OS

| OS | Mécanisme | API |
|----|-----------|-----|
| Linux | `dlopen` / `dlsym` / `dlclose` | `<dlfcn.h>` |
| Windows | `LoadLibrary` / `GetProcAddress` / `FreeLibrary` | `<windows.h>` |
| macOS | `dlopen` / `dlsym` / `dlclose` | `<dlfcn.h>` |

Implémentation portable via `std::filesystem` + `dlfcn.h` (POSIX) ou `<windows.h>` (Windows).

```cpp
class NativeLoader {
public:
    void* load(const std::filesystem::path& lib);
    void* getSymbol(void* handle, const std::string& name);
    void close(void* handle);
    std::string lastError() const;
};
```

## 9. Répertoires de plugins

### Linux

```
~/.local/share/bcad/plugins/
/usr/lib/bcad/plugins/
/usr/local/lib/bcad/plugins/
```

### Windows

```
%APPDATA%\BCAD\plugins\
C:\Program Files\BCAD\plugins\
```

### macOS

```
~/Library/Application Support/BCAD/plugins/
/Library/Application Support/BCAD/plugins/
```

## 10. Dépendances entre plugins

```cpp
struct PluginInfo {
    // ...
    std::vector<std::string> requiresPlugins;  // noms des plugins requis
};
```

Le PluginManager charge les dépendances avant le plugin qui en dépend.

## 11. Règles

1. Plugin = bibliothèque dynamique
2. Symbole d'entrée : `bcad_plugin_init`
3. PluginInfo déclare les métadonnées
4. Le PluginManager gère le cycle de vie
5. Les plugins utilisent uniquement le SDK public
6. Les dépendances entre plugins sont déclaratives
7. Le déchargement est sûr (pas de ressources partagées non gérées)