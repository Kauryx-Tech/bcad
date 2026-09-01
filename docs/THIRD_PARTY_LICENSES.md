# Licences des dépendances tierces BCAD

> Inventaire des dépendances, leurs licences et leurs implications.

## 1. Vue d'ensemble

| Dépendance | Version | Composants utilisés | Licence | Impact SDK |
|------------|---------|-------------------|---------|------------|
| CGAL | ~5.x | Boolean_set_operations_2, Triangulation_2 | GPL/LGPL | Force GPL |
| Qt6 | 6.x | Widgets, OpenGLWidgets, OpenGL, Gui | LGPL v3 | Compatible |
| Boost | 1.84+ | Algo, Geometry, Container | BSL 1.0 | Permissive |
| SQLite3 | 3.x | Core | Domaine public | Aucune |
| OpenGL | 3.3 | Core profile | SGI License | Aucune |
| CMake | 3.20+ | Build system | BSD-3-Clause | Build only |

## 2. CGAL

### 2.1 Licence

CGAL est sous **LGPL** avec des exceptions. Certains modules sont sous **GPL**.

### 2.2 Modules utilisés

| Module | Licence | Usage dans BCAD |
|--------|---------|-----------------|
| `Exact_predicates_inexect_constructions_kernel` | LGPL | Kernel géométrique |
| `Polygon_2` | LGPL | Représentation polygonale |
| `Boolean_set_operations_2` | **GPL** | Union, intersection, différence |
| `Constrained_Delaunay_triangulation_2` | **GPL** | Triangulation contrainte |
| `Delaunay_triangulation_2` | **GPL** | Triangulation Delaunay |
| `Aff_transformation_2` | LGPL | Transformations |

### 2.3 Impact

Les modules GPL de CGAL forcent BCAD à être sous **GPL** si utilisé dans `geometry/`.

### 2.4 Implications SDK

- CGAL ne doit pas être exposé dans le SDK public
- CGAL reste confiné à `src/bcad/geometry/detail/`

## 3. Qt6

### 3.1 Licence

**LGPL v3** (ou GPL v3 si commerciale).

### 3.2 Composants utilisés

| Composant | Usage |
|-----------|-------|
| QtWidgets | Interface graphique |
| QtOpenGLWidgets | Contexte OpenGL |
| QtOpenGL | Fonctions OpenGL |
| QtGui | Événements, IME, cursors |

### 3.3 Implications

- Liaison dynamique : Qt peut être remplacé
- Liaison statique : Qt doit être GPL
- BCAD utilise liaison dynamique

### 3.4 Implications SDK

- Qt n'est **pas** dans le SDK
- Le SDK est pur C++

## 4. Boost

### 4.1 Licence

**Boost Software License 1.0** (BSL) — permissive, compatible GPL.

### 4.2 Composants utilisés

| Composant | Usage |
|-----------|-------|
| `boost/geometry` | Algorithmes géométriques |
| `boost/container` | Allocateurs, containers |
| `boost/algorithm` | Algorithmes string |

### 4.3 Implications

- BSL est permissive
- Pas d'impact sur la licence de BCAD

### 4.4 Implications SDK

- Boost n'est pas dans le SDK public

## 5. SQLite3

### 5.1 Licence

**Domaine public** (ou PUBLICDOMAIN).

### 5.2 Usage

Format natif `.bcad` via SQLite.

### 5.3 Implications

- Aucune contrainte de licence
- Pas d'attribution obligatoire
- Pas de copyleft

## 6. OpenGL

### 6.1 Licence

**SGI Free Software License B** (SGI License) — permissive.

### 6.2 Usage

Pipeline de rendu OpenGL 3.3 Core Profile.

### 6.3 Implications

- Aucune contrainte
- Compatible avec toutes les licences

## 7. CMake

### 7.1 Licence

**BSD-3-Clause**.

### 7.2 Usage

Système de build.

### 7.3 Implications

- Aucune impact sur la licence de BCAD
- Build-time only

## 8. Résumé

| Catégorie | Dépendances | Impact licence |
|-----------|------------|----------------|
| Force GPL | CGAL (modules GPL) | BCAD = GPL |
| Compatibles | Qt6 (LGPL), Boost (BSL), SQLite (PD), OpenGL (SGI) | Aucun |
| Build only | CMake | Aucun |

## 9. Attribution requise

| Dépendance | Attribution requise |
|------------|-------------------|
| CGAL | Mentionner CGAL dans les crédits |
| Qt6 | Mentionner Qt dans les crédits (si utilisé) |
| Boost | Mentionner Boost dans les crédits |
| SQLite3 | Aucune (domaine public) |
| OpenGL | Aucune (SGI License) |

## 10. Redistribution

| Dépendance | Fichier de redistribution | Obligations |
|------------|---------------------------|-------------|
| CGAL | Obligatoire (GPL) | Code source ou offre |
| Qt6 | Si modifié | Code source ou offre |
| Boost | Non | Aucune |
| SQLite3 | Non | Aucune |
| OpenGL | Non | Aucune |

## 11. Impact commercial

| Dépendance | Plugin propriétaire | Distribution commerciale |
|------------|-------------------|-------------------------|
| CGAL (GPL) | Via liaison dynamique | Via licence séparée |
| Qt6 (LGPL) | Via liaison dynamique | Via licence Qt commerciale |
| Boost (BSL) | Oui | Oui |
| SQLite3 (PD) | Oui | Oui |
| OpenGL (SGI) | Oui | Oui |

## 12. Voir aussi

- `LICENSING.md` : analyse détaillée
- `LICENSE` : texte intégral GPL-3.0
- `docs/` : documentation