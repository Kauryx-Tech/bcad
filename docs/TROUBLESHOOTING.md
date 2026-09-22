# Guide de dépannage BCAD

> Problèmes courants et leurs solutions.

## Table des matières

1. [Erreurs de compilation](#erreurs-de-compilation)
2. [Erreurs à l'exécution](#erreurs-à-lexécution)
3. [Problèmes d'affichage](#problèmes-daffichage)
4. [Problèmes de performance](#problèmes-de-performance)
5. [Problèmes d'import/export](#problèmes-dimportexport)
6. [Contribuer une solution](#contribuer-une-solution)

---

## Erreurs de compilation

### "Could not find Qt6"

```
CMake Error: Could not find a package configuration file provided by "Qt6"
```

**Solution :**

```bash
# Ubuntu/Debian
sudo apt-get install qt6-base-dev libqt6opengl6-dev
```

### "Could not find CGAL"

```
CMake Error: Could not find CGAL
```

**Solution :**

```bash
sudo apt-get install libcgal-dev
```

### "Could not find Boost"

BCAD n'utilise pas Boost directement ; Boost n'apparaît que comme dépendance transitive de CGAL. Si `find_package(Boost)` échoue, l'erreur vient de CGAL :

```bash
sudo apt-get install libcgal-dev libboost-dev libboost-all-dev
```

### Erreur : "undefined reference to..."

**Cause :** Incohérence headers/implémentation.

**Solution :**

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

### Erreur : "No such file or directory"

```
fatal error: bcad/geometry/Types.h: No such file or directory
```

**Solution :**

```bash
# Vérifier la structure
ls include/bcad/

# Si manquants, cloner avec submodules
git clone --recurse-submodules https://github.com/Kauryx-Tech/bcad.git
```

---

## Erreurs à l'exécution

### Crash au démarrage

```bash
# Lancer avec GDB
gdb ./build/src/app/bcad
(gdb) run
(gdb) bt  # après crash
```

**Solutions :**

1. Vérifier OpenGL :
```bash
glxinfo | grep "OpenGL version"
sudo apt-get install libgl1-mesa-dev libglu1-mesa-dev
```

2. Vérifier Qt :
```bash
sudo apt-get install libxcb-* libxkbcommon-x11-0
```

### "Failed to load platform plugin"

```
qt.qpa.plugin: Could not load the Qt platform plugin "xcb"
```

**Solution :**

```bash
sudo apt-get install libxcb-*
QT_QPA_PLATFORM=offscreen ./build/src/app/bcad
```

---

## Problèmes d'affichage

### Canevas noir

1. Vérifier le pilote OpenGL :
```bash
glxinfo | grep "OpenGL renderer"
```

2. Tester en mode minimal :
```bash
QT_QPA_PLATFORM=offscreen ./build/src/app/bcad
```

### Entités non visibles

1. Vérifier la visibilité du calque (clic droit → Afficher)
2. Appuyer sur `F` pour zoomer sur l'ensemble
3. Appuyer sur `Échap` pour désélectionner

### Outils de dessin inactifs

1. Vérifier le calque actif (doit être surligné)
2. Appuyer sur `Échap` pour désactiver l'outil courant

---

## Problèmes de performance

### Lenteur zoom/pan

1. Activer le quadtree (devrait être par défaut)
2. Désactiver la tessellation temps réel
3. Vérifier que le culling est actif

### Freeze à l'ouverture

1. Attendre (gros fichiers DXF)
2. L'application `main.cpp` ne parse aucun argument de ligne de commande — l'option `--import` n'existe pas encore. Ouvrir le fichier via le menu Fichier.

### Interface lente

```bash
# Vérifier la RAM
free -h

# Mode minimal
QT_QPA_PLATFORM=offscreen ./build/src/app/bcad
```

---

## Problèmes d'import/export

### Erreur import DXF

```
DXF import failed: Unsupported DXF version
```

**Solution :** Exporter en format R2000 (AC1015) depuis AutoCAD.

### Erreur export DXF

```
DXF export failed: Entity type not supported
```

**Solution :** Supprimer les entités non supportées avant export.

### Fichier .bcad illisible

```bash
# Vérifier le format
file monfichier.bcad

# Vérifier SQLite
sqlite3 monfichier.bcad "PRAGMA integrity_check;"
```

---

## Tests qui échouent

```bash
# Sortie détaillée
ctest --test-dir build --output-on-failure -V

# Test spécifique
ctest --test-dir build -R smoke_test -V
```

---

## Contribuer une solution

Ouvre une issue ou PR avec le template :

```markdown
### [Problème]

**Cause :** ...
**Solution :** ...
```

---

## Obtenir de l'aide

| Méthode | Quand |
|---------|-------|
| [Issues GitHub](https://github.com/Kauryx-Tech/bcad/issues) | Bug |
| [Discussions](https://github.com/Kauryx-Tech/bcad/discussions) | Questions |

---

## Voir aussi

- [GETTING_STARTED.md](GETTING_STARTED.md)
- [CONTRIBUTOR_GUIDE.md](CONTRIBUTOR_GUIDE.md)
- [ARCHITECTURE.md](ARCHITECTURE.md)