# Plan de migration BCAD

> Statut : migration en cours. Ce plan recale la feuille de route cible sur
> l'etat reel du depot et sert de checklist d'execution.

## 1. Objectif

Migrer BCAD d'une application CAO 2D fonctionnelle vers une plateforme
extensible Core + SDK + plugins, sans casser l'existant et sans aggraver les
violations architecturales connues.

Les ADR applicables sont principalement :

- ADR-001 : Core independant du renderer
- ADR-002 : CGAL non expose dans l'API publique
- ADR-003 : entites extensibles par registry
- ADR-004 : serialisation par SerializerRegistry
- ADR-005 : plugins dynamiques
- ADR-006 : SDK public versionne
- ADR-008 : index spatial independant du renderer
- ADR-009 : commandes et transactions pures C++
- ADR-010 : EventBus type

## 2. Etat courant

Le depot est deja en migration.

Fait ou en cours :

- `include/bcad/index/ISpatialIndex.h` existe.
- `include/bcad/index/QuadtreeIndex.h` existe.
- `src/index/QuadtreeIndex.cpp` existe.
- `Document` possede maintenant un `std::unique_ptr<index::ISpatialIndex>`.
- `render/Quadtree.*` est supprime du working tree.
- `TessellationTypes` a ete deplace vers `core`.

Violations restantes connues :

- CGAL est encore expose dans `include/bcad/geometry/Types.h` et propage dans
  plusieurs headers publics.
- `EntityType` reste un enum ferme.
- Plusieurs modules utilisent encore `switch(EntityType)`.
- Le SDK installable et le systeme de plugins n'existent pas encore.

## 3. Phase 0 - Stabiliser Core / Index

Objectif : terminer proprement la migration deja commencee autour de
`ISpatialIndex`.

Taches :

- Verifier que `include/bcad/core/Document.h` n'inclut aucun header `render/`.
- Verifier que `bcad_core` ne depend plus de `bcad_render`.
- Verifier que `bcad_render` consomme les types de tessellation depuis `core`.
- Conserver `QuadtreeIndex` dans le module `index`.
- Compiler et tester.

Commandes :

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Critere de sortie :

- Build OK.
- Tests OK.
- Aucune dependance Core -> Render.

Commit conseille :

```text
refactor(core): decouple document from render quadtree
```

## 4. Phase 1 - Documentation de l'etat migre

Objectif : aligner la documentation sur le resultat de la Phase 0.

Taches :

- Mettre a jour `docs/ARCHITECTURE_REVIEW.md`.
- Mettre a jour `docs/ARCHITECTURE_ROADMAP.md`.
- Mettre a jour `docs/SPATIAL_INDEX.md` si l'API reelle diverge.

Critere de sortie :

- La violation Core -> Render n'est plus listee comme actuelle si la Phase 0
  est validee.
- Les violations restantes sont toujours visibles.

Commit conseille :

```text
docs(architecture): update spatial index migration status
```

## 5. Phase 2 - Masquer CGAL

Objectif : appliquer ADR-002 en confinant CGAL a l'implementation.

Taches :

- Introduire des value types publics BCAD (`Point2`, `Vector2`, puis au besoin
  `Point3`, `Vector3`).
- Deplacer les aliases CGAL dans `geometry/detail/`.
- Ajouter des conversions internes `toCgal` / `fromCgal`.
- Migrer progressivement les entites natives.
- Migrer `BooleanOps`, `Triangulation`, `SnapGeometry` et les helpers.

Critere de sortie :

```bash
rg "CGAL" include/bcad/geometry
```

ne doit plus trouver de type ou header CGAL expose publiquement.

Commit conseille :

```text
refactor(geometry): hide CGAL behind public value types
```

## 6. Phase 3 - Introduire TypeId

Objectif : preparer ADR-003 sans supprimer brutalement `EntityType`.

Taches :

- Ajouter un type public `TypeId`.
- Ajouter `Entity::typeId()`.
- Donner un `TypeId` stable a chaque entite native.
- Garder temporairement `Entity::type()` pour les chemins existants.

Critere de sortie :

- Toutes les entites natives exposent un `TypeId`.
- Le code compile encore avec les chemins `EntityType` existants.

Commit conseille :

```text
feat(geometry): introduce entity TypeId
```

## 7. Phase 4 - EntityRegistry

Objectif : commencer l'extension par registry.

Taches :

- Ajouter `include/bcad/registry/EntityRegistry.h`.
- Enregistrer les entites natives.
- Exposer les metadonnees utiles : identifiant, nom, factory.
- Adapter les nouveaux chemins de code au registry.

Critere de sortie :

- Les entites natives peuvent etre decouvertes via registry.
- L'ancien `EntityType` peut encore exister pendant la transition.

## 8. Phase 5 - Retirer les switch EntityType

Objectif : enlever les points de fermeture qui bloquent les plugins.

Ordre conseille :

1. `src/io/Database.cpp`
2. `src/io/DxfWriter.cpp`
3. `src/app/PropertiesPanel.cpp`
4. `src/app/SnapEngine.cpp`
5. `src/geometry/SnapGeometry.cpp`
6. `src/app/Viewport.cpp`

Critere de sortie :

- Les nouveaux comportements passent par registries, serializers ou methodes
  polymorphes ciblees.

## 9. Phase 6 - SerializerRegistry

Objectif : appliquer ADR-004.

Taches :

- Ajouter `IEntitySerializer`.
- Ajouter `SerializerRegistry`.
- Migrer la persistence SQLite.
- Migrer DXF ensuite, si la separation reste claire.

Critere de sortie :

- Les entites peuvent etre sauvegardees sans modifier un switch central.

## 10. Phase 7 - Commands / Transactions

Objectif : appliquer ADR-009.

Taches :

- Ajouter `commands::Command`.
- Ajouter `commands::Transaction`.
- Garder Qt confine dans `app`.
- Ajouter un adaptateur Qt pour `QUndoCommand`.

Critere de sortie :

- Le Core peut executer/annuler une transaction sans Qt.

## 11. Phase 8 - EventBus type

Objectif : appliquer ADR-010.

Taches :

- Ajouter `events::EventBus`.
- Emettre les evenements document : ajout, suppression, modification, clear.
- Remplacer progressivement les callbacks ad hoc.

Critere de sortie :

- Les clients peuvent s'abonner aux evenements typés du document.

## 12. Phase 9 - SDK installable

Objectif : appliquer ADR-006.

Taches :

- Ajouter les regles `install()`.
- Exporter les targets CMake.
- Ajouter `BCADConfig.cmake`.
- Tester un projet externe minimal avec `find_package(BCAD CONFIG REQUIRED)`.

Critere de sortie :

- Un projet externe peut compiler contre BCAD sans inclure de chemins internes.

## 13. Phase 10 - Plugin system

Objectif : appliquer ADR-005.

Taches :

- Ajouter `plugin::IPlugin`.
- Ajouter `PluginRegistry`.
- Ajouter `PluginManager`.
- Charger via `dlopen` / `LoadLibrary`.
- Ajouter une preuve avec un plugin externe minimal.

Critere de sortie :

- Un plugin externe peut s'enregistrer au demarrage.

## 14. Regle d'execution

Chaque phase doit rester petite, compiler, passer les tests et ne pas ajouter
de nouvelle violation architecturale. Les migrations risquées doivent garder
un chemin de compatibilite temporaire plutot que forcer un big bang.
