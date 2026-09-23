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
`BCAD::bcad_serialization`, ainsi que les bibliothèques de types portées par
l'exécutable (voir `examples/sdk_proof/loader`). Le loader de la preuve embarque
l'ensemble des objets de `bcad_geometry`/`bcad_core`/`bcad_properties` via
`-Wl,--whole-archive` et exporte les symboles bcad ciblés avec
`--export-dynamic-symbol` (voir ci-dessous). Sans cela, le chargement échoue ou
les enregistrements aboutissent dans une instance distincte.

### Types partagés hôte ↔ plugin (dynamic_cast inter-DSO)

Par défaut, chaque DSO embarque ses propres copies des vtables/typeinfo, ce qui
rend le `dynamic_cast` entre DSO non fiable (chaque unité d'édition de liens a
son `typeid`). Pour partager LES types entre l'hôte et le plugin :

- Le plugin (**Phase 10, preuve**) **ne lie aucune bibliothèque de types** : ses
  entités/vtables/typeinfo sont celles **de l'hôte**.
- L'édition de liens de l'hôte doit donc **définir et exporter** tous les
  symboles de types bcad que le plugin référence. C'est le rôle de
  `-Wl,--whole-archive` (tirer **tous** les objets des libs statiques, y compris
  `Property.cpp.o` qui définit les vtables) associé à
  `--export-dynamic-symbol` ciblé sur les seuls symboles `bcad` :
  `_ZN4bcad*` (fonctions), `_ZNK4bcad*`, `_ZTVN4bcad*` (vtables),
  `_ZTIN4bcad*`/`_ZTSN4bcad*`/`_ZGVN4bcad*` (RTTI).
- **Ne pas utiliser `-rdynamic`** : il entraîne un crash de teardown
  déterministe (collision des weak symbols vtables entre DSO). Export ciblé
  uniquement des symboles bcad (testé : binutils ≥ 2.46).
- Résultat : `typeid(*entite) == typeid(PointEntity)` et le `dynamic_cast`
  inter-DSO réussissent dans le loader de la preuve.

### Closures des factories : pointeurs de fonction, pas std::function

Les callbacks d'enregistrement (`EntityFactory`, `CommandFactory`) sont des
**pointeurs de fonction bruts** dans l'ABI (et non des `std::function`) : un
`std::function` traversant un DSO embarque son `_M_manager`/`_M_invoker` émis
dans le **plugin** (site de construction). Stocké dans un registre hôte, il
serait détruit à la sortie du processus **après** le `dlclose` → SEGV.
- L'hôte (`libbcad_plugin`, jamais déchargé) **re-emballe** le pointeur dans un
  `std::function` créé côté hôte : le registre global ne détient aucune closure
  dont le code vit dans le plugin.
- Les plugins passent donc des **fonctions libres** (`&makeEntity`) ou des
  lambdas **sans capture** (convertibles en pointeur de fonction). Un lambda
  avec capture n'est pas accepté (contrainte d'ABI volontaire).

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

    // Factories = pointeurs de fonction (ABI) : fonctions libres ou lambdas
    // SANS capture (conversion implicite). L'hote les re-emballe (cf. §4).
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

## 12. Modules partagés entre hôte et plugin

Le partage de types repose sur une **délimitation nette** entre les modules
que l'hôte porte/exporte et le reste :

| Modules hôte (partagés) | Rôle | Exporté par l'hôte |
|--------------------------|------|--------------------|
| `bcad_geometry` | types (Point2, TypeId, Entity, entités) | vtables/typeinfo via `--export-dynamic-symbol` |
| `bcad_properties` | `PropertyMap` / vtables associées | idem |
| `bcad_core` | Document & entités d'infra | idem |
| `bcad_registry` | EntityRegistry (singleton hôte) | registre |
| `bcad_commands` | CommandRegistry (singleton hôte) | registre |
| `bcad_serialization` | SerializerRegistry (singleton hôte) | registre |

Modules **exclus** de l'interface plugin : `render/` (OpenGL, ADR-001), `io/`
(DXF/SQLite, services applicatifs), `app/` (Qt, ADR-009), `plugin/` (hôte).
Un plugin ne lie jamais ces modules.

**Conséquence pour l'hôte :** tout exécutable qui charge des plugins doit
1) embarquer **tous** les objets statiques des modules partagés
(`-Wl,--whole-archive`, sinon `Property.cpp.o` etc. sont retirés et le
`dlopen` échoue sur `undefined symbol: _ZTVN4bcad...`), et
2) **exporter** les symboles bcad ciblés (`ZM..` → fonctions, `ZTV` → vtables,
`ZTI/ZTS/ZGV` → RTTI). Le plugin, lui, **ne lie aucune** bibliothèque de types.

## 13. Contrat plugin (règles pour l'auteur d'un plugin)

1. **Ne pas définir de types en conflit.** Toujours utiliser les entités /
   classes des headers SDK (ce sont celles de l'hôte, partagées). Ne pas
   redéfinir de classes à vtables « locales » doublonnant une classe SDK —
   des weak symbols en double mènent à des `dynamic_cast`/`typeid` instables.
2. **Ne pas lier de bibliothèque de types** (`bcad_geometry`, `bcad_core`,
   `bcad_properties`, `bcad_registry`, `bcad_commands`, `bcad_serialization`)
   : ce serait une seconde copie des vtables/typeinfo. Lier **uniquement**
   `BCAD::bcad_plugin`.
3. **Passer des fonctions libres** (`&makeEntity`) ou des lambdas **sans
   capture** aux méthodes `registerEntityType`/`registerCommand` (pointeurs de
   fonction). Un lambda avec capture n'est pas compilable (ABI volontaire).
4. **Déclarer `bcad_plugin_api_version()`** renvoyant `PLUGIN_API_VERSION` :
   le plugin ne se charge que si sa version d'ABI est **strictement égale**
   à celle de l'hôte (gate dans PluginManager). Recompiler le plugin pour
   chaque version de BCAD (ADR-011).
5. **Même chaîne d'outils** que l'hôte (compilateur, libstdc++, standard,
   RTTI **activé**, exceptions compatibles) : les ABI C++ (mangling, layout,
   vtables) diffèrent entre GCC / Clang / MSVC.
6. **Ressources avant `dlclose`** : détruire avant `unloadPlugin` toute entité
   ou commande créée depuis les factories (le code vit dans le DSO du plugin
   déchargeable). Les registres hôte ne gardent que des closures hôte (cf. §4).

## 14. Versionnement de l'ABI plugin

- `PLUGIN_API_VERSION` (`include/bcad/plugin/PluginRegistry.h`) est **incrémenté
  à chaque cassure d'ABI** de l'interface plugin (v1 : factories
  `std::function` → v2 : pointeurs de fonction). Contrôlé strictement au
  chargement (`pluginApiVersion != PLUGIN_API_VERSION` → refus).
- La bibliothèque hôte `libbcad_plugin` porte `VERSION ${BCAD_VERSION}` et
  `SOVERSION ${BCAD_VERSION_MAJOR}` (`src/plugin/CMakeLists.txt`) ; sa version
  **majeure** change à toute cassure ABI.
- SDK versionné via `BCADConfigVersion.cmake` (compatibilité
  `SameMajorVersion`, ADR-006) : `find_package(BCAD 1 REQUIRED)` accepte
  `1.x.y`.
- Politique de fond : ADR-011 — API source stable en version mineure, **aucun
  contrat ABI inter-versions** en v1 ; les plugins sont recompilés à chaque
  version de BCAD. Un wrapper C stable est visé en v2.