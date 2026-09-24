# Bienvenue dans BCAD

Ce guide est pour toi si tu découvres BCAD. Il explique comment comprendre le projet et faire tes premières modifications.

## 1. Qu'est-ce que BCAD ?

BCAD est une application CAO 2D écrite en C++20. Elle permet de dessiner (lignes, cercles, arcs, polylignes), organiser les dessins en calques, et exporter/importer des fichiers DXF.

** Technologies principales :**
- C++20
- Qt6 (interface graphique)
- OpenGL 3.3 (rendu)
- CGAL (géométrie)
- SQLite3 (format natif .bcad)

**Licence :** GPL-3.0-or-later

## 2. Structure du dépôt

```
bcad/
├── include/bcad/       # Headers publics (SDK)
│   ├── geometry/       # Types géométriques (Point, Line, Circle, Arc, Polyline)
│   ├── layers/         # Gestion des calques
│   ├── render/         # Rendu OpenGL, Quadtree
│   ├── core/           # Document (modèle central)
│   ├── io/             # DXF, SQLite
│   └── app/            # Interface Qt (MainWindow, Viewport, etc.)
│
├── src/                # Implémentation (correspond à include/)
│   ├── geometry/
│   ├── layers/
│   ├── render/
│   ├── core/
│   ├── io/
│   └── app/
│
├── tests/
│   └── smoke_test.cpp  # Tests de base
│
├── docs/               # Documentation architecturale
│   └── *.md            # 37 documents
│
└── CMakeLists.txt      # Build system
```

## 3. Dépendances à installer

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential cmake qt6-base-dev libqt6opengl6-dev \
    libcgal-dev libboost-dev libboost-all-dev libsqlite3-dev \
    libgl1-mesa-dev libglu1-mesa-dev
```

### Autres systèmes

Voir `README.md` § Compilation pour les instructions sur macOS, Windows, ou via vcpkg.

## 4. Compiler le projet

```bash
# 1. Cloner ou aller dans le répertoire
cd /chemin/vers/bcad

# 2. Configurer
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 3. Compiler
cmake --build build -j

# 4. Lancer les tests
ctest --test-dir build --output-on-failure
```

Si tout fonctionne, tu devrais voir des messages "OK" pour chaque module.

## 5. Lancer l'application

```bash
./build/src/app/bcad
```

Tu devrais voir une fenêtre avec :
- Un ruban (onglets Home, Modify, View)
- Un canevas de dessin
- Une barre de statut avec les coordonnées

## 6. Explorer le code

### Pour comprendre les entités géométriques

Lis dans l'ordre :
1. `include/bcad/geometry/Types.h` — types de base (Point, Vector, etc.)
2. `include/bcad/geometry/Entity.h` — enum des types d'entités
3. `include/bcad/geometry/Line.h`, `Circle.h`, etc. — chaque type

### Pour comprendre le modèle de document

1. `include/bcad/core/Document.h` — le Document central
2. `include/bcad/layers/LayerManager.h` — gestion des calques
3. `include/bcad/index/QuadtreeIndex.h` — index spatial indépendant du rendu

### Pour comprendre l'interface

1. `include/bcad/app/MainWindow.h` — fenêtre principale
2. `include/bcad/app/Viewport.h` — zone de dessin
3. `include/bcad/app/Commands.h` — liste des commandes

## 7. Première modification : ajouter un test

Le fichier de test est `tests/smoke_test.cpp`. Il vérifie que chaque module compile.

**Pour ajouter un test :**

1. Ouvre `tests/smoke_test.cpp`
2. Ajoute une fonction pour ton module :

```cpp
// Exemple : ajouter un test pour un nouveau calcul
bool test_my_feature() {
    // Ton code de test ici
    return true;  // ou false si le test échoue
}
```

3. Ajoute l'appel dans `int main()` :

```cpp
if (!test_my_feature()) {
    return 1;
}
```

4. Recompile et teste :

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 8. Vérifier l'architecture

Les principales séparations architecturales sont maintenant vérifiées par
`scripts/check_arch.sh` et `tests/arch_test.cpp` :

- les types CGAL restent confinés à l'implémentation de `geometry` ;
- `core` n'inclut ni Qt ni `render` ;
- l'index spatial est fourni par `index::ISpatialIndex` ;
- les entités et sérialiseurs extensibles passent par les registries ;
- le cadastre est chargé comme plugin et n'est pas une dépendance du Core.

Avant toute modification, consulte `AGENTS.md` et relance ces vérifications.

## 9. Glossaire des termes

| Terme | Signification |
|-------|---------------|
| Entity | Un objet géométrique (Line, Circle, etc.) |
| Document | Le modèle central qui contient toutes les entities |
| Layer | Un calque (comme dans AutoCAD) |
| Quadtree | Structure de données pour accélérer le rendu |
| Tessellation | Conversion en triangles pour OpenGL |
| DXF | Format de fichier AutoCAD |

**Voir aussi :** `docs/GLOSSARY.md` pour plus de définitions.

## 10. Prochaine étape

Maintenant que tu comprends la structure, consulte `docs/FIRST_CONTRIBUTION.md` pour un guide pas-à-pas de ta première contribution.

## 11. Obtenir de l'aide

- **Issues GitHub** : pour signaler un bug ou demander une fonctionnalité
- **Code source** : les commentaires dans les headers expliquent souvent le pourquoi
- **Documentation** : `docs/` contient 37 documents sur l'architecture

## 12. Après ta première compilation

Quand tu as compilé avec succès, tu peux :

1. **Lire le code** de ton entité préférée
2. **Comprendre le flux** : comment un clic utilisateur devient une entité
3. **Chercher un bug** dans la liste des issues
4. **Proposer une amélioration** architecturale (voir `docs/ARCHITECTURE_PRINCIPLES.md`)

Bon courage et bienvenue dans l'équipe !