# bcad

[![CI](https://github.com/Kauryx-Tech/bcad/actions/workflows/ci.yml/badge.svg)](https://github.com/Kauryx-Tech/bcad/actions/workflows/ci.yml)

Une application CAO 2D personnelle, écrite de zéro dans l'esprit
d'AutoCAD : interface Qt, rendu OpenGL, moteur géométrique adossé à CGAL,
interopérabilité DXF, et un format de projet natif SQLite.

Voir [`architecture bcad.txt`](architecture%20bcad.txt) pour l'esquisse de
conception d'origine, [`ARCHITECTURE.md`](ARCHITECTURE.md) pour la façon
dont le code s'y rattache, et [`DESIGN_NOTES.md`](DESIGN_NOTES.md) pour les
CAO open source qui ont inspiré cette conception.

## État du projet

Chaque couche de l'architecture est implémentée, compile et tourne (vérifié
dans cet environnement, y compris visuellement) : moteur géométrique,
calques, indexation spatiale, E/S DXF/SQLite, pipeline de rendu OpenGL, et
une interface Qt à ruban avec outils de dessin interactifs, accrochage et
undo/redo. Voir [`CAHIER_DES_CHARGES.md`](CAHIER_DES_CHARGES.md) pour la
liste complète et continuellement mise à jour des fonctionnalités et de la
feuille de route priorisée — le résumé ci-dessous n'est qu'un instantané,
ce document est la source de vérité.

## Fonctionnalités (MVP)

- Entités : ligne, cercle, arc, polyligne (ouverte/fermée).
- Calques : création/renommage/suppression, visibilité, verrouillage,
  couleur par calque.
- Outils interactifs : sélection, déplacement, ligne, cercle (centre+rayon),
  arc (centre+début+fin), polyligne (multi-clic, Entrée/clic droit pour
  terminer). Chaque point posé accepte aussi une saisie de coordonnées
  typée (`x,y`, `@dx,dy`, `@dist<angle`) via la ligne de commande de la
  barre de statut.
- Accrochage aux objets (extrémité/milieu/centre/intersection/perpendicu-
  laire) et accrochage à la grille, tous deux avec indicateurs sur le
  canevas ; bascules indépendantes pour l'accrochage aux objets (`F3`),
  l'affichage de la grille (`F7`), l'accrochage à la grille (`F9`).
- Undo/redo (`Ctrl+Z`/`Ctrl+Y`) couvrant le dessin, le déplacement, la
  suppression et les opérations booléennes.
- Pan (glisser avec le bouton du milieu), zoom vers le curseur (molette),
  zoom sur l'ensemble (`F`).
- Opérations booléennes (union/intersection/différence/différence
  symétrique), câblées à une commande GUI (menu Modifier / ruban) :
  sélectionner deux polylignes fermées, appliquer. La triangulation de
  Delaunay/Delaunay contrainte est implémentée et testée au niveau du
  moteur géométrique mais pas encore câblée à une commande GUI.
- Import/export DXF (sous-ensemble ASCII R2000 : LINE/CIRCLE/ARC/LWPOLYLINE
  + calques).
- Fichiers de projet natifs `.bcad` (SQLite).
- Tessellation en arrière-plan (thread dédié) pour que le pan/zoom sur de
  grands dessins ne bloque pas le thread UI ; élagage du viewport basé sur
  un quadtree.
- Interface à ruban (`RibbonBar` : onglets de panneaux de boutons
  légendés, à la manière des AutoCAD récents) aux côtés d'une barre de
  menus classique — les deux pilotent les mêmes `QAction`.

## Compilation

Nécessite un compilateur C++20, CMake ≥ 3.20, Qt6 (Widgets, OpenGLWidgets,
OpenGL, Gui), CGAL, Boost, SQLite3, et les en-têtes de développement
OpenGL :

```bash
sudo apt-get install build-essential cmake qt6-base-dev libqt6opengl6-dev \
    libcgal-dev libboost-dev libboost-all-dev libsqlite3-dev \
    libgl1-mesa-dev libglu1-mesa-dev
```

Puis :

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure   # tests de fumée
./build/src/app/bcad                          # lancer l'application
```

### Alternative : dépendances gérées par vcpkg

Plutôt que d'installer Qt6/CGAL/SQLite3 au niveau système,
[`vcpkg.json`](vcpkg.json) fixe des versions reproductibles via
[vcpkg](https://vcpkg.io) :

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$PWD/vcpkg"

cmake --preset vcpkg
cmake --build --preset vcpkg
ctest --preset vcpkg --output-on-failure
```

La première configuration compile Qt6 et CGAL depuis les sources et prend
du temps ; la CI utilise plutôt le chemin par paquets système ci-dessus
pour la rapidité.

## Organisation

```
include/bcad/<module>/   en-têtes publics, un répertoire par module
src/<module>/            implémentation + le CMakeLists.txt de ce module
tests/                   smoke_test.cpp — une vérification par module, sans framework
```

Modules : `geometry` (entités/opérations CGAL) → `layers` → `render`
(quadtree, caméra, moteur de rendu GL) → `core` (Document, relie calques +
entités + index) → `io` (DXF, SQLite) → `app` (interface Qt). Chacun est sa
propre cible de bibliothèque statique CMake, pour que le graphe de
dépendances reste explicite et à sens unique.

## Contribuer

Voir [CONTRIBUTING.md](CONTRIBUTING.md) pour le cycle build/test et la
checklist de PR, et [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) pour les
règles de la communauté.

## Licence

GPL-3.0-or-later — voir [LICENSE](LICENSE). Ce choix découle de
l'utilisation, dans `geometry`, des paquets CGAL
`Boolean_set_operations_2` et `Triangulation_2`, que CGAL lui-même licencie
en GPL (et non LGPL) ; le reste du projet s'aligne pour que le binaire dans
son ensemble reste distribuable.

## Feuille de route

Voir [`CAHIER_DES_CHARGES.md`](CAHIER_DES_CHARGES.md) §3 pour la liste
priorisée et à jour (P0–P3). Points marquants : rotation/mise à
l'échelle/miroir interactifs, sélection multiple, propriétés d'entité
éditables, puis texte/cotations/hachures/blocs comme nouveaux sous-types
d'Entity, puis le support DWG (format propriétaire — probablement via un
convertisseur externe plutôt qu'un lecteur écrit de zéro).
