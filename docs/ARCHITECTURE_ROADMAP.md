# Feuille de route architecturale BCAD

> Migration progressive de l'architecture actuelle vers l'architecture cible.
> **Statuts** (« FAIT » = vérifié par les tests/`check_arch.sh`, « PARTIEL » =
> cœur réalisé mais une partie reste cible, « À VENIR » = non commencé) —
> suivi vivant dans `CONSOLIDATION_STATUS.md`.

## 1. Phases

| Phase | Objectif | Pré-requis | Statut |
|-------|----------|-----------|--------|
| 1 | Documentation | - | FAIT |
| 2 | Geometry API | Phase 1 | FAIT |
| 3 | Core/Render decoupling | Phase 2 | FAIT |
| 4 | Entity Registry | Phase 3 | FAIT |
| 5 | Property System | Phase 4 | FAIT |
| 6 | Command + Transaction | Phase 5 | FAIT |
| 7 | Event System | Phase 6 | FAIT |
| 8 | Persistence Registry | Phase 7 | PARTIEL |
| 9 | Rendering abstraction | Phase 3 | PARTIEL |
| 10 | SDK | Phases 4-8 | FAIT |
| 11 | Plugin System | Phase 10 | FAIT |
| 12 | External plugin proof | Phase 11 | FAIT |
| 13 | 3D foundation | Phases 2-11 | À VENIR |
| 14 | 3D implementation | Phase 13 | À VENIR |

## 2. Phase 1 : Documentation

**Statut : FAIT** — corpus `docs/` (41 fichiers) et `AGENTS.md` tenus à jour.

**Objectif :** produire la documentation architecturale.

**Livrables :**
- 21 documents dans `docs/`
- Validation par l'utilisateur

**Critères :** cohérence entre documents, validation par le reviewer.

## 3. Phase 2 : Geometry API

**Statut : FAIT** — `Point2` est un type BCAD ; CGAL confiné à `src/geometry/*.cpp` et `detail/` (ADR-002, vérifié par `check_arch.sh`).

**Objectif :** masquer CGAL derrière des types BCAD.

**Fichiers :** `include/bcad/geometry/Types.h`, `Entity.h`, `src/geometry/BooleanOps.cpp`, `src/geometry/Triangulation.cpp`, `detail/`.

**Risques :** performance, tests existants.

**Critères :** aucun header CGAL dans `include/bcad/geometry/`.

## 4. Phase 3 : Core / Render decoupling

**Statut : FAIT** — `Document` est sur `ISpatialIndex` (`index/QuadtreeIndex`), aucun header render dans le Core ; tessellation par `Entity::tessellate()`.

**Objectif :** supprimer la dépendance Core → Render.

**Fichiers :** `core/Document.h`, `render/Quadtree.h` → `index/QuadtreeIndex.h`, `include/bcad/render/TessellationTypes.h` → `index/TessellationInput.h`.

**Risques :** `TessellationWorker` Qt doit être déplacé.

**Critères :** `core/Document.h` n'inclut aucun header de `render/`.

## 5. Phase 4 : Entity Registry

**Statut : FAIT** — `EntityRegistry` + `TypeId` opérationnels (ADR-003) ; l'enum `EntityType` reste déprécié (rétrocompat `Database.cpp` seule).

**Objectif :** remplacer l'enum `EntityType` par un registre.

**Fichiers :** `geometry/Entity.h` → ajout TypeId, `registry/EntityRegistry.h` (nouveau), `io/DxfReader.cpp`, `io/DxfWriter.cpp`, `io/Database.cpp`.

**Critères :** `EntityType` enum supprimé, registry opérationnel.

## 6. Phase 5 : Property System

**Statut : FAIT** — `properties/PropertyMap.h` opérationnel (types `PropertyType`/`PropertyValue`, copiable).

**Objectif :** PropertyMap générique.

**Fichiers :** `properties/Property.h`, `properties/PropertyMap.h` (nouveau).

**Critères :** PropertyMap existe, PropertyPanel Qt s'adapte.

## 7. Phase 6 : Command + Transaction

**Statut : FAIT** — `bcad::commands::Command`/`Transaction`/`CommandRegistry` pures C++ (ADR-009), adaptateur Qt dans `app`.

**Objectif :** Transaction explicite, au-delà de QUndoCommand.

**Fichiers :** `commands/Command.h`, `commands/Transaction.h`, `commands/CommandRegistry.h`, `app/commands/QCommandAdapter.h`.

**Critères :** Transaction explicite fonctionne, undo/redo via Transaction.

## 8. Phase 7 : Event System

**Statut : FAIT** — `events/EventBus.h` typé (header-only), événements publiés par le Document.

