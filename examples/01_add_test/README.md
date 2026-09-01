# Exemple 1 : Ajouter un test

> Comment ajouter un test dans `tests/smoke_test.cpp`.

## Contexte

BCAD utilise un système de test minimaliste : `tests/smoke_test.cpp`.
Pas de framework de test (pas de doctest, Catch2, etc.) — juste des `check()`.

## Le code actuel

```cpp
// tests/smoke_test.cpp
namespace {
int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}
} // namespace
```

## Ajouter un test

### 1. Créer une fonction de test

```cpp
void test_my_feature() {
    // Ton code de test ici
    check(someCondition, "my feature works");
}
```

### 2. L'appeler dans `main()`

```cpp
int main() {
    testGeometryUtils();
    testEntities();
    // ... autres tests existants ...
    test_my_feature();  // ← ajouter ici
    
    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d test(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nAll tests passed\n");
    return 0;
}
```

## Exemple complet

```cpp
#include "bcad/geometry/Line.h"

void test_line_length() {
    using namespace bcad::geom;
    
    // Crée une ligne de (0,0) à (3,4)
    LineEntity line(Point2(0, 0), Point2(3, 4));
    
    // Vérifie la longueur (3-4-5 triangle)
    check(std::abs(line.length() - 5.0) < 1e-9, 
          "line length is 5.0");
    
    // Vérifie la bounding box
    auto bb = line.boundingBox();
    check(bb.minX == 0 && bb.minY == 0, "bounding box is correct");
    check(bb.maxX == 3 && bb.maxY == 4, "bounding box is correct");
}
```

## Commandes

```bash
# Compiler et exécuter
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Notes

- Le test est **compilatif** : il vérifie que le code compile et fonctionne
- Pour des tests plus complets, voir `docs/TESTING_ARCHITECTURE.md`
- L'architecture cible utilise `doctest` (voir Phase 5 de la roadmap)