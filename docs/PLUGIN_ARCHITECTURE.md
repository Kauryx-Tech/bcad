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
4. Enregistre ses entités, commandes, serializers via le `PluginRegistry` (§4, médiatisé par l'hôte)

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

## 3. Interface plugin

L'ABI plugin est la cible ADR-005 : une bibliothèque dynamique qui exporte le
symbole `bcad_plugin_init(PluginRegistry&)`. Il n'y a **pas** d'interface de
plugin orientée objet (`IPlugin` a été supprimé, ADR-013) : le contrat est le
registre passé au point d'entrée.

```cpp
// include/bcad/plugin/PluginRegistry.h
namespace bcad::plugin {

struct PluginInfo {
    std::string name;            // "architecture"
    std::string version;        // "1.0.0"
    std::string description;    // "..."
    std::string author;          // "..."
    int apiVersion = PLUGIN_API_VERSION; // gate ABI (optionnel, precoce via bcad_plugin_api_version)
};

using PluginInitFunc = bool (*)(PluginRegistry& reg);

}
```
## 4. PluginRegistry

Le registre est **concret** (pas d'interface virtuelle, ADR-013) et
**médiatisé par l'hôte** : les enregistrements sont implémentés dans
libbcad_plugin (DSO hôte), ce qui garantit une seule instance des registres
globaux quel que soit le DSO du plugin.

```cpp
namespace bcad::plugin {

class BCAD_PLUGIN_API PluginRegistry {
public:
    PluginInfo& info();   // metadonnees du plugin

    bool registerEntityType(geom::TypeId typeId, const EntityFactory& factory);
    bool registerCommand(std::string_view commandName, const CommandFactory& factory);
    bool registerSerializer(std::unique_ptr<serialization::IEntitySerializer> serializer);

private:
    PluginInfo info_;
};

}
```

### Comment fonctionne la médiation (une seule instance des registres)

- `libbcad_plugin.so` est volontairement **mince** : il ne lie **pas** les
  bibliothèques statiques qu'il médiatise. Dans son diagramme de symboles,
  `EntityRegistry::*`, `CommandRegistry::instance()` et
  `SerializerRegistry::registerSerializer` sont **non définis (U)**.
- Les registres (singletons) sont portés par **l'executable hôte**, qui lie
  `bcad_registry`, `bcad_commands` et `bcad_serialization`. Ces trois
  bibliothèques sont compilées en **visibilité par défaut** (pas `hidden`),
  condition nécessaire pour que l'éditeur de liens accepte qu'un DSO les
  référence et les auto-exporte vers l'executable.
- Au chargement, les références non définies de libbcad_plugin sont résolues
  vers l'instance de l'executable : hôte et plugins aboutissent donc au **même**
  registre, sans duplication des statics entre DSO.

**Exigence d'hôte :** toute application qui charge des plugins doit lier
`BCAD::bcad_plugin` **et** `BCAD::bcad_registry`, `BCAD::bcad_commands`,
`BCAD::bcad_serialization` (voir `examples/sdk_proof/loader`). Sans cela, le
chargement échoue ou les enregistrements aboutissent dans une instance
distincte.

**Limitation connue :** les bibliothèques de types (`bcad_geometry`,
`bcad_core`, …) restent en visibilité `hidden` et le plugin embarque ses
propres copies. Le typeinfo/vtables n'est donc **pas partagé** entre l'hôte et
le plugin : un plugin doit lier `bcad_geometry` lui-même (comme la preuve) et
ne doit pas se reposer sur `dynamic_cast` inter-DSO.

## 5. Symbole d'entrée

```cpp
// Plugin.cpp
#include <bcad/plugin/PluginRegistry.h>
#include <memory>

extern "C" int bcad_plugin_api_version() {   // optionnel : gate ABI precoce
    return bcad::plugin::PLUGIN_API_VERSION;
}

extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    reg.info().name = "architecture";
    reg.info().version = "1.0.0";
    reg.info().description = "Architecture domain entities";

    reg.registerEntityType(bcad::geom::TypeId{"arch.wall"}, [](std::string_view params) {
        return std::make_unique<WallEntity>(/* ... */);
    });

    reg.registerCommand("CreateWall", [](const std::vector<std::string>& args) {
        return std::make_unique<CreateWallCommand>(args);
    });

    reg.registerSerializer(std::make_unique<WallSerializer>());

    return true;   // false fait echouer le chargement
}

extern "C" void bcad_plugin_shutdown() {}
```

## 6. PluginManager

### 6.1 Interface

L'interface publique du gestionnaire est **réduite au cycle de vie** (ADR-013) :
la découverte et la configuration par search paths restent des fonctionnalités
futures (§7).

**Médiation :** l'executable hôte porte les registres
(`BCAD::bcad_registry`, `BCAD::bcad_commands`, `BCAD::bcad_serialization`) ;
`libbcad_plugin.so` est mince et résout ses références vers l'hôte (§4).

```cpp
namespace bcad::plugin {

class BCAD_PLUGIN_API PluginManager {
public:
    virtual ~PluginManager() = default;

    // Chargement (dlopen) : vérifie bcad_plugin_init, appelle le plugin, publie
    // un PluginHandle portant reg.info(). nullptr si échec.
    virtual PluginHandle* loadPlugin(const std::string& path) = 0;

    // Déchargement : bcad_plugin_shutdown (si présent) puis dlclose.
    virtual bool unloadPlugin(PluginHandle* handle) = 0;

    // Liste
    virtual std::vector<PluginHandle*> getLoadedPlugins() const = 0;
};

plugin::PluginManager& pluginManager();   // singleton hôte

}
```

### 6.2 Cycle de vie

```
1. Chargement (dlopen)
2. Validation (symbole bcad_plugin_init présent)
3. Vérification de version (bcad_plugin_api_version optionnel, gate ABI précoce)
4. Appel à bcad_plugin_init(PluginRegistry&), hors mutex (ré-entrance possible)
5. Copie de reg.info() dans le PluginHandle puis publication
6. Plugin actif
7. Shutdown (bcad_plugin_shutdown, hors mutex)
8. Unload (dlclose)
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