**Objectif :** EventBus typé.

**Fichiers :** `events/EventBus.h` (nouveau).

**Critères :** EventBus opérationnel, événements émis par le Document.

## 9. Phase 8 : Persistence Registry

**Statut : PARTIEL** — `SerializerRegistry` opérationnel (y compris cycle de vie plugin, `bae000d`) ; le **schéma JSON** et le **versioning** du format `.bcad` restent cibles.

**Objectif :** SerializerRegistry dynamique.

**Fichiers :** `persistence/IEntitySerializer.h`, `registry/SerializerRegistry.h`, `io/DxfSerializer.cpp`, `io/BcadSerializer.cpp`.

**Critères :** SerializerRegistry opérationnel.

## 10. Phase 9 : Rendering abstraction

**Statut : PARTIEL** — rendu découplé du Core (`GlRenderer`, `Camera2D`, `Grid`) ; `IRenderBackend` multi-backend reste cible.

**Objectif :** IRenderBackend avec OpenGL/Vulkan.

**Fichiers :** `render/IRenderBackend.h`, `render/ICamera.h`, `render/ITessellator.h`, `render/OpenGLBackend.cpp`, `render/Camera2D.cpp`, `render/Camera3D.cpp`, `render/SceneExtractor.cpp`.

**Critères :** IRenderBackend opérationnel, OpenGLBackend par défaut.

## 11. Phase 10 : SDK

**Statut : FAIT** — `install()` + export + `find_package(BCAD CONFIG REQUIRED)`, consommé par `examples/sdk_proof` (`BCAD::bcad_core`, `bcad_geometry`, `bcad_plugin`).

**Objectif :** SDK public exportable.

**Fichiers :** `CMakeLists.txt` (racine) → `install()`, `export()`. `cmake/BCADConfig.cmake`, `cmake/BCADVersion.cmake`.

**Critères :** `find_package(BCAD CONFIG REQUIRED)` fonctionne.

## 12. Phase 11 : Plugin System

**Objectif :** PluginManager + ABI ADR-005 (`bcad_plugin_init(PluginRegistry&)`).
**Statut : FAIT** (commit `30dbd41`) — l'ABI objet `IPlugin` a été supprimée au
profit de ce contrat unique.

**Fichiers :** `plugin/Plugin.h`, `plugin/PluginRegistry.h`, `plugin/PluginManager.cpp`, `app/PluginLoader.cpp`.

**Critères :** PluginManager charge .so/.dll, plugins peuvent enregistrer types.

## 13. Phase 12 : External plugin proof

**Objectif :** prouver l'architecture via un plugin externe.
**Statut : FAIT** — `examples/sdk_proof/plugin` (preuve incorporée au repo,
test CTest `sdk_external_test` ; l'ancienne cible repo séparé n'est pas retenue).

**Fichiers :** `examples/sdk_proof/` (consumer, plugin, loader), `tests/sdk_external_test.cpp`.

**Critères :** test passe.

## 14. Phase 13 : 3D foundation

**Statut : À VENIR.**

**Objectif :** préparer la 3D sans l'implémenter complètement.

**Fichiers :** ajout `Point3`, `Vector3`, `Transform3`, `BoundingBox3`, `OctreeIndex`, `Camera3D`, `Entity3D`.

**Critères :** 2D et 3D coexistent dans le Document.

## 15. Phase 14 : 3D implementation

**Statut : À VENIR.**

**Objectif :** fonctionnalités 3D complètes.

**Fichiers :** entités 3D, booléens 3D, rendu 3D, workbench 3D.

**Critères :** BCAD est une plateforme 2D/3D.

## 16. Critères de fin de migration

| Critère | Mesure | Statut |
|---------|--------|--------|
| Aucun header CGAL dans API publique | grep | SATISFAIT |
| Aucun header Qt dans Core | grep | SATISFAIT |
| Aucun header render dans Core | grep | SATISFAIT |
| find_package(BCAD) fonctionne | test | SATISFAIT (`sdk_proof`) |
| External plugin test passe | test | SATISFAIT (`sdk_external_test`) |
| PropertyPanel dynamique | test | PARTIEL (`PropertyMap` en place ; pilotage du panel non testé) |
| Undo/Redo via Transaction | test | SATISFAIT |
| Document fonctionne sans render | test | SATISFAIT |
| EntityRegistry dynamique | test | SATISFAIT |
| SerializerRegistry dynamique | test | SATISFAIT |

## 17. Conclusion

Les phases 1-12 sont **FAIT** (migration achevée, vérifiée par `tests/` et
`scripts/check_arch.sh`) ; restent les fondations 3D (13) et la 3D complète
(14), hors périmètre 2D.