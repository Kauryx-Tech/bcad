# Build et packaging BCAD

> [!IMPORTANT]
>
> ## Statut : MIXTE — état actuel + architecture cible
>
> La section **1** décrit l'état **réel** (aucun export CMake, `find_package(BCAD)` impossible).
> Les sections **2 à 8** décrivent l'**architecture cible** (installation/export, targets `bcad::`,
> packaging) **non implémentée**.

> Cible CMake : installation, export, find_package, packaging.

## 1. État actuel

Le CMakeLists.txt actuel ne fait pas d'export. Impossible de faire `find_package(BCAD)`.

## 2. Architecture cible

### 2.1 Structure

```
bcad/
├── CMakeLists.txt          # project(bcad) + add_subdirectory(src/)
├── src/
│   ├── core/CMakeLists.txt
│   ├── geometry/CMakeLists.txt
│   └── ...
├── include/bcad/           # headers publics
├── cmake/                  # modules CMake internes
│   ├── BCADCompilerSettings.cmake
│   └── BCADDependencies.cmake
├── install/                # contenu de l'installation
│   ├── bin/
│   ├── lib/
│   ├── include/
│   └── share/bcad/
│       └── cmake/
└── package/                # fichiers de packaging
```

### 2.2 Cibles CMake

```
bcad::core          (bibliothèque)
bcad::geometry      (bibliothèque)
bcad::document      (bibliothèque)
bcad::commands      (bibliothèque)
bcad::events        (bibliothèque)
bcad::properties    (bibliothèque)
bcad::index         (bibliothèque)
bcad::persistence   (bibliothèque)
bcad::io            (bibliothèque)
bcad::render        (bibliothèque)
bcad::sdk           (bibliothèque, exportée)
bcad::app           (exécutable)
```

## 3. Export et installation

### 3.1 Installation

```cmake
# src/CMakeLists.txt
install(TARGETS bcad::core bcad::geometry bcad::document
    EXPORT BCADCoreTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(TARGETS bcad::sdk
    EXPORT BCADTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include/bcad
)

# Headers publics
install(DIRECTORY include/bcad
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# CMake config
install(EXPORT BCADTargets
    FILE BCADTargets.cmake
    NAMESPACE bcad::
    DESTINATION lib/cmake/BCAD
)

install(EXPORT BCADCoreTargets
    FILE BCADCoreTargets.cmake
    NAMESPACE bcad::
    DESTINATION lib/cmake/BCAD
)

install(FILES
    cmake/BCADConfig.cmake
    cmake/BCADVersion.cmake
    DESTINATION lib/cmake/BCAD
)
```

### 3.2 BCADConfig.cmake

```cmake
# cmake/BCADConfig.cmake
include(CMakeFindDependencyMacro)

# Dépendances
find_dependency(Threads REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/BCADTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/BCADCoreTargets.cmake")
```

### 3.3 BCADVersion.cmake

```cmake
# cmake/BCADVersion.cmake
set(BCAD_VERSION "1.0.0")
set(BCAD_VERSION_MAJOR 1)
set(BCAD_VERSION_MINOR 0)
set(BCAD_VERSION_PATCH 0)
```

## 4. Utilisation

### 4.1 Application

```cmake
find_package(BCAD CONFIG REQUIRED COMPONENTS core geometry document)

add_executable(my-app main.cpp)
target_link_libraries(my-app PRIVATE bcad::core bcad::geometry)
```

### 4.2 Plugin

```cmake
find_package(BCAD CONFIG REQUIRED)

add_library(bcad-my-plugin SHARED src/Plugin.cpp)
target_link_libraries(bcad-my-plugin PRIVATE bcad::sdk)
```

### 4.3 vcpkg

```json
{
  "name": "bcad",
  "version": "1.0.0",
  "description": "CAD application",
  "dependencies": ["bcad"]
}
```

## 5. Plugins

```cmake
# Plugins sont des cibles PRIVATE de bcad::app
# Installés dans le répertoire plugins
install(TARGETS bcad-my-plugin
    LIBRARY DESTINATION plugins
)

# Ou dans un sous-répertoire
install(TARGETS bcad-my-plugin
    LIBRARY DESTINATION lib/bcad/plugins
)
```

## 6. Installation

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build

# Installation
cmake --install build

# Désinstallation
xargs rm < build/install_manifest.txt
```

## 7. Tests

```cmake
enable_testing()
add_subdirectory(tests)

# Config pour vcpkg
if("$ENV{VCPKG_CHAINLONG}")
    create_per_config_helpers()
    vcpkg_configure_cmake(SOURCE_PATH ${CMAKE_CURRENT_LIST_DIR}
        DISABLE_PARALLEL_CONFIGURATION
        DISABLE_PARALLEL_BUILD
        OPTIONS -DVCPKG_CHAINLONG=ON)
endif()
```

## 8. Règles

1. `install()` pour tous les targets publics
2. `EXPORT BCADTargets` pour find_package
3. `BCADConfig.cmake` complet
4. `BCADVersion.cmake` pour le versioning
5. Headers publics dans `include/bcad/`
6. Plugins dans `plugins/` ou `lib/bcad/plugins`
7. Tests activés avec `enable_testing()`