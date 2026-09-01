# Feuille de route architecturale BCAD

> Migration progressive de l'architecture actuelle vers l'architecture cible.

## 1. Phases

| Phase | Objectif | Pré-requis |
|-------|----------|-----------|
| 1 | Documentation | - |
| 2 | Geometry API | Phase 1 |
| 3 | Core/Render decoupling | Phase 2 |
| 4 | Entity Registry | Phase 3 |
| 5 | Property System | Phase 4 |
| 6 | Command + Transaction | Phase 5 |
| 7 | Event System | Phase 6 |
| 8 | Persistence Registry | Phase 7 |
| 9 | Rendering abstraction | Phase 3 |
| 10 | SDK | Phases 4-8 |
| 11 | Plugin System | Phase 10 |
| 12 | External plugin proof | Phase 11 |
| 13 | 3D foundation | Phases 2-11 |
| 14 | 3D implementation | Phase 13 |

## 2. Phase 1 : Documentation (en cours)

**Objectif :** produire la documentation architecturale.

**Livrables :**
- 21 documents dans `docs/`
- Validation par l'utilisateur

**Critères :** cohérence entre documents, validation par le reviewer.

## 3. Phase 2 : Geometry API

**Objectif :** masquer CGAL derrière des types BCAD.

**Fichiers :** `include/bcad/geometry/Types.h`, `Entity.h`, `src/bcad/geometry/BooleanOps.cpp`, `Triangulation.cpp`, `detail/`.

**Risques :** performance, tests existants.

**Critères :** aucun header CGAL dans `include/bcad/geometry/`.

## 4. Phase 3 : Core / Render decoupling

**Objectif :** supprimer la dépendance Core → Render.

**Fichiers :** `core/Document.h`, `render/Quadtree.h` → `index/QuadtreeIndex.h`, `render/TessellationResult.h` → `render/TessellationInput.h`.

**Risques :** `TessellationWorker` Qt doit être déplacé.

**Critères :** `core/Document.h` n'inclut aucun header de `render/`.

## 5. Phase 4 : Entity Registry

**Objectif :** remplacer l'enum `EntityType` par un registre.

**Fichiers :** `geometry/Entity.h` → ajout TypeId, `registry/EntityRegistry.h` (nouveau), `io/DxfReader.cpp`, `io/DxfWriter.cpp`, `io/Database.cpp`.

**Critères :** `EntityType` enum supprimé, registry opérationnel.

## 6. Phase 5 : Property System

**Objectif :** PropertyMap générique.

**Fichiers :** `properties/Property.h`, `properties/PropertyMap.h` (nouveau).

**Critères :** PropertyMap existe, PropertyPanel Qt s'adapte.

## 7. Phase 6 : Command + Transaction

**Objectif :** Transaction explicite, au-delà de QUndoCommand.

**Fichiers :** `commands/Command.h`, `commands/Transaction.h`, `commands/CommandRegistry.h`, `app/commands/QCommandAdapter.h`.

**Critères :** Transaction explicite fonctionne, undo/redo via Transaction.

## 8. Phase 7 : Event System

**Objectif :** EventBus typé.

**Fichiers :** `events/EventBus.h` (nouveau).

**Critères :** EventBus opérationnel, événements émis par le Document.

## 9. Phase 8 : Persistence Registry

**Objectif :** SerializerRegistry dynamique.

**Fichiers :** `persistence/IEntitySerializer.h`, `registry/SerializerRegistry.h`, `io/DxfSerializer.cpp`, `io/BcadSerializer.cpp`.

**Critères :** SerializerRegistry opérationnel.

## 10. Phase 9 : Rendering abstraction

**Objectif :** IRenderBackend avec OpenGL/Vulkan.

**Fichiers :** `render/IRenderBackend.h`, `render/ICamera.h`, `render/ITessellator.h`, `render/OpenGLBackend.cpp`, `render/Camera2D.cpp`, `render/Camera3D.cpp`, `render/SceneExtractor.cpp`.

**Critères :** IRenderBackend opérationnel, OpenGLBackend par défaut.

## 11. Phase 10 : SDK

**Objectif :** SDK public exportable.

**Fichiers :** `CMakeLists.txt` (racine) → `install()`, `export()`. `cmake/BCADConfig.cmake`, `cmake/BCADVersion.cmake`.

**Critères :** `find_package(BCAD CONFIG REQUIRED)` fonctionne.

## 12. Phase 11 : Plugin System

**Objectif :** PluginManager + IPlugin.

**Fichiers :** `plugin/IPlugin.h`, `plugin/PluginManager.h`, `plugin/PluginRegistry.h`, `plugin/NativeLoader.cpp`, `app/PluginLoader.cpp`.

**Critères :** PluginManager charge .so/.dll, plugins peuvent enregistrer types.

## 13. Phase 12 : External plugin proof

**Objectif :** prouver l'architecture via un plugin externe.

**Fichiers :** `tests/external-plugin/` (repo séparé), `tests/external_plugin_test.cpp`.

**Critères :** test passe.

## 14. Phase 13 : 3D foundation

**Objectif :** préparer la 3D sans l'implémenter complètement.

**Fichiers :** ajout `Point3`, `Vector3`, `Transform3`, `BoundingBox3`, `OctreeIndex`, `Camera3D`, `Entity3D`.

**Critères :** 2D et 3D coexistent dans le Document.

## 15. Phase 14 : 3D implementation

**Objectif :** fonctionnalités 3D complètes.

**Fichiers :** entités 3D, booléens 3D, rendu 3D, workbench 3D.

**Critères :** BCAD est une plateforme 2D/3D.

## 16. Critères de fin de migration

| Critère | Mesure |
|---------|--------|
| Aucun header CGAL dans API publique | grep |
| Aucun header Qt dans Core | grep |
| Aucun header render dans Core | grep |
| find_package(BCAD) fonctionne | test |
| External plugin test passe | test |
| PropertyPanel dynamique | test |
| Undo/Redo via Transaction | test |
| Document fonctionne sans render | test |
| EntityRegistry dynamique | test |
| SerializerRegistry dynamique | test |

## 17. Conclusion

Migration progressive. Chaque phase est réversible, a des critères d'acceptation clairs, est testable indépendamment.