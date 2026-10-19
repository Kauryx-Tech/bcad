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
cmake --build build -j2

# Tester
ctest --test-dir build --output-on-failure
```

Le hook pre-commit local (`.githooks/pre-commit`) peut être activé :

```bash
git config core.hooksPath .githooks
```

## Structure du code

```
include/bcad/<module>/   # Headers PUBLICS : installés, exportés, contractuels
src/<module>/            # Implémentation + CMakeLists.txt du module
src/plugins/<domaine>/   # Modules dynamiques : le métier vit ICI, pas dans le core
src/app/                 # Interface Qt de l'hôte — privée, non installée, non contractuelle
  window/                #   MainWindow (une classe, plusieurs unités), ruban, échelle d'interface
  viewport/              #   Viewport (canevas et outils), accrochage, tessellation, alias clavier
  detail/                #   calculs purs sans Qt (*Detail.h, saisie de coordonnées, presse-papiers)
  widgets/               #   palettes et champs : calques, propriétés, ligne de commande, dialogues
  editing/               #   commandes annulables (Qt), EditBatch, sessions des dessins ouverts
  theme/, icons/         #   design system et icônes SVG embarquées
tests/
  smoke_test.cpp         # Le test historique, sans framework
  unit/<module>/         # Tests unitaires par module
  integration/           # Parcours complets (dessin → sauvegarde → impression)
docs/                    # Documentation architecturale
scripts/                 # check_arch.sh, prove_sdk.sh, prove_cadastre.sh
examples/<domaine>_proof # Projet externe qui consomme le SDK installé
docs/schemas/            # Diagrammes Draw.io (non régénérés)
```

La ligne de partage est une règle, pas une convention de rangement : tout ce qui
est sous `include/bcad/` est une promesse faite aux consommateurs du SDK
(installé, linkable, sans CGAL ni Qt), et ce qui est sous `src/` peut changer
sans avis. Un en-tête d'hôte placé sous `include/bcad/` donnerait une API qui
n'est pas linkable — le binaire `bcad` n'est pas exporté. Voir
`VISUAL_ARCHITECTURE.md` §6.4.

`tests/CMakeLists.txt` enregistre un exécutable par test avec
`add_test(...)` : c'est ce fichier, pas `smoke_test.cpp`, qui décide de ce que
`ctest` exécute.

## Normes de code

Ces règles décrivent le code tel qu'il est écrit, et les outils en vérifient
une partie (`.editorconfig`, `.clang-format`, `.clang-tidy`,
`scripts/check_arch.sh`, `cargo fmt` / `clippy`). L'interface a sa propre
référence : [`DESIGN_SYSTEM.md`](DESIGN_SYSTEM.md).

### Nommage (C++)

| Élément | Forme | Exemple |
|---|---|---|
| Fichier | PascalCase, une classe ou une responsabilité | `ViewportFilletTools.cpp`, `ModifyDetail.h` |
| Classe, struct, `enum class` et ses valeurs | PascalCase | `EditBatch`, `ToolMode::Fillet` |
| Fonction, méthode, variable | camelCase | `pickEntityAt`, `filletRadiusMemo_` |
| Membre privé | camelCase + `_` | `toolPoints_` |
| Constante | `k` + PascalCase | `kPickToleranceScreenPx`, `theme::kIconRibbonLarge` |
| Espace de noms | minuscules, par module | `bcad::geom`, `bcad::app` ; calcul pur : `bcad::app::detail` |

C++20, quatre espaces, 110 colonnes (`.clang-format`).

### Organisation

1. **Une responsabilité par fichier.** `Viewport` et `MainWindow` sont répartis
   en unités `Viewport<Famille>Tools.cpp` / `MainWindow<Rôle>.cpp`, de 350 lignes
   au plus (`check_arch.sh`) ; la carte des unités est en tête de
   `viewport/Viewport.cpp` et `window/MainWindow.cpp`. Une nouvelle famille
   d'outils = une nouvelle unité dans `src/app/viewport/`, ajoutée à
   `src/app/CMakeLists.txt` et à la liste du test du canevas
   (`tests/CMakeLists.txt`). Les `#include` de `src/app` partent de sa racine :
   `#include "viewport/Viewport.h"`.
2. **Le calcul pur à part.** La géométrie d'un outil (raccord, décalage,
   mesure…) vit dans un en-tête `src/app/detail/*Detail.h`, espace `detail`,
   sans Qt ni état : le même code sert l'exécution et l'aperçu, et se teste
   seul.
3. **Les données en tables.** Outils (`kTools`), alias (`kToolAliases`),
   jetons visuels (`Theme.cpp`), panneaux du ruban (`MainWindowMenus.cpp`) : on
   ajoute une ligne, pas un cas particulier dans le code.
4. **En-têtes publics** (`include/bcad/`) : sans Qt ni CGAL, chaque en-tête
   inclut ce qu'il utilise, déclarations anticipées quand c'est possible, pas de
   `using namespace`.

### Code commun : chercher avant d'écrire

Avant d'écrire un calcul ou une boucle, vérifier qu'il n'existe pas déjà. Une
troisième copie d'un même motif devient une fonction partagée (et pas avant :
pas d'abstraction spéculative).

