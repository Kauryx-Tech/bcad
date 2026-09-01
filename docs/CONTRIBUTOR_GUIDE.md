# Guide du contributeur BCAD

> Comment contribuer au Core BCAD : standards, tests, PR.

## Bienvenue

BCAD est GPL-3.0. Toute contribution au Core doit passer par PR avec tests.

## Prérequis

- C++20 (GCC 11+, Clang 14+, MSVC 17+)
- CMake 3.20+
- vcpkg
- Git
- Compréhension des `ARCHITECTURE_PRINCIPLES.md`

## Setup

```bash
git clone https://github.com/Kauryx-Tech/bcad.git
cd bcad

# Configurer (preset vcpkg) :
cmake --preset vcpkg
# OU configuration manuelle (apt) :
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compiler
cmake --build build -j

# Tester
ctest --test-dir build --output-on-failure
```

Le hook pre-commit local (`.githooks/pre-commit`) peut être activé :

```bash
git config core.hooksPath .githooks
```

## Structure du code

```
include/bcad/       # Headers publics (un répertoire par module)
src/<module>/       # Implémentation + CMakeLists.txt du module
tests/              # smoke_test.cpp (pas de framework de test externe)
docs/               # Documentation architecturale
examples/           # Exemples pratiques
schemas/            # Sous-dossier de docs/ (Draw.io)
```

## Standards C++

### Style
- clang-format (config dans `.clang-format`)
- snake_case fichiers, PascalCase classes, snake_case_ fonctions
- C++20 (pas de C++23 pour rester compatible)

### Règles

1. **Pas d'inclusion transitive** — chaque header inclut ce qu'il utilise
2. **Forward declarations** quand possible dans les headers
3. **`noexcept`** marqué sur les fonctions garanties sans exception
4. **Pas de `using namespace` dans les headers**
5. **Pas de macros** sauf constexpr/templates
6. **Casts explicites** (`static_cast`, pas de C-style)
7. **`std::optional`**, `std::variant`, `std::span` quand approprié

### Erreurs

- `bcad::Result<T>` plutôt qu'exceptions
- Ou exceptions seulement pour erreurs "fatal" (programmation)

## Tests

### Framework actuel : smoke_test.cpp

BCAD utilise un système de test minimaliste sans framework externe. Une fonction par module dans `tests/smoke_test.cpp` :

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

void testGeometryUtils() {
    geom::Point2 a(0, 0), b(3, 4);
    check(std::abs(geom::distance(a, b) - 5.0) < 1e-9, "distance(3-4-5 triangle)");
}

int main() {
    testGeometryUtils();
    // ... autres tests ...
    if (g_failures > 0) {
        return 1;
    }
    return 0;
}
```

### Règles
- Pas de framework de test externe
- Une fonction par module
- `check()` pour les assertions
- Pas de `bcad::Result<T>` (pas encore utilisé)
- Pas de sous-dossiers `tests/unit/`, `tests/integration/`, `tests/perf/`

## PR

### Workflow
1. Fork
2. Branche `feature/xyz`
3. Commit
4. Tests locaux (`ctest`)
5. Push
6. PR vers `main`

### Checklist PR
- [ ] Tests unitaires ajoutés
- [ ] `clang-format` appliqué
- [ ] Pas de warnings (`-Wall -Wextra -Wpedantic`)
- [ ] Doxygen si nouvelle API publique
- [ ] ADR si décision architecturale

### Commit messages
```
type(scope): description courte

Description détaillée si nécessaire.

Refs: #123
```

Types : `feat`, `fix`, `refactor`, `docs`, `test`, `perf`, `chore`.

## Décisions architecturales

Toute décision importante doit être ajoutée à `docs/ARCHITECTURE_DECISIONS.md` avec un nouvel ADR.

## Review

- 2 reviews minimum
- CI doit passer
- Pas de merge si couverture baisse

## Voir aussi

- `../CONTRIBUTING.md` (racine) — guide général
- `ARCHITECTURE_PRINCIPLES.md` — principes
- `TESTING_ARCHITECTURE.md` — tests
- `BUILD_AND_PACKAGING.md` — build
- `API_ABI_POLICY.md` — règles API/ABI