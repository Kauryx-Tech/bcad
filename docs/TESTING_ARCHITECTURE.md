# Architecture de tests BCAD

> Stratégie de test couvrant tous les aspects du Core, des services, et des plugins.

## 1. Niveaux de tests

| Niveau | Cible | Framework |
|--------|-------|-----------|
| **Unit** | Une classe, une fonction | doctest / Catch2 |
| **Module** | Un module complet | doctest / Catch2 |
| **Intégration** | Modules combinés | doctest / Catch2 |
| **Système** | Application complète | Tests manuels + scripts |
| **Proof of architecture** | Plugin externe | Repo séparé |

## 2. Couverture par sujet

### 2.1 Geometry

- Point2/Point3, Vector2/Vector3
- Transform2/Transform3 (translation, rotation, scale, mirror)
- BoundingBox2/BoundingBox3 (intersection, expansion)
- Polygon2 (containment)
- BooleanOps (union, intersection, difference, symmetric difference)
- Triangulation (Delaunay, constrained Delaunay)
- Distance, projection, angle

### 2.2 Entities

- Line/Circle/Arc/Polyline creation
- tessellate() output
- distanceTo()
- transform application
- boundingBox

### 2.3 Document

- addEntity / removeEntity
- transaction begin/commit/rollback
- undo/redo
- selection
- layer management
- spatial index query
- event bus
- dirty state

### 2.4 Properties

- PropertyMap add/get/set
- Types: Double, Int, String, Bool, Color, Enum
- Constraints (range, enum values)
- Read-only

### 2.5 Events

- subscribe/unsubscribe
- publish
- sync vs queued
- multiple subscribers

### 2.6 Commands

- execute/undo
- transactions (multi-command)
- repeat (isRepeatable)

### 2.7 Persistence

- Serializer (each type)
- Version migration
- Roundtrip (save/load)
- Format detection

### 2.8 Plugins

- Plugin loading
- PluginInfo validation
- Version check
- Entity registration via plugin
- Command registration via plugin
- Serializer registration via plugin
- Unload safety

### 2.9 SDK

- find_package(BCAD) successful
- Linking against bcad::sdk
- Headers self-contained

### 2.10 Rendering

- Tessellator output (CPU backend)
- Scene extraction
- Camera transforms
- LOD computation

### 2.11 Concurrency

- Document thread-safety (read/write lock)
- Multiple readers concurrent
- Writer exclusivity

### 2.12 2D

- All 2D entity types
- Quadtree index
- 2D camera

### 2.13 3D

- 3D entity types (futur)
- Octree index
- 3D camera

## 3. Test "ExternalPlugin" (Proof of architecture)

Ce test est **critique** : il constitue le proof of architecture.

### 3.1 Caractéristiques

- Dans un **projet séparé** du repository BCAD
- Compile avec `find_package(BCAD CONFIG REQUIRED)`
- Ne modifie pas le Core

### 3.2 Scénario

1. Trouver BCAD avec `find_package`
2. Compiler le plugin
3. Charger le plugin dynamiquement
4. Enregistrer une commande via le plugin
5. Enregistrer une entité via le plugin
6. Définir des propriétés
7. Sauvegarder l'entité dans un fichier `.bcad`
8. Recharger l'entité depuis le fichier
9. Vérifier que tout fonctionne sans modification du Core

### 3.3 Code

```cpp
// external-plugin/Plugin.cpp
#include <bcad/sdk.h>

class TestEntity : public bcad::document::Entity {
    // ...
};

class TestCommand : public bcad::commands::Command {
    // ...
};

extern "C" void bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    reg.info().name = "external-test";
    reg.info().version = "1.0.0";
    reg.info().requiresBCAD = "1.0.0";

    reg.entityRegistry().registerType<TestEntity>();
    reg.commandRegistry().registerCommand<TestCommand>("TestCmd");
}
```

```cmake
# external-plugin/CMakeLists.txt
find_package(BCAD CONFIG REQUIRED)
add_library(bcad-external-test SHARED Plugin.cpp)
target_link_libraries(bcad-external-test PRIVATE bcad::sdk)
```

```cpp
// tests/external_plugin_test.cpp
TEST_CASE("External plugin can be loaded and used") {
    bcad::plugin::PluginManager mgr;
    mgr.setSearchPaths({"plugins/"});

    auto* plugin = mgr.load("libbcad-external-test.so");
    REQUIRE(plugin != nullptr);
    REQUIRE(plugin->info().name == "external-test");

    // Verify the entity was registered
    auto* doc = mgr.document();
    auto* entity = doc->addEntity({"external-test", "TestEntity", ...});
    REQUIRE(entity != nullptr);

    // Verify the command can be created
    auto* cmd = mgr.commandRegistry().create("TestCmd");
    REQUIRE(cmd != nullptr);

    // Save and reload
    mgr.save("test.bcad");
    auto* doc2 = mgr.load("test.bcad");
    REQUIRE(doc2->findEntity(entity->id()) != nullptr);

    mgr.unload(plugin);
}
```

## 4. Frameworks

- **doctest** : léger, single-header, simple
- **Catch2** : plus riche, plus de fonctionnalités

Choix : **doctest** pour le Core, **Catch2** pour les tests d'intégration.

## 5. Exécution

```bash
# Tests unitaires
ctest --test-dir build --output-on-failure

# Tests par module
./build/tests/geometry_tests
./build/tests/document_tests

# Test external plugin (proof of architecture)
./build/tests/external_plugin_test
```

## 6. CI

```yaml
# .github/workflows/ci.yml
- name: Tests
  run: |
    cmake --build build
    ctest --test-dir build --output-on-failure
```

## 7. Règles

1. Chaque module a ses tests
2. Tests d'intégration entre modules
3. Test "ExternalPlugin" obligatoire
4. Tests de régression pour chaque bug
5. Coverage > 80% pour le Core
6. Tests dans CI