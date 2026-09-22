# Architecture du SDK BCAD

> [!IMPORTANT]
>
> ## Statut : ARCHITECTURE CIBLE — non implémentée
>
> Il n'y a **aucun SDK** aujourd'hui : pas d'`install()`/`export()` CMake, pas de headers
> `bcad/sdk.h` ni `Version.h`. CGAL et le Quadtree sont d'ailleurs encore exposés publiquement
> (`include/bcad/geometry/Types.h`, `include/bcad/core/Document.h`) — voir `ARCHITECTURE_REVIEW.md`.
> Ce document est la fiche de conception de la future surface SDK.

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

target_link_libraries(bcad-architecture-plugin PRIVATE bcad::sdk)

set_target_properties(bcad-architecture-plugin PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
```

```cpp
// Plugin.cpp
#include <bcad/sdk.h>

using namespace bcad;

extern "C" bool bcad_plugin_init(plugin::PluginRegistry& reg) {
    reg.info().name = "architecture";
    reg.info().version = "1.0.0";

    reg.registerEntityType(geom::TypeId{"arch.wall"}, /* factory */);
    reg.registerCommand("CreateWall", /* factory */);
    reg.registerSerializer(std::make_unique<WallSerializer>());

    return true;
}
```

## 7. Versioning

```cpp
// include/bcad/sdk/Version.h
namespace bcad {
constexpr int kSDKVersionMajor = 1;
constexpr int kSDKVersionMinor = 0;
constexpr int kSDKVersionPatch = 0;
constexpr const char* kSDKVersionString = "1.0.0";
}
```

Le SDK est versionné indépendamment de l'application BCAD.

## 8. Politique de compatibilité

| Version SDK | Compatibilité |
|-------------|---------------|
| Majeure (X.y.z) | Cassure possible |
| Mineure (x.Y.z) | Ajouts, pas de cassure |
| Patch (x.y.Z) | Corrections, pas de cassure |

Un plugin compilé avec SDK 1.0 fonctionne avec BCAD 1.x mais pas 2.x.

## 9. Documentation

Le SDK est documenté via Doxygen :

```cmake
find_package(Doxygen)
if(DOXYGEN_FOUND)
    doxygen_add_docs(bcad-sdk-docs
        include/bcad/
        COMMENT "Generate SDK documentation"
    )
endif()
```

## 10. Exemples

Un projet séparé `bcad-sdk-examples` contient :

- `example-plugin-hello` : plugin minimal
- `example-entity-wall` : entité personnalisée
- `example-command` : commande
- `example-serializer` : serializer personnalisé
- `example-external-plugin` : proof of architecture

## 11. Règles

1. Le SDK est pur C++ (pas de Qt, OpenGL, etc.)
2. Headers publics dans `include/bcad/`
3. Installation via `install()` + `EXPORT`
4. `BCADConfig.cmake` pour `find_package(BCAD)`
5. Versionning strict
6. Documentation Doxygen
7. Exemples dans un projet séparé