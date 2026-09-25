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
| Surface publique réduite à ce qui l'est vraiment : les 10 en-têtes d'hôte quittent `include/bcad/` pour `src/app/`, la garde Qt (ADR-009) porte sur l'arbre public entier, `prove_sdk.sh` échoue si un en-tête d'hôte réapparaît dans l'installation | FAIT (décision A3) | ce commit |
| Geste 1 : `DxfWriter` n'écrit plus de métier cadastral (étiquettes, cotation 1:500, calques `CADASTRE_*` supprimés) ; les propriétés partent en XDATA générique `BCAD_PROPS`, types inclus, et `writeDxf` perd son paramètre `fullCadastre` | FAIT | ce commit |
| `ctest` | 40/40 | vérifié en continu |

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

## 5. Problèmes restants / prochaines étapes

- `Viewport.cpp` (1318 lignes) n'est pas découpé, et `ValidationResultsPanel`
  n'est pas extrait de `MainWindow` : la découpe en cinq TU a posé la règle, pas
  extrait les collaborateurs.
- Aucun point d'extension d'**import** : `IFileExporter` écrit, rien ne lit
  depuis un module.
- Les calques et styles cadastraux sont des données JSON sans lecteur
  (`CADASTRE_PLUGIN_STATUS.md`) : il manque un point d'extension de styles.
- `src/io/Database.cpp` écrit une table `cadastre_parcels` en dur : littéral
  métier hors de `src/app/`, donc hors du champ de `check_arch.sh` §10bis.
  (`DxfWriter.cpp` ne nomme plus aucune clé : passe à `BCAD_PROPS` depuis le
  geste 1.)
- `prove_sdk.sh` et `prove_cadastre.sh` tournent dans `ctest`, pas comme étapes
  distinctes de la CI.

### Programme approuvé par le mainteneur (dans cet ordre)

| Étape | Contenu | Statut |
|-------|---------|--------|
| A3 | `include/bcad/app/` → `src/app/` : la surface publique n'est plus qu'une API réelle (ADR-006/009) | **FAIT** (ce commit) |
| Geste 1 | Retirer le métier de `DxfWriter` : plus d'échelle 1:500, de hauteur 2 mm, de calque `CADASTRE_ETIQUETTES` ni de composition d'étiquette côté hôte ; `writeDxf(path, doc)` sans option métier, propriétés exportées en XDATA générique `BCAD_PROPS` | **FAIT** (ce commit) — la composition d'un document d'export cadastral reste à écrire **côté module** |
| Geste 2 | `.bcad` v2 : table générique `entity_properties(entity_id, key, value_json)`, `user_version` 1→2, migration transactionnelle et précédence testée, **plus la conservation opaque des entités dont le plugin est absent** (`if (!entity) continue;` est interdit) | approuvé, non commencé |
| Garde | `check_arch.sh` étendu à `src/io/` en deux niveaux : dépendances interdites vers un module métier, puis liste courte d'identifiants de contrat réels | approuvé, à faire **après** les gestes. Un cas à trancher à l'écriture : la lecture de compatibilité `BCAD_CADASTRE`, laissée volontairement dans `src/io/DxfReader.cpp`, correspond au motif `CADASTRE_` |

Le point qui distingue le geste 2 d'un simple nettoyage : aujourd'hui une
entité de type inconnu est **déclarée puis abandonnée** (`type=5` dans
`entities`, attributs dans `cadastre_parcels`), donc ouvrir puis resauvegarder
un fichier avec une installation incomplète détruit les deux copies. Le test «
ouvrir sans le plugin, sauvegarder, rouvrir avec le plugin, tout est encore là »
est le test qui prouve l'extensibilité annoncée.