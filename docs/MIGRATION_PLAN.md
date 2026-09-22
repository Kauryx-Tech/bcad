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

Le depot est deja en migration et **compile et passe ses tests** :

```
cmake --build build -j
ctest --test-dir build --output-on-failure   # 8/8 tests OK
```

Fait (build vert, commit `7413086` + corrections) :

- `include/bcad/index/ISpatialIndex.h` et `QuadtreeIndex.h` existent.
- `src/index/QuadtreeIndex.cpp` existe.
- `Document` possede un `std::unique_ptr<index::ISpatialIndex>`.
- `render/Quadtree.*` est supprime du working tree.
- `TessellationTypes` est deplace vers `core`.
- `TypeId` et `EntityRegistry` existent (`include/bcad/registry/`, `src/registry/`) ;
  un seul registre (plus de doublon `bcad::geom::EntityRegistry`).
- `SerializerRegistry` (`include/bcad/serialization/`, `src/serialization/`).
- `EventBus` type (`include/bcad/events/`), header-only, sans dependances de lien.
- Commandes pures C++ (`include/bcad/commands/`, `src/commands/`).
- Plugin manager (`include/bcad/plugin/`, `src/plugin/`).
- Systeme de proprietes (`include/bcad/properties/`, `src/properties/`).
- Tests supplementaires : arch, typeid_stability, roundtrip, command,
  sdk_abi, command_pattern, geometry_utils2.

Corrections apportees pour verdir le build :

- Cycle d'inclusion `Entity.h <-> PropertyMap.h <-> EventBus.h` resolu :
  `Entity.h` ne fait que forward-declarer `bcad::properties::PropertyMap`
  (membres en reference uniquement).
- `PropertyMap` est copiable (deep copy via `Property::clone()`), ce qui
  restaure le `clone()` des entites (Line, Circle, Arc, Point, Polyline).
- `PolylineEntity` est aligne sur les autres entites : membre `PropertyMap`
  par valeur (plus de `unique_ptr`), copie implicite restauree.
- `enum class PropertyType` ajoute dans `PropertyTypes.h` (etait manquant).
- `bcad_events` est un INTERFACE target header-only : plus de lien vers
  core/geometry, ce qui casse le cycle de bibliotheques
  `geometry -> properties -> events -> geometry`.
- `bcad_geometry` expose `bcad_properties` dans son interface de lien.

Violations restantes connues :

- `EntityType` reste un enum ferme (deprecie, garde pour compat) ; l'ecriture
  de fichiers (`src/io/Database.cpp`) map desormais `typeId()` vers l'entier
  legacy (`legacyTypeInt`, plus aucun appel a `type()` deprecie) et le seul
  `switch(EntityType)` restant est la conversion de lecture de fichiers
  legacy (retenu volontairement).
- Le SDK installable (Phase 9, test `sdk_external_test`) et le systeme de
  plugins (Phase 10, preuve plugin externe) sont finalises ; l'ABI plugin est
  unique (cible ADR-005, voir note Phase 10).

## 3. Phase 0 - Stabiliser Core / Index

Statut : **Terminee** (build vert, 8/8 tests OK, `scripts/check_arch.sh` ne
signale aucune dependance Core -> Render ni include `render/` dans core).

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

Statut : **Terminee** (voir `docs/CGAL_MIGRATION.md`). Aucun type ni header
CGAL dans les headers publics hors `detail/` ; `check_arch.sh` passe.

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

Statut : **Terminee** (`TypeId` stable + `Entity::typeId()` sur toutes les
entites natives ; `Entity::type()` garde deprecie pour la compat).

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

Statut : **Terminee** (`bcad::registry::EntityRegistry` + macro
`BCAD_REGISTER_ENTITY`, entites natives enregistrees). Consolidation faite :
l'ancien registre `bcad::geom::EntityRegistry` a ete supprime ; l'API plugin
et `PluginManager` utilisent `bcad::registry::EntityRegistry`. Le registre
unifie supporte les factories sans argument (par defaut) et les factories a
parametres serialises (CSV, meme format que `serializeParams()`), ce qui
couvre l'ancienne capacite de deserialisation du registre historique.

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

