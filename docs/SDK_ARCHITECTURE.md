# Architecture du SDK BCAD

> [!IMPORTANT]
>
> ## Statut : IMPLEMENTE — SDK installable (ADR-006)
>
> Le SDK existe : `install()`/`export()` CMake, targets importés
> `BCAD::bcad_*`, `BCADConfig.cmake` + `BCADConfigVersion.cmake`, prouvé par
> `examples/sdk_proof` (test `sdk_external_test`, script `scripts/prove_sdk.sh`).
> Il n'y a pas de header d'agrégat `bcad/sdk.h` : chaque module expose ses headers
> sous `include/bcad/`.

> SDK public. Utilisable par un développeur qui n'a pas besoin de connaître les internals.

## 1. Objectif

Le **BCAD SDK** est la surface publique destinée aux développeurs de plugins et d'intégrations. Il expose :
- Les types du Core nécessaires
- Les interfaces d'enregistrement
- Les contrats d'extension

Le SDK est **séparé** du Core interne.

## 2. Structure

```
bcad-sdk/
├── include/bcad/        # Headers publics du SDK
│   ├── geometry/        # Point2, Vector2, Transform2, ...
│   ├── document/        # Entity, Document, TypeId
│   ├── properties/      # PropertyMap, Property
│   ├── events/          # EventBus, EntityAddedEvent
│   ├── commands/        # Command, Transaction
│   ├── registry/        # EntityRegistry, CommandRegistry, SerializerRegistry
│   ├── plugin/          # PluginRegistry, PluginManager, PluginInfo
│   └── sdk.h            # include omnibus
├── lib/                 # Bibliothèque statique ou partagée
├── cmake/               # BCADConfig.cmake
└── README.md
```

## 3. API publique

### 3.1 Headers publics

Le SDK expose uniquement les headers dans `include/bcad/`. Les détails internes (CGAL, Quadtree, SQLite) sont dans `src/bcad/internal/`.

### 3.2 Headers privés

```
include/bcad/sdk/         # ← public
src/bcad/internal/        # ← privé (jamais dans le SDK)
src/bcad/geometry/detail/ # ← privé
src/bcad/persistence/impl # ← privé
```

**Règle :** un plugin ne doit jamais inclure un header privé. CMake ne fournit que `include/bcad/` au moment de la compilation.

## 4. Installation

```cmake
# SDK CMakeLists.txt
install(TARGETS bcad-sdk
    EXPORT BCADTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/bcad DESTINATION include)

install(EXPORT BCADTargets
    FILE BCADTargets.cmake
    NAMESPACE bcad::
    DESTINATION lib/cmake/BCAD
)
```

## 5. Configuration CMake

`BCADConfig.cmake` :

```cmake
include(CMakeFindDependencyMacro)
find_dependency(Threads)

include("${CMAKE_CURRENT_LIST_DIR}/BCADTargets.cmake")
```

## 6. Utilisation par un plugin

```cmake
# Plugin CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(bcad-architecture-plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(BCAD CONFIG REQUIRED)

add_library(bcad-architecture-plugin SHARED
    src/Plugin.cpp
    src/WallEntity.cpp
    src/CreateWallCommand.cpp
    src/WallSerializer.cpp
)

target_link_libraries(bcad-architecture-plugin PRIVATE BCAD::bcad_plugin)

set_target_properties(bcad-architecture-plugin PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
```

> **Contrat plugin** : lier **uniquement** `BCAD::bcad_plugin` (jamais les
> bibliothèques de types — leurs vtables/typeinfo sont celles de l'hôte,
> partage de types §10) et passer des **fonctions libres** (pas de lambdas
> avec capture) aux factories. Voir `PLUGIN_ARCHITECTURE.md` §13.

```cpp
// Plugin.cpp
#include <bcad/plugin/PluginRegistry.h>

using namespace bcad;

extern "C" int bcad_plugin_api_version() { return plugin::PLUGIN_API_VERSION; }

extern "C" bool bcad_plugin_init(plugin::PluginRegistry& reg) {
    reg.info().name = "architecture";
    reg.info().version = "1.0.0";

    reg.registerEntityType(geom::TypeId{"arch.wall"}, &makeWall);
    reg.registerCommand("CreateWall", &makeCreateWall);
    reg.registerSerializer(std::make_unique<WallSerializer>());

    return true;
}
```

## 7. Versioning

La version vient de `project(bcad VERSION 1.0.0)` (`CMakeLists.txt`) :

- `BCADConfigVersion.cmake` filtre `find_package(BCAD 1 REQUIRED)` (compatibilité
  `SameMajorVersion`, ADR-006) ;
- `libbcad_plugin` porte `VERSION`/`SOVERSION` (major => rupture ABI) ;
- l'ABI **plugin** a son propre compteur au chargement : `PLUGIN_API_VERSION`
  (`include/bcad/plugin/PluginRegistry.h`), contrôle strict dans
  `PluginManager::loadPlugin`.

## 8. Politique de compatibilité

| Version SDK | Compatibilité |
|-------------|---------------|
| Majeure (X.y.z) | Cassure possible |
| Mineure (x.Y.z) | Ajouts, pas de cassure |
| Patch (x.y.Z) | Corrections, pas de cassure |

Suivant ADR-011, il n'y a **aucune garantie d'ABI inter-versions en v1** : un
plugin est toujours **recompilé** pour la version BCAD avec laquelle il
tourne (l'ABI C++ diffère aussi entre GCC/Clang/MSVC). Cet énoncé est plus
strict que « un plugin 1.0 marche avec BCAD 1.x » : la recompilation est
requise.

## 9. Documentation

Le SDK n'a pas encore de cible Doxygen dédiée ; chaque module documente ses
interfaces dans les headers publics (`include/bcad/`) et dans les fiches
`docs/*.md` (voir le tableau « Pour les tâches courantes » d'`AGENTS.md`).

## 10. Exemples

La preuve réelle du SDK est `examples/sdk_proof` (dépôt principal, test
`sdk_external_test`) : consommateur `BCAD::bcad_core`/`BCAD::bcad_geometry`,
plugin externe minimal (`hello_plugin`, entité + commande + serializer) et
chargeur hôte qui vérifie la médiation et les types partagés.

## 11. Règles

1. Le SDK est pur C++ (pas de Qt, OpenGL, etc.)
2. Headers publics dans `include/bcad/`
3. Installation via `install()` + `EXPORT`
4. `BCADConfig.cmake` pour `find_package(BCAD)`
5. Versionning strict
6. Documentation Doxygen
7. Exemples dans un projet séparé