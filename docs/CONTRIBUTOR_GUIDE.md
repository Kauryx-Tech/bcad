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
include/bcad/<module>/   # Headers publics (un répertoire par module)
src/<module>/            # Implémentation + CMakeLists.txt du module
src/plugins/<domaine>/   # Modules dynamiques : le métier vit ICI, pas dans le core
src/app/                 # Interface Qt (aucun littéral métier, contrôlé)
tests/
  smoke_test.cpp         # Le test historique, sans framework
  unit/<module>/         # Tests unitaires par module
  integration/           # Parcours complets (dessin → sauvegarde → impression)
docs/                    # Documentation architecturale
scripts/                 # check_arch.sh, prove_sdk.sh, prove_cadastre.sh
examples/<domaine>_proof # Projet externe qui consomme le SDK installé
docs/schemas/            # Diagrammes Draw.io (non régénérés)
```

`tests/CMakeLists.txt` enregistre un exécutable par test avec
`add_test(...)` : c'est ce fichier, pas `smoke_test.cpp`, qui décide de ce que
`ctest` exécute.

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

- `bcad::Result<T>` **n'existe pas**. Le code rendu public exprime l'échec
  comme il le fait aujourd'hui : `bool` (chargement d'un module, exécution
  d'une règle), `nullptr` (factory, `loadPlugin`, `deserialize`), ou un
  argument de sortie (`std::string* reason`) quand l'appelant doit afficher
  pourquoi ça a échoué.
- Une opération d'E/S qui ne peut pas produire son fichier doit **échouer
  visiblement**, pas rendre `true` sans rien écrire.
- Exceptions réservées aux erreurs de programmation (invariant violé) ; les
  commandes et le core n'en propagent aucune à l'UI.
- Un refus métier remonte en `Diagnostic` (sévérité + message porté par le
  module), jamais en `QString` construit dans `src/app`.

## Tests

### Style de test : assertions maison, sans framework

BCAD n'utilise aucun framework de test externe. `tests/smoke_test.cpp` garde le
pattern historique (une fonction par module, compteur de échecs) :

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

- Pas de framework de test externe : `check()`, `assert()`, code de retour.
- Un exécutable par sujet, déclaré dans `tests/CMakeLists.txt` ; les tests
  unitaires vivent dans `tests/unit/<module>/`, les parcours complets dans
  `tests/integration/`.
- `assert()` est **mort en RelWithDebInfo** (`-DNDEBUG`), qui est le type de
  build des preuves : un test qui n'utilise que `assert()` peut réussir sans
  rien vérifier. La parade est globale : `tests/CMakeLists.txt` pose
  `add_compile_options(-UNDEBUG)` pour toute la sous-direction, donc une
  nouvelle cible de test n'a rien à déclarer pour vérifier vraiment. Ne pas
  retirer cette ligne, et ne pas la redéclarer par cible.
- Ne jamais mettre d'effet de bord dans un `assert()` : le même `-DNDEBUG`
  ferait disparaître l'appel, pas seulement la vérification.
- Un test qui ne peut pas échouer n'est pas une preuve : avant de compter sur
  lui, invalider volontairement le code testé et vérifier que le test tombe.
- Modifier un test existant demande l'accord du mainteneur (`AGENTS.md`) : on
  n'ajuste pas une assertion pour faire passer un build.

### Ce que la suite vérifie au-delà des comportements

| Commande | Vérifie |
|----------|---------|
| `ctest --test-dir build` | comportements, roundtrip, ABI, et les deux preuves externes qui compilent un projet hors arbre |
| `scripts/check_arch.sh` | frontières de couches : Qt hors du core, CGAL hors de `include/bcad/`, `core ↛ render`, aucun nom de module ni littéral métier dans `src/app` |
| `scripts/prove_sdk.sh` | `find_package(BCAD)` sur une installation réelle, plugin externe chargé |
| `scripts/prove_cadastre.sh` | cycle complet du module cadastral : install, compilation externe, `dlopen`, registres, déchargement |

`ci.yml` lance configure + build + `ctest`. Les preuves externes font partie de
`ctest` ; `check_arch.sh`, lui, n'est **pas** appelé par la CI : à exécuter à la
main avant une PR qui touche aux couches.

## PR

### Workflow
1. Fork
2. Branche `feature/xyz`
3. Commit
4. Tests locaux (`ctest`)
5. Push
6. PR vers `main`

### Checklist PR
- [ ] Tests ajoutés, et le nouveau test a été vu **échouer** une fois le code
      source invalidé
- [ ] `scripts/check_arch.sh` passe
- [ ] `cmake --build build && ctest --test-dir build --output-on-failure` passe
- [ ] `clang-format` appliqué
- [ ] Pas de warnings (`-Wall -Wextra -Wpedantic`)
- [ ] Doxygen si nouvelle API publique
- [ ] `PLUGIN_API_VERSION` incrémentée **et** `docs/API_ABI_POLICY.md` à jour si
      le layout ou les signatures traversant un module ont changé
- [ ] Documentation d'état (`CADASTRE_PLUGIN_STATUS.md`, `CADASTRAL_AUDIT_2026.md`)
      corrigée si elle promettait ce que le lot change
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

- `main` : build + `ctest` en CI (Ubuntu, paquets système, pas de vcpkg).
  Une CI verte n'est pas une preuve d'architecture : `check_arch.sh` reste à
  lancer à la main.
- Nombre de reviews : aucune règle n'est automatisée dans le dépôt — deux
  relectures sont la convention, pas une garde.
- Aucune mesure de couverture n'est outillée : « la couverture ne doit pas
  baisser » n'est donc pas vérifiable ici, ce qui se remplace par la règle du
  test vu-échouer ci-dessus.

## Voir aussi

- `../CONTRIBUTING.md` (racine) — guide général
- `ARCHITECTURE_PRINCIPLES.md` — principes
- `TESTING_ARCHITECTURE.md` — tests
- `BUILD_AND_PACKAGING.md` — build
- `API_ABI_POLICY.md` — règles API/ABI