Statut : **Terminee** pour `DxfWriter`, `PropertiesPanel`, `SnapEngine`,
`SnapGeometry`, `Viewport` (migres vers `typeId()`). Le seul reste est la
conversion legacy dans `src/io/Database.cpp` (compatibilite fichiers, retenue
volontairement).

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

Statut : **Terminee** (ADR-006 applique, verifie par le test `sdk_external_test`).

Taches realisees :

- Regles `install()` pour tous les modules (headers, targets avec export
  `BCADTargets`, fichiers `.so`/`.a`).
- `BCADConfig.cmake` genere par `configure_package_config_file` (relocalisable,
  `set_and_check` des chemins), plus `BCADConfigVersion.cmake`
  (Compatibilite `SameMajorVersion`).
- Targets importes `BCAD::bcad_*` (prefixe + nom de bibliotheque) :
  `find_package(BCAD CONFIG REQUIRED)`.

Critere de sortie :

- Preuve fournie par `examples/sdk_proof` : consommateur externe compile avec
  `find_package(BCAD CONFIG REQUIRED)` et lie `BCAD::bcad_core`/`BCAD::bcad_geometry`
  (test `sdk_external_test`, execute par `scripts/prove_sdk.sh`).

## 13. Phase 10 - Plugin system

Statut : **Terminee** pour l'ABI active `bcad::plugin::Plugin.h`
(ADR-005 applique, charge via `dlopen`).

Taches realisees :

- `PluginManager` (interface + `pluginManager()` singleton) dans `bcad_plugin`
  (SHARED) : chargement via `dlopen`/`dlsym`, verification `PLUGIN_API_VERSION`,
  enregistrement de types d'entites et de commandes.
- API publique exportee : macro `BCAD_PLUGIN_API` (`-fvisibility=hidden`,
  seuls les symboles publics sont visibles).
- Correctif de reentrance : `loadPlugin`/`unloadPlugin` ne tiennent plus le
  mutex pendant `bcad_plugin_init`/`bcad_plugin_shutdown` (le plugin re-entre
  dans `PluginManager` a l'enregistrement, interblocage reel corrige).

Preuve :

- `examples/sdk_proof/plugin` : plugin externe minimal construit contre le SDK
  installe, enregistre le type d'entite `hello.marker` et la commande
  `hello.greet` au chargement ;
- `examples/sdk_proof/loader` : charge le plugin, verifie ses metadonnees,
  PUIS que les enregistrements aboutissent bien dans les registres globaux de
  l'hote (mediation effective, une seule instance), et le decharge
  (test `sdk_external_test`).

Critere de sortie :

- Un plugin externe peut s'enregistrer au demarrage et ses extensions sont
  visibles depuis l'hote (valide par le test).

Note ABI : duplication resolue — l'ABI est consolidee sur la cible ADR-005 :
`bcad_plugin_init(PluginRegistry&)` comme unique point d'entree, PluginRegistry
concret (metadonnees + enregistrement entites/commandes/serializers, mediatise
par l'hote pour garantir une seule instance des registres), PluginManager
reduit au cycle de vie. Les anciennes interfaces `IPlugin`,
`PluginManager.h` et l'ancien `PluginRegistry` (modele objet IPlugin) ont ete
supprimees.

Detail liaison (mediation effective, decouvert lors du renforcement de la
preuve) : `libbcad_plugin.so` est mince (symboles des registres non definis,
resolus depuis l'hote) ; `bcad_registry`, `bcad_commands` et
`bcad_serialization` sont compiles en visibilite par defaut (plus `hidden`) ;
l'hote lie `BCAD::bcad_plugin` + ces trois bibliotheques. La verification
end-to-end `hello.marker` dans `EntityRegistry` de l'hote couvre ce point.

## 14. Regle d'execution

Chaque phase doit rester petite, compiler, passer les tests et ne pas ajouter
de nouvelle violation architecturale. Les migrations risquées doivent garder
un chemin de compatibilite temporaire plutot que forcer un big bang.
