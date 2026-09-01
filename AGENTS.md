# Guide pour agents IA — BCAD

> Document à destination des agents IA de développement. Ce fichier fait autorité pour les questions architecturales.

## Règles architecturales non négociables

Les règles suivantes sont tirées des ADR et ne doivent PAS être contournées :

| # | Règle | Justification |
|---|-------|---------------|
| ADR-001 | Core **ne doit pas** inclure de header `render/` | Core testable sans rendu, 3D substituable |
| ADR-002 | CGAL **ne doit pas** être exposé dans `include/bcad/` | API stable, plugins sans CGAL |
| ADR-003 | Types d'entités via **Registry**, pas enum | Extension par plugins sans modifier Core |
| ADR-004 | Sérialisation via **SerializerRegistry** | Plugins sauvegardent sans modifier Core |
| ADR-005 | Plugins via `dlopen`/`LoadLibrary` + `bcad_plugin_init` | Extensions dynamiques |
| ADR-006 | Surface publique via **SDK** (CMake export) | Plugins utilisent `find_package(BCAD)` |
| ADR-008 | Index spatial via **ISpatialIndex** | Core/Renderer découplés |
| ADR-009 | Command/Transaction **purs C++**, pas Qt | Core indépendant Qt |
| ADR-010 | EventBus **typé** | Découplage plugins |
| ADR-013 | Pas d'interfaces virtuelles inutiles | Simplicité, performance |

**Voir** : `docs/ARCHITECTURE_DECISIONS.md`

## Structure du projet

```
bcad/
├── include/bcad/          # Headers publics (SDK actuel)
├── src/                   # Implémentation
│   ├── geometry/          # Types, CGAL
│   ├── layers/            # Calques
│   ├── render/            # Quadtree, OpenGL (PROBLÈME: Core→Render)
│   ├── io/                # DXF, SQLite
│   ├── core/              # Document
│   └── app/               # Qt UI
├── tests/
│   └── smoke_test.cpp     # Tests (pas de framework)
├── docs/                  # Documentation architecturale
└── CMakeLists.txt
```

**Problèmes actuels à ne pas aggraver** :
- `include/bcad/geometry/Types.h` expose CGAL (violation ADR-002)
- `include/bcad/core/Document.h` inclut `render/Quadtree.h` (violation ADR-001)

## Workflow obligatoire avant modification

```
1. Comprendre la tâche
2. Lire ARCHITECTURE.md (vue d'ensemble)
3. Lire ARCHITECTURE_PRINCIPLES.md (principes)
4. Lire ARCHITECTURE_DECISIONS.md (ADR applicables)
5. Identifier les fichiers concernés
6. Identifier la couche architecturale
7. Vérifier les violations existantes (ne pas en ajouter)
8. Modifier le minimum nécessaire
9. Vérifier les dépendances (pas de cycle)
10. Compiler : cmake --build build
11. Tester : ctest --test-dir build
12. Ne pas modifier les tests sans accord du mainteneur
```

## Règles d'interdiction

1. **Ne pas ajouter de dépendance de `core/` vers `render/`** (violation ADR-001)
2. **Ne pas exposer CGAL dans `include/bcad/geometry/`** (violation ADR-002)
3. **Ne pas utiliser l'enum `EntityType` pour de nouvelles fonctionnalités** (utiliser TypeId quand disponible)
4. **Ne pas introduire de `#include <Qt...>` dans `include/bcad/`** (violation ADR-009)
5. **Ne pas contourner les registries quand ils existent** (ils permettent l'extension)
6. **Ne pas supprimer des abstractions simplement parce qu'elles "semblent inutiles"** — vérifier les ADR

## Hiérarchie des sources

En cas de conflit, la priorité est :

1. **Code source** (`include/bcad/*.h`, `src/`)
2. **ADR** (`docs/ARCHITECTURE_DECISIONS.md`) — fait autorité pour les décisions
3. **Principes** (`docs/ARCHITECTURE_PRINCIPLES.md`)
4. **Architecture** (`docs/ARCHITECTURE.md`)
5. **Autres documents** (`docs/*.md`)

## Commandes de build vérifiées

```bash
# Configuration
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Compilation
cmake --build build -j

# Tests
ctest --test-dir build --output-on-failure

# Exécution (si compilé)
./build/src/app/bcad
```

## Rapports de fin de tâche

Tout agent doit rapporter :

```
## Modifications

Files modified: [liste]
Files added: [liste]
Files deleted: [liste]

## Vérification

Build status: [OK/FAILED]
Tests executed: [ctest output]
Architecture violations introduced: [NONE ou liste]

## Impact

API/ABI impact: [NONE ou description]
ADR consulted: [ADR-xxx]

## Problèmes restants

[Liste des problèmes détectés ou non résolus]
```

## Modules actuels vs cibles

| Module actuel | Module cible | Statut |
|---------------|--------------|--------|
| geometry | geometry | Exists, CGAL exposé |
| layers | layers | Existe |
| render | render (services) | Existe, Core→Render violation |
| io | io (services) | Existe |
| core | core + commands + events | Document existe, commands/events à ajouter |
| app | app (Qt) | Existe |

## Pour les tâches courantes

| Tâche | Documents à lire |
|-------|------------------|
| Ajouter une Entity | `docs/ENTITY_MODEL.md`, `docs/GEOMETRY_ARCHITECTURE.md` |
| Modifier Document | `docs/DOCUMENT_MODEL.md` |
| Ajouter une Command | `docs/COMMAND_SYSTEM.md` |
| Modifier le renderer | `docs/RENDERING_ARCHITECTURE.md` |
| Ajouter un format de fichier | `docs/IO_ARCHITECTURE.md`, `docs/PERSISTENCE_ARCHITECTURE.md` |
| Préparer 3D | `docs/COORDINATE_SYSTEMS.md`, `docs/SPATIAL_INDEX.md` |

## Contact

Pour des questions architecturales, ouvrir une issue avec le label `architecture`.