# Revue architecturale BCAD

> Synthèse de l'audit et de la documentation produite.

## 1. Ce qui a été découvert

### 1.1 Modules identifiés

BCAD contient 6 modules :
- `geometry` (entités CGAL, opérations booléennes, triangulation)
- `layers` (Layer, LayerManager)
- `render` (Quadtree, Camera2D, GlRenderer, TessellationTypes, LevelOfDetail)
- `core` (Document)
- `io` (DxfReader, DxfWriter, Database)
- `app` (MainWindow, Viewport, LayerPanel, TessellationWorker)

### 1.2 Dépendances principales

- **CGAL** : kernel géométrique, opérations booléennes, triangulation
- **Qt6** : interface graphique
- **OpenGL 3.3** : pipeline de rendu
- **Boost** : algorithms
- **SQLite3** : format natif `.bcad`

### 1.3 Violations architecturales

| Violation | Sévérité | Description |
|-----------|----------|-------------|
| CGAL exposé publiquement | Critique | `Types.h`, `Entity.h` exposent `CGAL::Point_2` |
| Dépendance inversée Core → Render | Critique | `core::Document` possède `render::Quadtree` |
| EntityType enum figé | Majeure | `enum class EntityType` empêche l'extension |
| Pas de plugin system | Majeure | Aucun `dlopen`, pas de SDK |
| Pas de CMake installable | Majeure | Pas de `find_package(BCAD)` |
| Switch sur EntityType partout | Majeure | `DxfReader`, `DxfWriter`, `Database` |

## 2. Confirmé par le code

- `include/bcad/geometry/Types.h` : types CGAL exposés
- `include/bcad/geometry/Entity.h` : enum `EntityType`
- `include/bcad/core/Document.h` : possède `unique_ptr<render::Quadtree>`
- `include/bcad/io/Database.h` : utilise `switch(entity.type())`
- `include/bcad/render/Quadtree.h` : index spatial Quadtree

## 3. Contradictions éventuelles

### ENTRE DESIGN_NOTES et ARCHITECTURE

DESIGN_NOTES mentionne que `TessellationWorker` a été déplacé dans `app` comme correction d'une dépendance circulaire. Mais le problème de fond (Core → Render) persiste.

**Décision :** Phase 3 de la roadmap s'attaque au problème de fond.

## 4. Décisions architecturales proposées

### 4.1 Masquer CGAL derrière value types BCAD

**Problème :** `Point2 = CGAL::Point_2<Kernel>` exposé publiquement.
**Solution :** `Point2` est un struct BCAD. CGAL confiné à `geometry/detail/`.
**Raison :** permettre remplacement du kernel.

### 4.2 Découpler Core et Render

**Problème :** `core::Document` possède `render::Quadtree`.
**Solution :** `Document` possède `unique_ptr<ISpatialIndex>`. Backend par défaut fourni par app.
**Raison :** inversion de dépendance.

### 4.3 EntityRegistry dynamique

**Problème :** `enum class EntityType` figé.
**Solution :** `TypeId { namespace, name, guid }` + `EntityRegistry`.
**Raison :** extension par plugins.

### 4.4 PropertyMap générique

**Solution :** `PropertyMap` avec types dynamiques.
**Raison :** propriétés plugins.

### 4.5 Transaction explicite

**Solution :** `commands::Command` + `Transaction` multi-commandes + adaptateur Qt.
**Raison :** Core indépendant de Qt, transactions atomiques.

### 4.6 EventBus typé

**Solution :** `events::EventBus` typé via templates.
**Raison :** découplage, abonnements plugins.

### 4.7 SerializerRegistry

**Solution :** `SerializerRegistry` enregistre un `IEntitySerializer` par type.
**Raison :** persistance plugin.

### 4.8 IRenderBackend

**Solution :** `IRenderBackend` + `OpenGLBackend` par défaut.
**Raison :** backends multiples.

### 4.9 SDK exportable

**Solution :** `install()`, `EXPORT`, `BCADConfig.cmake`.
**Raison :** plugins externes.

### 4.10 PluginSystem dlopen

**Solution :** `PluginManager` + `dlopen` + `bcad_plugin_init`.
**Raison :** extension sans recompilation.

## 5. Décisions à valider

- [ ] Compatibilité juridique de chaque version de CGAL
- [ ] Choix framework de tests (doctest vs Catch2)
- [ ] Politique DCO/CLA pour contributions
- [ ] Wrapper C pour ABI stable en v2
- [ ] Choix kernel 3D (CGAL vs Eigen)
- [ ] Format manifeste plugin (JSON vs TOML)

## 6. Documents créés