| Besoin | Utiliser | Où |
|---|---|---|
| Modifier le dessin en un pas annulable | `EditBatch` (`add`, `remove`, `replace`, `transform`) | `src/app/editing/EditBatch.h` |
| Créer un objet dessiné (calque courant) | `Viewport::commitEntity` | `Viewport.cpp` |
| Objet sous le curseur, tolérance | `pickEntityAt`, `pickTolerance` | `Viewport.cpp` |
| Monde → écran, fantôme d'aperçu | `toScreenF`, `drawGhost` | `Viewport.cpp` |
| Couleur, taille, police | `theme::color(Token)`, `theme::k…` | `theme/Theme.h` |
| Bouton à variantes, icône seule, panneau étendu, widget | `RibbonBar::splitAction`, `iconOnly`, argument `overflow`, `QWidgetAction` | `RibbonBar.h` |
| Raccord, chanfrein, coupe côté gardé | `detail::planFillet`, `filletBetween`, `trimKeeping` | `ModifyDetail.h` |
| Décaler, étirer, contour de hachures | `detail::offsetClone`, `stretchClone`, `hatchBoundaryAt` | `ModifyDetail.h` |
| Distance, angle, aire, périmètre | `detail::measureDistance`, `measureAngleDeg`, `ringArea`, `ringPerimeter` | `MeasureDetail.h` |
| Poignées d'un objet, objet après une poignée tirée | `detail::gripsOf`, `moveGrip` | `GripDetail.h` |
| Copier / coller des objets entre dessins | `EntityClipboard::instance()` | `EntityClipboard.h` |
| Cercle 2P / 3P, arc 3 points | `detail::circleFrom2P`, `circleFrom3P`, `arcFrom3P` | `CircleDetail.h` |

`check_arch.sh` (règle 18) refuse une tolérance de désignation recalculée et une
macro d'annulation écrite à la main dans les outils du canevas.

### Écriture

- **Fonctions courtes**, retour anticipé plutôt qu'imbrication ; un nom dit ce
  que fait la fonction, un commentaire dit **pourquoi**.
- **Commentaires en français**, en tête de fichier : sa responsabilité ; dans le
  code : la raison d'un choix (règle AutoCAD, piège évité). Pas de journal de
  modifications dans le code (c'est le rôle de Git).
- **Propriété explicite** : `std::unique_ptr` pour posséder, pointeur nu pour
  observer ; `new` seulement pour un objet confié à Qt (parent, `QUndoStack`).
- `std::optional` plutôt qu'une valeur sentinelle ; `enum class` ; `const`
  partout où c'est vrai ; casts explicites ; pas de macro.
- **Pas de nombre magique** : tolérances dans `geom::Tolerance`, valeurs
  visuelles dans le design system, constantes nommées sinon.
- **Qt** : toute chaîne visible par `tr()` ; un refus va en barre d'état
  (`statusMessage`), jamais en boîte modale ; aucune couleur ni taille en dur.

### Rust (`rust/`)

- `cargo fmt` ; `cargo clippy --workspace --all-targets -- -D warnings` avec les
  lints du workspace (`rust/Cargo.toml` : clippy *all*, *pedantic*, *nursery*) ; noms de code dans
  la documentation entre apostrophes inverses.
- `unsafe` seulement dans `bcad-ffi`, chaque bloc précédé d'un commentaire
  `// SAFETY:` qui dit ce que l'appelant garantit.
- Le comportement suit la spécification DXF d'AutoCAD ; un test épingle chaque
  règle lue (ex. calque éteint = couleur négative).

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
| `scripts/check_arch.sh` | frontières de couches : aucun Qt dans les en-têtes publics (`include/bcad/` entier), CGAL hors de `include/bcad/`, `core ↛ render`, aucun nom de module ni littéral métier dans `src/app` |
| `scripts/prove_sdk.sh` | `find_package(BCAD)` sur une installation réelle vidée au préalable, absence d'en-têtes privés d'hôte dans l'install, plugin externe chargé |
| `scripts/prove_cadastre.sh` | cycle complet du module cadastral : install, compilation externe, `dlopen`, registres, déchargement |

`ci.yml` lance `check_arch.sh` **en premier**, avant même d'installer les
dépendances : ses règles portent sur les sources seules, donc une violation doit
coûter quelques secondes et non un build Qt/CGAL complet. Puis configure + build +
`ctest`. Les preuves externes font partie de `ctest`.

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
- [ ] `clang-format` appliqué ; Rust : `cargo fmt`, `cargo clippy -- -D warnings`, `cargo test`
- [ ] Interface : valeurs visuelles par jetons, outil à sa place AutoCAD ([`DESIGN_SYSTEM.md`](DESIGN_SYSTEM.md))
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

- `main` : `check_arch.sh` + build + `ctest` en CI (Ubuntu, paquets système, pas
  de vcpkg). Le contrôle d'architecture y est une étape, donc une violation bloque
  la branche ; ce que la CI ne vérifie pas pour autant, c'est ce que le script ne
  sait pas exprimer (voir la liste de ses règles dans le script même).
- Nombre de reviews : aucune règle n'est automatisée dans le dépôt — deux
  relectures sont la convention, pas une garde.
- Aucune mesure de couverture n'est outillée : « la couverture ne doit pas
  baisser » n'est donc pas vérifiable ici, ce qui se remplace par la règle du
  test vu-échouer ci-dessus.

## Voir aussi

- `../CONTRIBUTING.md` (racine) — porte d'entrée
- `DESIGN_SYSTEM.md` — interface : jetons, composants, motifs, recettes
- `ARCHITECTURE_PRINCIPLES.md` — principes
- `TESTING_ARCHITECTURE.md` — tests
- `BUILD_AND_PACKAGING.md` — build
- `API_ABI_POLICY.md` — règles API/ABI