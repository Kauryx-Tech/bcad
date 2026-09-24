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
| ADR-016 | Plateforme cible : poste modeste, **hors-ligne**, livrable = document, aucune règle métier dans le core | Outil implantable en Afrique de l'Ouest, pas un clone d'AutoCAD |

**Voir** : `docs/ARCHITECTURE_DECISIONS.md`

## Structure du projet

```
bcad/
├── include/bcad/          # Headers publics (SDK actuel)
├── src/                   # Implémentation
│   ├── geometry/          # Types, transformations, booléens (CGAL inclus via detail/)
│   ├── layers/            # Calques
│   ├── index/             # Index spatial (QuadtreeIndex)
│   ├── events/            # EventBus typé (header-only)
│   ├── properties/        # PropertyMap / PropertyTypes
│   ├── registry/          # EntityRegistry
│   ├── serialization/     # SerializerRegistry
│   ├── commands/          # Commandes / transactions pures C++
│   ├── plugin/            # PluginManager (dlopen), PluginRegistry (ABI ADR-005)
│   ├── render/            # OpenGL (GlRenderer, Camera2D)
│   ├── io/                # DXF, SQLite
│   ├── core/              # Document
│   └── app/               # Qt UI
├── tests/
│   ├── smoke_test.cpp     # Tests fumée
│   ├── arch_test.cpp      # Violations architecturales
│   ├── command_test.cpp, roundtrip_test.cpp, sdk_abi_test.cpp,
│   │   typeid_stability_test.cpp
│   └── unit/              # Tests unitaires par module
├── docs/                  # Documentation architecturale
├── examples/
│   └── sdk_proof/         # Preuve SDK installable + plugin externe (Phase 9/10)
└── CMakeLists.txt
```

**Problèmes actuels à ne pas aggraver** :
- CGAL est confiné à `src/geometry/*.cpp` et `detail/CgalConversions.h`
  (ADR-002 OK, vérifié par `scripts/check_arch.sh`) — ne pas ré-exposer de
  types CGAL dans `include/bcad/geometry/`.
- La migration CGAL est documentée dans `docs/CGAL_MIGRATION.md`

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
| geometry | geometry | Existe (Phase 2 CGAL masqué, BooleanOps.h n'expose que des types BCAD) |
| layers | layers | Existe |
| index | index | Existe (QuadtreeIndex, ISpatialIndex) |
| events | events | Existe (EventBus typé, header-only) |
| properties | properties | Existe (PropertyMap copiable) |
| registry | registry | Existe (EntityRegistry) |
| serialization | serialization | Existe (SerializerRegistry) |
| commands | commands | Existe (pures C++, CommandRegistry) |
| plugin | plugin | Existe (ABI ADR-005 : `bcad_plugin_init(PluginRegistry&)`, PluginManager cycle de vie, preuve plugin externe `sdk_external_test`) |
| render | render | Existe (découplé du Core, ADR-001 OK) |
| io | io (services) | Existe |
| core | core | Existe (Document sur ISpatialIndex) |
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
| Ajouter/étendre un plugin métier | `docs/PLUGIN_ARCHITECTURE.md`, `docs/PLUGIN_DOMAINS.md`, `docs/WORKBENCH.md` |
| Arbitrer une priorité ou une nouvelle cible | `docs/ROADMAP_MARKET.md`, `docs/ARCHITECTURE_DECISIONS.md` (ADR-016) |

## Contact

Pour des questions architecturales, ouvrir une issue avec le label `architecture`.