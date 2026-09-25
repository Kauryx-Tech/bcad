# Guide de démarrage rapide BCAD

> **Tu as 5 minutes ?** Ce guide te permet de compiler et lancer BCAD en moins de 5 minutes.

## Prérequis

Assure-toi d'avoir installé les dépendances :

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake qt6-base-dev libqt6opengl6-dev \
    libcgal-dev libboost-dev libboost-all-dev libsqlite3-dev \
    libgl1-mesa-dev libglu1-mesa-dev
```

Sous Windows/macOS, ou pour figer les versions, le manifest `vcpkg.json`
apporte les mêmes dépendances (`cgal`, `sqlite3`, `qtbase[widgets,opengl]`) :

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

Le projet exige C++20, Qt 6 (Widgets, OpenGLWidgets, PrintSupport), CGAL,
OpenGL 3.3 et SQLite3 — ce sont les cinq `find_package` de `CMakeLists.txt`.

## Étape 1 : Cloner le projet (30 secondes)

```bash
git clone https://github.com/Kauryx-Tech/bcad.git
cd bcad
```

## Étape 2 : Configurer (1 minute)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

**Si ça échoue :** Vérifie les dépendances manquées. L'erreur indique quel package installer.

## Étape 3 : Compiler (2-3 minutes)

```bash
cmake --build build -j$(nproc)
```

## Étape 4 : Lancer les tests (30 secondes)

```bash
ctest --test-dir build --output-on-failure
```

Le nombre de tests suit `tests/CMakeLists.txt` (30 à ce jour). La fin de
sortie doit ressembler à :

```
30/30 Test #30: cadastre_validators_test .....   Passed

100% tests passed, 0 tests failed out of 30
```

Deux tests prennent plusieurs secondes parce qu'ils compilent et installent
des projets hors arbre (`sdk_external_test`, `cadastre_external_test`) : ne
les retire pas pour « aller plus vite », ce sont les seules preuves que le SDK
et les modules dynamiques fonctionnent réellement.

## Étape 5 : Lancer l'application (30 secondes)

```bash
./build/src/app/bcad
```

**Félicitations !** 🎉 BCAD est maintenant lancé. Tu vois une fenêtre avec :
- Un ruban à onglets français (`Accueil`, `Modifier`, `Affichage`, `Annoter`,
  plus un onglet par module chargé)
- Le canevas de dessin au centre
- Les docks `Calques`, `Propriétés` et `Vérifications`
- Une ligne de commande et une barre d'état sous le canevas

L'application ne connaît aucun module : elle cherche ses plugins dans
`$BCAD_PLUGIN_PATH`, `lib/bcad/plugins` (installation), l'arbre de build et
`$XDG_DATA_HOME/bcad/plugins`, et construit ses menus à partir de ce qui est
déclaré. Le module cadastral est compilé par le dépôt : sans lui, seul le ruban
générique apparaît.

## Première action : dessiner une ligne

1. Clique sur l'outil **Ligne** dans le ruban (onglet `Accueil`, panneau `Dessin`)
2. Clique quelque part sur le canevas pour le point de départ
3. Clique ailleurs pour le point d'arrivée
4. Appuie sur **Entrée** ou **clic droit** pour terminer

**Ça y est !** Tu as dessiné ta première entité.

## Prochaines étapes

| Tu veux... | Lis... |
|------------|--------|
| Comprendre ce que tu viens de voir | [VISUAL_ARCHITECTURE.md](VISUAL_ARCHITECTURE.md) |
| Explorer le code | [WALKTHROUGH.md](WALKTHROUGH.md) |
| Contribuer | [FIRST_CONTRIBUTION.md](FIRST_CONTRIBUTION.md) |
| Résoudre un problème | [TROUBLESHOOTING.md](TROUBLESHOOTING.md) |

## Commandes clavier essentielles

| Raccourci | Action |
|-----------|--------|
| `F` | Zoom sur l'ensemble |
| `F3` | Activer/désactiver l'accrochage aux objets |
| `F7` | Afficher/masquer la grille |
| `F9` | Activer/désactiver l'accrochage à la grille |
| `Ctrl+Z` | Annuler (Undo) |
| `Ctrl+Y` | Rétablir (Redo) |
| `Molette` | Zoom |
| `Clic molette + glisser` | Pan |

## Structure des dossiers clés

```
bcad/
├── include/bcad/       # API publique (SDK)
│   ├── geometry/        # Types géométriques
│   ├── core/            # Document
│   ├── plugin/          # PluginManager, registres d'extension (workbench, validateur)
│   └── validation/      # Diagnostic / Severity (porte de sortie des règles métier)
├── src/                 # Implémentation
│   ├── geometry/
│   ├── core/
│   ├── plugin/
│   ├── plugins/         # Modules dynamiques (ex. cadastre) — hors du core
│   └── app/             # Interface Qt — privée, non installée, non contractuelle
├── scripts/             # check_arch.sh, prove_sdk.sh, prove_cadastre.sh
└── tests/
    ├── smoke_test.cpp   # Tests de base
    └── unit/            # Tests unitaires par module
```

Le cœur (`src/core`, `src/geometry`, ...) ne dépend ni de Qt ni du cadastre :
`scripts/check_arch.sh` le vérifie à chaque exécution.

## Besoin d'aide ?

- **Problème de compilation ?** → [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- **Questions sur l'architecture ?** → [ARCHITECTURE.md](ARCHITECTURE.md)
- **Questions sur la contribution ?** → [CONTRIBUTOR_GUIDE.md](CONTRIBUTOR_GUIDE.md)

---

*Pour un guide plus détaillé, voir [GETTING_STARTED.md](GETTING_STARTED.md).*