| # | Document | Statut |
|---|----------|--------|
| 1 | `docs/ARCHITECTURE.md` | ✓ |
| 2 | `docs/ARCHITECTURE_PRINCIPLES.md` | ✓ |
| 3 | `docs/ARCHITECTURE_BENCHMARK.md` | ✓ |
| 4 | `docs/GEOMETRY_ARCHITECTURE.md` | ✓ |
| 5 | `docs/ENTITY_MODEL.md` | ✓ |
| 6 | `docs/DOCUMENT_MODEL.md` | ✓ |
| 7 | `docs/PROPERTY_SYSTEM.md` | ✓ |
| 8 | `docs/COMMAND_SYSTEM.md` | ✓ |
| 9 | `docs/EVENT_SYSTEM.md` | ✓ |
| 10 | `docs/RENDERING_ARCHITECTURE.md` | ✓ |
| 11 | `docs/SPATIAL_INDEX.md` | ✓ |
| 12 | `docs/COORDINATE_SYSTEMS.md` | ✓ |
| 13 | `docs/PERSISTENCE_ARCHITECTURE.md` | ✓ |
| 14 | `docs/IO_ARCHITECTURE.md` | ✓ |
| 15 | `docs/SDK_ARCHITECTURE.md` | ✓ |
| 16 | `docs/PLUGIN_ARCHITECTURE.md` | ✓ |
| 17 | `docs/API_ABI_POLICY.md` | ✓ |
| 18 | `docs/BUILD_AND_PACKAGING.md` | ✓ |
| 19 | `docs/TESTING_ARCHITECTURE.md` | ✓ |
| 20 | `docs/LICENSING.md` | ✓ |
| 21 | `docs/THIRD_PARTY_LICENSES.md` | ✓ |
| 22 | `docs/ARCHITECTURE_ROADMAP.md` | ✓ |
| 23 | `docs/ARCHITECTURE_REVIEW.md` | ✓ (ce document) |
| 24 | `docs/ARCHITECTURE_DECISIONS.md` | ✓ (ADR) |

**Total : 24 documents.**

## 7. Prochaines étapes

1. **Validation** par l'utilisateur
2. **Phase 2** : Geometry API
3. **Phase 3** : Core / Render decoupling
4. **Phase 4** : Entity Registry
5. **Phases 5-8** : Property, Command, Event, Persistence
6. **Phases 9-10** : Rendering, SDK
7. **Phases 11-12** : Plugin System + proof
8. **Phases 13-14** : 3D

Voir `ARCHITECTURE_ROADMAP.md`.

## 8. Réponses aux questions architecturales

| # | Question | Réponse |
|---|----------|---------|
| 1 | Une nouvelle Entity sans modifier Core/EntityType/Document/Database/Renderer ? | OUI |
| 2 | Une nouvelle Command sans modifier le Core ? | OUI |
| 3 | Un plugin peut-il ajouter des Properties ? | OUI |
| 4 | Un plugin peut-il sauvegarder ses Entities ? | OUI |
| 5 | Le Core sans Qt/OpenGL ? | OUI |
| 6 | Remplacer CGAL sans réécrire l'API publique ? | OUI |
| 7 | Ajouter un backend de rendu ? | OUI |
| 8 | Ajouter 3D sans casser 2D ? | OUI |
| 9 | Transaction multi-commandes ? | OUI |
| 10 | Plugin externe dans repo séparé ? | OUI |
---

## 9. Documents supplémentaires (session 2026-09-01)

| # | Document | Statut |
|---|----------|--------|
| 25 | `docs/README.md` | ✓ (index docs) |
| 26 | `docs/GLOSSARY.md` | ✓ (glossaire) |
| 27 | `docs/EXTENDING_BCAD.md` | ✓ (guide extension) |
| 28 | `docs/CONTRIBUTOR_GUIDE.md` | ✓ (guide contributeur) |
| 29 | `docs/WORKBENCH.md` | ✓ (workbench model) |
| — | `docs/schemas/` | 4 schémas Draw.io |

**Total : 29 documents + 4 schémas Draw.io**

---

## 10. Documents Phase 2 (2026-09-01) — Accessibilité

| # | Document | Statut |
|---|----------|--------|
| 30 | `docs/GETTING_STARTED.md` | ✓ (mise en route) |
| 31 | `docs/FIRST_CONTRIBUTION.md` | ✓ (tutoriel PR) |
| 32 | `docs/README.md` | ✓ (index mis à jour) |
| 33 | `AGENTS.md` | ✓ (guide agents IA) |
| 34 | `examples/` | ✓ (4 exemples pratiques) |

**Total : 34 documents + 4 schémas + exemples/**
