# État d'avancement — consolidation BCAD

> **Fichier vivant** : chaque tâche menée par un agent met à jour cette feuille
> (voir `AGENTS.md`, workflow). Les références `#<sha>` pointent les commits.

## 1. Consolidation plugin (ADR-005)

| Tâche | Statut | Trace |
|-------|--------|-------|
| ABI `EntityFactory`/`CommandFactory` en pointeurs de fonction (plus de `std::function` traversant le DSO), rewrap hôte dans `libbcad_plugin` | FAIT | `2e6df52` |
| `PLUGIN_API_VERSION` 1→2, gate égalité stricte au chargement | FAIT | `2e6df52` |
| Partage des types C++ hôte↔plugin (resolve hôte, pas `-rdynamic` loader) | FAIT | `2e6df52`, `9af7565` |
| Branche MSVC (`/WHOLEARCHIVE`), CMake proof | FAIT (non testé sur Windows) | `2e6df52` |
| Cycle de vie serializers plugin (retrait avant `dlclose`) | FAIT | `bae000d` |
| Médiation par l'hôte réellement effective (pas d'accès direct au Core) | FAIT | `9af7565` |

## 2. Dette doc / cohérence (bannières « CIBLE non implémentée » devenues fausses)

| Document | Statut | Trace |
|----------|--------|-------|
| `EVENT_SYSTEM`, `SPATIAL_INDEX`, `PROPERTY_SYSTEM`, `COMMAND_SYSTEM`, `DOCUMENT_MODEL`, `RENDERING_ARCHITECTURE` | bannières corrigées (IMPLEMENTE) | `77f88b6` |
| `SDK_ARCHITECTURE.md` (§6 ex. CMake/header, §7 versioning, §8 politique ADR-011, §9/§10 fictifs) | FAIT | `77f88b6` |
| `EXTENDING_BCAD.md` (note haut, Entity réel, Command, EventBus, PropertyMap, CMake plugin, entry point, tests) | FAIT | `77f88b6` |
| `ARCHITECTURE.md` (banner + §5 table) | FAIT | `77f88b6` |
| `ENTITY_MODEL.md`, `PERSISTENCE_ARCHITECTURE.md` (banners) | FAIT | `77f88b6` |
| `WALKTHROUGH.md` (banner + §4-6 réconciliés à l'API réelle) | FAIT | `77f88b6` |
| `FIRST_CONTRIBUTION.md` (§4 entité, §5 plugin) | FAIT | `77f88b6` |
| `examples/README.md` + `01..04`/`sdk_proof` | FAIT | `77f88b6` |
| `ARCHITECTURE_ROADMAP.md` (table + statuts des 14 phases) | FAIT | `29924f1` |
| Bannières réellement cibles (workbench, 3D, DWG/SVG, versioning `.bcad`) | conservées (à NE PAS déformer) | — |

## 3. Outils / CI

| Tâche | Statut | Trace |
|-------|--------|-------|
| `scripts/check_arch.sh` : exit spurieux (pipefail + grep vide dans le décompte `EntityType::`) | FAIT (rc=0 si propre) | `29924f1` |
| `scripts/check_arch.sh` appelé par la CI (il n'avait aucun exécutant) | FAIT (première étape du job `build`) | `1cc3e9b` |
| Surface publique réduite à ce qui l'est vraiment : les 10 en-têtes d'hôte quittent `include/bcad/` pour `src/app/`, la garde Qt (ADR-009) porte sur l'arbre public entier, `prove_sdk.sh` échoue si un en-tête d'hôte réapparaît dans l'installation | FAIT (décision A3) | `36a0b84` |
| Geste 1 : `DxfWriter` n'écrit plus de métier cadastral (étiquettes, cotation 1:500, calques `CADASTRE_*` supprimés) ; les propriétés partent en XDATA générique `BCAD_PROPS`, types inclus, et `writeDxf` perd son paramètre `fullCadastre` | FAIT | `86f4942`, `dce23f2` |
| Geste 2 : `.bcad` v2 — table générique `entity_properties`, `type_id` en chaîne, `user_version` 1→2, migration transactionnelle à précédence testée, lecture de compatibilité v1, refus d'une version future, et entité de module absent conservée en `UnknownEntity` | FAIT | `52e8e7a` |
| Garde : `check_arch.sh` §12a/§12b interdit à `src/io/` **et** `include/bcad/io/` toute dépendance vers un module métier et toute identité de domaine, avec exemption explicite `NOLINT(arch-legacy-v1)` portée par la ligne du littéral de compatibilité | FAIT (les deux niveaux ont été prouvés capables d'échouer) | `f0cfee8` |
| Découpe : `Viewport.cpp` (1318 lignes) réparti sur sept unités par responsabilité (entrée, surimpressions, édition, trois familles d'outils), en-tête `Q_OBJECT` unique, `kPickToleranceScreenPx` seul symbole partagé, garde 11bis étendue à `src/app/Viewport*.cpp` | FAIT (à comportement constant : moc identique, `nm` sans perte hors inlining) | `1c82310` |
| Parcours cadastral en 14 étapes (dossier → profil → calques → layout A3 → vue 1:500 → cartouche alimenté → flèche/barre/légende/grille → nomenclature → validation → PDF vectoriel → DXF/GeoJSON/CSV → relecture sans perte → resauvegarde sans module) : **exécutable de 10 à 14, bibliothèque et tests seulement de 1 à 9** | DIAGNOSTIQUÉ | `layout_spike_test`, ADR-017 |
| Spike avant décision : sept obstacles de mise en page **mesurés** sur le code réel (échelle explicite respectée par la composition mais écrasée par `applySuggestedScale`, garde `fitsIn` sans appelant, vue unique et centrée d'office, mobilier deviné du contenu — 0 mm de bande pour dix champs d'attributs —, champs déclarés perdus à l'aller-retour, vocabulaire cadastral FR dans l'API publique) | FAIT (ne décide rien, 7 assertions) | `5aeebe1` |
| Obstacle 5 du spike, devenu bug : un cartouche rempli d'attributs autres que `commune`/`section`/`projectName` ne réservait pas sa bande et n'était pas peint — il disparaissait de la feuille sans un mot. `Cartouche::isValid()` regarde maintenant tout champ d'attribut, `echelle` tenu hors de la liste car la composition l'écrit elle-même | FAIT (correction minimale : ni changement de format, ni renommage, ni déplacement) | ce commit |
| ADR-017 « l'espace papier est un objet du document, son vocabulaire est déclaré » | **ACCEPTÉE** par le mainteneur : les 22 champs de `Cartouche` quittent l'API publique (rupture assumée, remplacée par une API générique de champs / gabarits / résolution / diagnostics, pas par un `PropertyMap` en fuite), et `.bcad` passe directement de v2 à v3 — migration atomique, testée, idempotente, préservant les données des plugins absents | `5aeebe1`, ce commit |
| Ordre de mise en œuvre de l'ADR-017, consigné dans l'ADR : tranche 1 **sans persistance** (`Document::properties()` en mémoire, cartouche alimenté, « Plan cadastral » et la première parcelle avec `break` retirés, sélecteur de profil dans l'UI), puis tranche 2 = format v3, derrière cinq portes | À FAIRE (aucun code de migration avant la 5ᵉ porte) | ADR-017 §« dans quel ordre » |
| `ctest` | 42/42 | vérifié en continu |

## 4. Journal des commits

- `9af7565` — `fix(plugin): rendre la mediation par l'hote reellement effective`
- `2e6df52` — `fix(plugin): ABI factories → pointeurs de fonction + rewrap hôte (ADR-005)`
- `bae000d` — `fix(plugin): cycle de vie des serializers plugin (retrait avant dlclose)`
- `77f88b6` — `docs: nettoyer la dette obsolete (statuts realises, exemples a jour)`
- `29924f1` — `fix(ci): check_arch.sh retourne 0 quand propre (pipefail)`
- `1769c2a` — workbench déclaré par le plugin, menus générés par l'hôte (ABI v3)
- `8077ac8` — validateurs branchés + feuille imprimée composée par un peintre partagé (ABI v4)
- `035ed77` — export de fichier comme point d'extension (ABI v5)
- `40d9d86` — audit `MainWindow` corrigé, puis classe répartie sur cinq TU
- `815a210` — F4 : la recherche par référence cadastrale revient, côté module (ABI v6)
- `1e32031` — C3 : le module lit son gabarit de profil (ABI v7)
- `1cc3e9b` — `check_arch.sh` devient une étape de la CI
- `36a0b84` — A3 : `src/app/` privé, `include/bcad/` = API publique seule
- `86f4942`, `dce23f2` — geste 1 : le DXF ne connaît que des propriétés typées
- `52e8e7a` — `.bcad` v2 : les propriétés dans une table générale, le module absent ménagé
- `f0cfee8` — `src/io/` sous garde métier, ADR-004/015/016 alignés sur le format v2
- `1c82310` — `Viewport.cpp` réparti sur sept TU par responsabilité, garde de taille étendue

## 5. Problèmes restants / prochaines étapes

- `Viewport.cpp` est découpé, mais la machine à états des outils reste un
  `enum ToolMode` piloté par des `switch` dans l'hôte : un module ne peut pas
  ajouter un outil interactif, seulement une commande. `ValidationResultsPanel`
  n'est pas non plus extrait de `MainWindow`.
- Les étapes 1 à 9 du parcours restent hors de portée : l'ADR-017 est **acceptée**,
  mais rien de sa mise en œuvre n'est écrit. La tranche 1 (attributs du dossier en
  mémoire, cartouche alimenté, sélecteur de profil) conditionne la tranche 2
  (format v3), qui conditionne l'étape 13 pour la mise en page. Le spike
  `layout_spike_test` liste les six obstacles restants ; il est à convertir en
  tests de contrat au fur et à mesure de la mise en œuvre, pas à supprimer quand
  une assertion casse. L'obstacle 5 (bande non réservée) a été corrigé hors de là,
  et son bloc est devenu une assertion de contrat.
- `Document` n'a **aucun** `PropertyMap` : les métadonnées du dossier
  (projet, phase, géomètre, dossier) n'ont pas où vivre, ce qui rend l'étape 7
  impossible même avec un layout persisté.
- Aucun point d'extension d'**import** : `IFileExporter` écrit, rien ne lit
  depuis un module.
- Les calques et styles cadastraux sont des données JSON sans lecteur
  (`CADASTRE_PLUGIN_STATUS.md`) : il manque un point d'extension de styles.
- `src/io/Database.cpp` n'écrit plus de table métier : `cadastre_parcels` a
  disparu de l'écriture, ne reste que comme source de la migration v1→v2, et
  `check_arch.sh` §12a/§12b surveille désormais `src/io/` comme `src/app/`.
- `Database::migrateSchema` et `Database::schemaVersion` sont exposés et testés
  mais **n'ont aucun appelant dans l'hôte** : ouvrir puis resauvegarder un
  fichier v1 produit déjà un v2, donc rien ne demande une migration explicite
  aujourd'hui. À garder sous ce statut, ou à retirer si un CLI de migration
  n'arrive jamais.
- La lecture de compatibilité `BCAD_CADASTRE` (XDATA legacy, `kLegacyAppId`
  dans `src/io/DxfReader.cpp`) est un identifiant de domaine que la garde §12b
  ne vise pas — le motif `CADASTRE_` ne correspond pas à `BCAD_CADASTRE`. C'est
  toléré pour la lecture seule ; l'écriture ne nomme plus rien.
- `prove_sdk.sh` et `prove_cadastre.sh` tournent dans `ctest`, pas comme étapes
  distinctes de la CI.

### Programme approuvé par le mainteneur (dans cet ordre)

| Étape | Contenu | Statut |
|-------|---------|--------|
| A3 | `include/bcad/app/` → `src/app/` : la surface publique n'est plus qu'une API réelle (ADR-006/009) | **FAIT** (`36a0b84`) |
| Geste 1 | Retirer le métier de `DxfWriter` : plus d'échelle 1:500, de hauteur 2 mm, de calque `CADASTRE_ETIQUETTES` ni de composition d'étiquette côté hôte ; `writeDxf(path, doc)` sans option métier, propriétés exportées en XDATA générique `BCAD_PROPS` | **FAIT** (`86f4942`, `dce23f2`) — la composition d'un document d'export cadastral reste à écrire **côté module** |
| Geste 2 | `.bcad` v2 : table générique `entity_properties(entity_id, key, value_json)`, `user_version` 1→2, migration transactionnelle et précédence testée, **plus la conservation opaque des entités dont le plugin est absent** (`if (!entity) continue;` est interdit) | **FAIT** (`52e8e7a`) — v1 lu, v2 seul écrit, v3 refusé sans toucher à l'octet ; `UnknownEntity` porte le type et la charge utile jusqu'au retour du module |
| Garde | `check_arch.sh` étendu à `src/io/` en deux niveaux : dépendances interdites vers un module métier, puis liste courte d'identifiants de contrat réels | **FAIT** (`f0cfee8`) — le cas annoncé ne s'est pas présenté : `BCAD_CADASTRE` ne correspond pas au motif `CADASTRE_`, aucune exemption n'était nécessaire pour lui |

Hors de ce programme, à la demande du mainteneur : `Viewport.cpp`, dernier
fichier du dépôt hors de la règle de taille, est découpé à son tour par
responsabilité (ce commit). La découpe est une répartition de corps, pas une
refonte : les seize gestionnaires d'outils extraits du `switch` de
`placePoint` et la boucle de texte sortie de `paintGL` sont les seuls changements
de structure, et le moc produit un `moc_Viewport.cpp` bit à bit identique.

Le point qui distinguait le geste 2 d'un simple nettoyage était le suivant : une
entité de type inconnu était **déclarée puis abandonnée** (`type=5` dans
`entities`, attributs dans `cadastre_parcels`), donc ouvrir puis resauvegarder un
fichier avec une installation incomplète détruisait les deux copies. C'est
devenu un test : `testBlindSessionDestroysNothing` et
`testEquippedWriteBlindWriteEquippedRead` (`tests/unit/io/BcadSchemaTest.cpp`)
ferment la boucle — sans le module, la parcelle reste opaque mais présente ;
avec le module, ses attributs reviennent intacts. Vérifié par mutation : rendre
`UnknownEntity` introuvable fait échouer la suite, pas seulement le test.