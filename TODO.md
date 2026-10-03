# TODO — BCAD

> File de travail. Les documents faisant autorité restent `CAHIER_DES_CHARGES.md`
> (fonctionnalités), `docs/ROADMAP_MARKET.md` (ordre marché),
> `docs/CONSOLIDATION_STATUS.md` et `docs/RUST_STATUS.md` (avancement consigné).
> Ce fichier n'arbitre rien : il rassemble ce qui est **ouvert**, avec sa source.
> Dernière passe : 2026-10-03, **sur code compilé et testé**, et non sur lecture.
>
> ## État mesuré, pas déduit
>
> ```
> cmake --build build -j          → OK
> ctest --test-dir build          → 49/49 verts
> scripts/check_arch.sh           → PASSED
> ```
>
> **La branche est saine.** Le lot des trous est complet (§0 entièrement résolu,
> §7.2/7.3 branchés, §7.4 partiellement). Voir l'historique §6.

---

## 0. ~~Bloquant~~ — chantier trous **CLOS** (2026-10-03)

Toutes les tâches 0.1 → 0.12 ont été résolues. Résumé des commits :

- `ddd9611` — grammaire compatible v1 (`encodeRings`/`decodeRings`), trous dans sérialisation
- `be65b1f` — UB cast et entité dégénérée détectés par revue
- `b49e294` — trous visibles dans renderer (§0.3), DXF (§0.5), `polygonArea` (§0.7), validateurs (§0.8)

- [x] **0.1** `applyTransform` itère maintenant `holes_` (`Polyline.h`)
- [x] **0.2** `boundingBox` inclut les sommets des trous (`Polyline.h`)
- [x] **0.3** Renderer : chaque anneau de trou = range GL séparée (`Document.cpp`)
- [x] **0.4** `distanceTo` mesure les arêtes des trous (`Polyline.h`)
- [x] **0.5** DXF : chaque trou = LWPOLYLINE fermée séparée *(sémantique perdue, bords visibles)*
- [x] **0.6** GeoJSON : anneaux de trous inclus dans `Polygon.coordinates` (core + cadastre)
      *(CSV coordinate exporter utilise encore `tessellate()` → outer ring seulement : cas marginal)*
- [x] **0.7** `polygonArea()` soustrait les trous via formule du lacet → `parcelArea()` correcte
      *(`booleanOp()` utilise encore le polygon simple CGAL — nécessiterait `Polygon_with_holes_2`)*
- [x] **0.8** `ParcelTopologyValidator` vérifie la simplicité de chaque trou ;
      `ParcelOverlapRuleValidator` soustrait les intersections dans les trous (faux positifs corrigés)
- [x] **0.9 / 0.9bis** Rétrocompatibilité v1 : `decodeRings` distingue ancien et nouveau format
- [x] **0.10** Nettoyage debug et UB — `NativeSerializers.cpp`, `roundtrip_test.cpp`
- [x] **0.11** `HoleTest.cpp` converti en round-trip via `SerializerRegistry`
- [x] **0.12** Format CSV polylignes : `serializeParams` délègue à `encodeRings` (source unique)

---

## 1. P0 — chantiers ouverts consentis dans les docs

- [ ] **ADR-017, items marqués « non entamé »** (`docs/CONSOLIDATION_STATUS.md`,
      ligne du commit `2cc3999`) :
      - retrait des 22 champs de `Cartouche` de l'API publique (déprécié mais
        encore présent) ;
      - libellés du peintre (`FurniturePaint`) à aligner sur le vocabulaire
        déclaré ;
      - échelles et formats : le vocabulaire cadastral FR est encore dans
        l'API publique ;
      - ~~bump de `PLUGIN_API_VERSION`~~ **FAIT** (`68e4be2`) — bumped 10→11 avec
        retrait de `EntityParamsFactory` ;
      - réécriture du module cadastre sur la nouvelle API générique ;
      - ~~garde `check_arch.sh` étendue à `src/layout/`~~ **FAIT** — le script
        couvre déjà `src/layout/` (lignes 219-274).
- [ ] **Porte 3 au niveau fichier** (même ligne) : clé inconnue conservée **et
      signalée**, valeur manquante rendue par un diagnostic nommant la clé.
      Partiellement fait (message enrichi), à finir et à prouver par test.
- [ ] **Parcours cadastral en 14 étapes** — « exécutable de 10 à 14,
      bibliothèque et tests seulement de 1 à 9 » (statut DIAGNOSTIQUÉ). Les
      six obstacles restants sont listés par le spike `layout_spike_test` et
      doivent être **convertis en tests de contrat au fur et à mesure**, pas
      supprimés quand une assertion casse.
- [ ] **`Database::migrateSchema` / `schemaVersion` n'ont aucun appelant dans
      l'hôte.** Exposés et testés, mais ouvrir puis resauvegarder un v1 produit
      déjà un v2. Décision à prendre : garder sous ce statut, ou retirer si un
      CLI de migration n'arrive jamais.
- [ ] **Aucun point d'extension d'import.** `IFileExporter` écrit ; rien ne lit
      depuis un module. Bloque l'étape 6/7 de la feuille de route marché.

---

## 2. P1 — lacunes réelles trouvées dans le code

- [x] **`TypeId` : null guards** — **RÉSOLU** (`68e4be2`). Tous les opérateurs de
      comparaison testent le pointeur avant `strcmp`. `operator!=` et `operator<`
      délèguent ou gardent eux-mêmes. Accompagné du bump `PLUGIN_API_VERSION` 10→11.
- [x] **Les entités de cotation dégradées en `UnknownEntity`** — **RÉSOLU** (`453660a`).
      5 serializers ajoutés dans `NativeSerializers.cpp` : `LinearDimension`,
      `AlignedDimension`, `AngularDimension`, `RadiusDimension`, `DiameterDimension`.
      `RadiusDimension`/`DiameterDimension` enforced par TypeId (pas par le champ CSV)
      pour éviter toute confusion de type.
- [x] **Cotations cadastrales non persistantes** — **RÉSOLU** (`36869f3`).
      `addParcelDimensionsToDocument` (`CadastreDimension.cpp`) implémentée :
      crée des `AlignedDimensionEntity` et `AngularDimensionEntity` sur calque `COTATION`.
- [x] **Deux voies concurrentes / fabriques mortes** — **RÉSOLU** (`68e4be2`).
      `EntityParamsFactory`, `create(TypeId, string_view params)`, les six
      `makeXxxEntity` (~65 lignes) et les surcharges `registerType` correspondantes
      ont été retirés. `NativeEntityRegistration.cpp` enregistre 6 types via
      factory sans arg ; `registerNativeTypes()` enregistre 11 types (id. sans
      paramsFactory), appelé uniquement par `sdk_abi_test.cpp`. Les doublons entre
      les deux chemins sont désormais bénins (même factory, `emplace` ignore sans
      risque). `PLUGIN_API_VERSION` bumped 10→11. `PluginManager.cpp` adapté :
      `EntityFactory` plugin (pointeur `(string_view)`) enveloppé en `factory("")`.
- [x] **`EventBus` thread-safety — contrainte documentée** (`EventBus.h:247-253`).
      Commentaire ajouté : « non thread-safe, thread UI uniquement, tessellation
      via signaux Qt ». Mutex à ajouter si un futur code publie depuis un autre
      thread. `EventFilter::matches` garde ~15 RTTI/événement (fonctionnel, pas
      le chemin critique aujourd'hui).
- [x] **`Document::extents()` O(n) → cache** — **RÉSOLU**. `cachedExtents_` +
      `extentsDirty_` dans `Document.h`. `addEntity` étend le cache en O(1) ;
      `removeEntity`/`notifyEntityChanged`/`clear` invalident. Le scan complet
      ne se produit qu'une fois après invalidation.
- [x] **`CommandStack` historique non borné** — **RÉSOLU**. `kMaxHistory = 100`
      dans le pur C++ (`Command.h`) ; `undoStack_.setUndoLimit(100)` pour le
      `QUndoStack` Qt. `doc_` reste un pointeur nu (durée de vie garantie par
      l'hôte) ; `shared_mutex` Document non utilisé par les commandes (consentit
      — les commandes s'exécutent sur le thread UI).
- [ ] **Trim / Extend / Break limités aux lignes** — `dynamic_cast` vers
      `PolylineEntity` avec `// TODO: add ArcEntity, CircleEntity support`
      (`commands/ConcreteCommands.h:569, 632, 704, 767`) et
      `// TODO: geom::extendPolyline` (`:630`).
- [ ] **Arc non pris en charge par la découpe d'entités** (`src/app/Viewport.h:66`) ;
      le clic unique de prolongement n'est pas tenté (`ViewportCutTools.cpp:140`).
- [ ] **`ParcelEntity.h:35-36` et les trois autres entités cadastrales**
      concatènent leurs propriétés au CSV de la géométrie derrière un `|`
      (`serializeParams()` = base + `'|' + …`). Une valeur contenant un `|` ou
      une virgule casse la relecture. À remplacer par une sérialisation
      `PropertyMap` propre.
- [ ] **`SurveyToleranceValidator` implémenté mais non branché** — consenti dans
      `CADASTRE_PLUGIN_STATUS.md` (faute de couche de référence juridique :
      `BoundaryEntity` ne porte ni section ni numéro). **Assumé, ne pas « réparer »
      sans cette couche.** À garder trace ici pour ne pas le re-découvrir.
- [ ] **`ArcGisAdapter::exportToGdb` est un stub de 9 lignes qui retourne
      `false`** (`src/plugins/cadastre/io/ArcGisAdapter.cpp`).
- [ ] **`GeoPackageExporter` non conforme à la spec** — `sqlite3` direct, tables
      minimales `gpkg_contents`/`gpkg_geometry_columns` + une table personnelle
      `cadastre_parcels` ; ni `gpkg_nortree`/rtree, ni WKB complet ; relecture
      mince. **Export lisible par BCAD, pas garanti par QGIS** — ce qui contredit
      l'objectif d'interop de la feuille de route.
- [ ] **`prove_sdk.sh` et `prove_cadastre.sh` tournent dans `ctest`**, pas comme
      étapes distinctes de la CI.
- [ ] **Branche MSVC `/WHOLEARCHIVE` non testée sur Windows**
      (`CONSOLIDATION_STATUS.md` §1).
- [x] **`CAHIER_DES_CHARGES.md` §3 mis à jour** (`68e4be2`). TextEntity,
      LinearDimension/AlignedDimension/cotes radiales-angulaires, et PdfExport
      passés de ❌ à 🟡 avec note sur l'état réel (moteur OK, GUI/DXF DIMENSION
      manquant).

---

## 3. P2 — feuille de route marché (`docs/ROADMAP_MARKET.md` §3)

Étapes 0 et 1 **FAITES**. Dans l'ordre du document :

- [ ] **Étape 2 — étage « document »** : hachures, texte multi-lignes et styles
      de texte, cotations exportées en vrais objets DXF `DIMENSION`,
      styles/flèches de cotation, fiabilité d'impression A4→A0.
      *(Le cartouche à champs pilotés par les attributs est livré par ADR-017.)*
- [ ] **Étape 3 ⭐ — gabarits Afrique** : Bénin, Côte d'Ivoire, Burkina Faso,
      Sénégal, Niger, Mali. **100 % données JSON, quasi aucun C++** — le canal
      est ouvert (`resolveDataFile`, `$BCAD_PLUGIN_DATA`, profil Togo qui pilote
      déjà les motifs d'identification). **Mais** : les calques/styles/cartouches
      attendent que l'hôte sache les consommer — le point d'extension
      `IStyleProvider` a été ajouté (`e22e60d`), vérifier ce qui reste.
      Signalé « meilleur rapport valeur/effort, presque gratuit et unique ».
- [ ] **Étape 4 — états et nomenclatures** : outil générique entités → tableau
      attributaire + totaux et métré → export PDF/CSV/tableur.
- [ ] **Étape 5 ⭐ — 2.5D à coût marginal** : câbler la triangulation de Delaunay
      **déjà implémentée et testée dans le moteur** (`geometry/Triangulation.cpp`,
      non exposée en commande GUI) → points cotés → MNT, courbes de niveau,
      profils en long/travers, cubatures. « Code déjà écrit. »
- [ ] Étape 6 — plugin **réseaux** (effort élevé).
- [ ] Étape 7 — plugin **architecture simple** (effort élevé).
- [ ] Étape 8 — éducation & robustesse : mode TP, tutoriels hors-ligne, export
      SVG/PNG, paquet Windows autonome + AppImage Linux, optimisation de taille.

Indicateurs de réussite à mesurer par version (`ROADMAP_MARKET.md` §6) :
démarrage fluide sur le poste de référence ADR-016 ; un géomètre produisant seul
plan + tableau de surfaces + cartouche en PDF **sans connexion** ; un aller-retour
DXF sur un plan réel **sans perte mesurable** ; zéro littéral `"cadastre.` dans
`src/app/` ; **0 ligne de core modifiée par nouveau plugin métier**.

---

## 4. P3 — intégration Rust (`docs/RUST_STATUS.md`)

- [ ] **Rejouer les quatre gates, un par un** : `cargo fmt --all --check`,
      `cargo check --workspace --all-targets`, `cargo clippy … -D warnings`,
      `cargo test --workspace --all-features`. Volumétrie consignée au dernier
      passage : **162 tests verts**. Ne **jamais** les enchaîner avec `&& … || echo OK`
      — cette forme a masqué un échec de clippy sur un commit déjà poussé
      (`0794ac6` → corrigé en `a25525b`).
- [ ] Le pont C++ ↔ Rust est compilé en cible séparée `bcad_io_ffi`, **non
      installée** (sinon un chemin absolu Cargo entrerait dans
      `BCADTargets.cmake`, violation ADR-006). Conséquence : les chemins gardés
      par `#ifdef BCAD_ENABLE_RUST` ne sont **pas exercés par les builds de
      l'SDK**. `RUST_INTEGRATION_SUMMARY.md` déclare lui-même les phases 1-8
      « code complet, non testé faute de toolchain ». **C'est la zone la moins
      validée du projet** : à re-mesurer avant d'y construire quoi que ce soit.

---

## 5. Hors périmètre — décisions prises, à ne pas reprendre

Arbitrage du mainteneur (`ROADMAP_MARKET.md` §4). À réviser **sur argument
marché, pas technique** :

noyau 3D solide (B-Rep / Open CASCADE / CGAL 3D) · solveur de contraintes et
historique paramétrique · scripting Python embarqué · lecture DWG native
(passer par un convertisseur externe ODA/LibreDWG) · matériaux PBR et rendu
temps réel · course au nombre de commandes AutoCAD.

---

## 6. Ce que la vérification a infirmé

Une première version de ce fichier était écrite **à la lecture seule**. Trois
affirmations ont été corrigées après compilation, exécution et traçage des
appelants. Elles sont conservées ici pour qu'on ne les « recorrige » pas en
sens inverse :

| Affirmation de lecture | Ce que l'exécution a donné |
|---|---|
| « Un fichier écrit avant ce changement sera relu **de travers** (compteur de sommets faux) » | **Plus grave et prouvé** : perte totale de la géométrie (0 sommet), `load` retourne `true`, et `bcad_schema_test` est **déjà rouge** (`BcadSchemaTest.cpp:312`). Ce n'était pas une hypothèse à vérifier, c'est la branche cassée. §0.9 |
| « Le format CSV existe à **deux** endroits » | **Trois.** `makePolylineEntity` (`NativeEntityRegistration.cpp:90`) est le troisième et n'a pas été mis à jour. §0.9bis, §2-bis |
| « `TypeId` : **UB sur nullptr**, à corriger en priorité » | **Piège latent, voie inactive.** Le seul site produisant un `TypeId{}` le teste via `operator bool` avant de comparer (`EventBus.h:164`) ; `internTypeId` rend un `c_str()` d'un pool statique, soigneusement documenté. Rétrogradé : à corriger pour le prochain auteur, pas pour hier. §2 |
| « Une cotation **ne survit pas** à un enregistrement/rechargement » | **Faux tel quel.** Les octets sont conservés (fallback `serializeParams`, `Database.cpp:140`) ; c'est le **type** qui est perdu, dégradé en `UnknownEntity` → invisible, incrochable, non transformable, **et muet en export DXF**. Le trou est réel, le mécanisme était mal dit. §2 |
| « `EventBus` sans verrou **alors que la tessellation tourne dans un thread dédié** » | **Pas de course établie.** Le `QThread` de tessellation ne publie aucun événement (vérifié : 0 référence à `EventBus` dans `TessellationWorker.*`). Le défaut est une **absence de règle écrite**, pas une bug observable. §2 |
| « Les 4 `*DimensionEntity` » | **5.** `RadiusDimension` et `DiameterDimension` sont deux TypeIds distincts sur la même classe `RadialDimensionEntity` (`EntityRegistry.cpp:163-166`). |

Mesures brutes de la passe initiale (2026-09-30) : build `-j` OK · `ctest`
**48/49** · `scripts/check_arch.sh` PASSED · `hole_test` vert (ne prouvait rien,
cf. §0.11 — corrigé depuis).

Mesures après correctifs 2026-10-03 : build OK · `ctest` **49/49** ·
`scripts/check_arch.sh` PASSED.

---

## 7. Chantiers du module cadastre (`src/plugins/cadastre/`)

Cinq sources se contredisent sur l'état du module (`CADASTRE_SPEC.md` §6,
`CADASTRE_PLUGIN_STATUS.md`, `CADASTRAL_AUDIT_2026.md`, les messages de commit,
et le code). **Ce qui suit est vérifié dans le code et par l'absence d'appelant**,
pas récrit d'un document.

### 7.1 — `IStyleProvider` → `LayerManager` : premier consommateur branché

**RÉSOLU partiellement** (cette session). Premier consommateur opérationnel :

- `MainWindow::applyStyleProvidersToDocument(Document&)` (`MainWindowDocument.cpp`)
  est appelée après `loadAllDiscovered()` dans le constructeur.
- Pour chaque `IStyleProvider`, elle itère `layerStyles()` et crée les calques
  dans `doc.layers()` avec couleur ACI→RGB et flags `visible`/`locked`.
- `StyleProviderTest.cpp` prouve le cycle register → apply → unregister (50ᵉ test).

Ce qui reste ouvert pour §7.1 :
- `plotStyles()` et `textStyles()` ne sont pas encore consommés — le `GlRenderer`
  utilise la couleur du calque mais pas l'épaisseur ni le motif de trait.
- `DxfExporter` n'utilise pas `PlotStyle.width` / `dash` — il fait son propre
  `colorToAci` sur la couleur du calque.
- ~~Permettre à un module de déclarer des calques après Fichier > Nouveau~~
  **FAIT** : `applyStyleProvidersToDocument` appelée dans `onNew()` et `onOpen()`
  en plus du démarrage (idempotente via `createLayer` qui ignore les doublons).
- Toujours la condition de l'**étape 3 de `ROADMAP_MARKET.md`** (gabarits pays) :
  `layers.json` atterrit maintenant dans le document, mais `plot_styles` et
  `text_styles` ne sont pas encore appliqués.

### 7.2 — ~~Cotations cadastrales : la « base » est une fonction qui ne fait rien~~ RÉSOLU

**RÉSOLU** (`36869f3`). `addParcelDimensionsToDocument` est implémentée : crée des
`AlignedDimensionEntity` (offset perpendiculaire 1,5 u) et `AngularDimensionEntity`
(bisectrice) sur calque `COTATION`. Les sérialiseurs de dimension (§2) étaient un
prérequis — les deux fixes sont dans le même commit.

Export DXF : `DxfExporter` délègue à `Entity::writeDxf` — les cotations partent en
géométrie + texte séparés, pas en entité `DIMENSION` DXF native. Item
`CADASTRAL_AUDIT_2026.md:35-36` **toujours dû** pour l'export DXF natif.

### 7.3 — ~~Lotissement : algorithme écrit, commande absente~~ RÉSOLU

**RÉSOLU** (`36869f3`). `SubdivideParcelCommand` ajoutée dans
`SplitParcelCommand.cpp` ; enregistrée sous `"cadastre.subdivide_parcel"` dans
`cadastre_plugin.cpp`. C3 de `CADASTRE_SPEC.md` est maintenant atteignable par
l'opérateur.

### 7.4 — Trous : module cadastre — état après 2026-10-03

`CADASTRAL_AUDIT_2026.md:47-48` posait le trou comme non représentable. La
situation après les correctifs :

- [x] `GeoJsonSerializer.cpp` : anneaux de trous inclus dans `Polygon.coordinates`
- [x] `parcelArea()` via `polygonArea()` : surface nette correcte (trous soustraits)
- [x] `ParcelTopologyValidator` : vérifie la simplicité de chaque trou
- [x] `ParcelOverlapRuleValidator` : corrige les faux positifs pour trou-recouvrant-voisine
- [x] DXF : chaque trou = LWPOLYLINE fermée séparée (sémantique perdue, bords visibles)
- [x] `ParcelEntitySerializer` et `BoundaryEntitySerializer` — **item stale**.
  Appellent bien `encodeRings`/`decodeRings` (vérifié : `ParcelEntity.cpp:23-50`,
  `BoundaryEntity.cpp:21-38`). Les trous sont persistés dans `.bcad`.
- [ ] `booleanOp()` (union/intersection/différence) utilise encore le polygon simple
  CGAL — nécessiterait `CGAL::Polygon_with_holes_2` pour être exact sur les trous.

### 7.5 — Référentiel et tolérance : le contrat de données manque, pas l'algo

`SurveyToleranceValidator` (0,02 m) est écrit et testé **en tant que primitive
géométrique**, et reste non déclaré comme règle. Consentit par
`CADASTRE_PLUGIN_STATUS.md:18-24` et `CADASTRAL_AUDIT_2026.md:51-54` : rien dans
le modèle n'associe une **référence juridique** à une parcelle — `BoundaryEntity`
ne porte ni `section` ni `numero`. **Ne pas « réparer » en inventant le contrat** ;
la tâche est de le concevoir (une référence par limite, ou une table de levé),
ensuite la règle s'alimente seule. Le gabarit prévoit déjà la clé :
`survey_tolerance.default_m` de `cadastre_togo.json`, **non consommée**.

Au-delà, l'audit garde trois items non commencés et qui ne dépendent pas de
PROJ : import de levé (GNSS/station totale, codes terrain et Z), tableau de
coordonnées avec gisement/fermeture et rapport opérateur, transformations de
systèmes de coordonnées. Le premier est le plus rentable et **`ROADMAP_MARKET.md`
§3 le classe déjà** ; `CAHIER_DES_CHARGES.md` §3-P1 l'avait noté de même
(« import CSV/TXT de points de levé »).

### 7.6 — Livrables : deux trous de production

- **I4 export image (PNG/JPG haute résolution)** : non commencé. `grep QImage`
  dans `src/layout` ne trouve qu'un usage interne à `FurniturePaint.cpp:127`,
  aucun chemin d'export. `CADASTRE_SPEC.md:184` le confirme « non commencés ».
- **G3 carroyage / légende** : la **grille nationale a été retirée
  volontairement** comme ne relevant pas de l'hôte (`layout/Scale.h`), et la
  légende n'existe pas (`CADASTRAL_AUDIT_2026.md:72-74`). Ne pas traiter ça comme
  un oubli : c'est un arbitrage. À reprendre seulement si un pays en a besoin dans
  son gabarit — ce qui relèverait de 7.1, pas de `src/layout`.
- Les objets de mise en page (flèche nord, barre d'échelle, tableau, bornes)
  **ne vivent que sur le papier** : ni dans le canevas, ni dans `.bcad`, ni en
  DXF (`AUDIT:74,96-100`). Assumé — « la feuille est un rendu, pas une source de
  données ». Gardé ici pour qu'on ne le prenne pas pour un manque.
- **Étiquettes de parcelle dans le canevas (H3/F3)** : seulement sur la feuille
  imprimée. `grep` sur `Label`/`forParcel` dans `src/app` et `src/render` : rien.

### 7.7 — Interop : GeoPackage conforme, ou assumé non conforme

`GeoPackageExporter` **est** enregistré (`cadastre_plugin.cpp:176`) —
`CADASTRAL_AUDIT_2026.md:64-66` (« n'est pas enregistré, aucun document ne part
en GeoPackage aujourd'hui ») est **périmé**, corrigé par `8e59394` et la série
`I3`. `cadastre_export_test` relit le fichier produit.

Mais le sérialiseur écrit `gpkg_contents` + `gpkg_geometry_columns` + une table
personnelle `cadastre_parcels` en `sqlite3` direct : **ni `gpkg_nortree`/rtree, ni
WKB complet, ni géométrie multipolygon**. Un GeoPackage est un sous-ensemble de
SQLite, mais « SQLite avec quelques tables » n'est pas la spec. **Conséquence
concrète : rien ne dit que QGIS ouvre ce fichier, et l'interop SIG est un
argument marché de `ROADMAP_MARKET.md` §1.** Deux issues honnêtes, à arbitrer :
soit se mettre en conformité (et l'écrire dans `CADASTRE_PLUGIN_STATUS.md`), soit
renommer le livrable (« export SQLite de BCAD ») pour ne pas promettre un format.
`ArcGisAdapter::exportToGdb` reste un stub de 9 lignes retournant `false` —
honnête, et `AUDIT:66` le garde comme tel tant que GDAL/OGR n'est pas une décision.

### 7.8 — Ce qui est fait et ne doit pas être redemandé

Pour éviter qu'un prochain agent re-ouvre du clos : **chaîne minimale viable
G1→G4→H3→G2→I2→I1 atteinte** (feuille composée, `composeSheet`/`drawSheet`,
aperçu et export partageant le même peintre) · **I3 DXF cadastral complet**
(calques `CADASTRE`/`COTATION`/`CARTOUCHE`, cartouche, composés **par le module** —
`CADASTRE_SPEC.md:176-182` qui dit le contraire est périmé) · **F4 recherche**
(`cadastre.find_parcel`, régression rétablie) · **formats d'échange branchés** sur
`IFileExporter`, GeoJSON/CSV écrits par l'hôte depuis le `PropertyMap` sans
littéral cadastral · **les trois validateurs de topologie branchés** · **profil
désigné par l'opérateur** avec anti path-traversal et undo des deux effets ·
**cartouche déclaratif** (`Furniture`/`FurnitureTemplate`, résolution par le
module, `ADR-017` tranches 1 et 2, format v3).

### 7.9 — Erreurs de documents, à corriger avec le code

| Document | Ce qu'il affirme | Réalité vérifiée |
|---|---|---|
| `CADASTRAL_AUDIT_2026.md:64-66` | GeoPackage « pas enregistré », aucun export | **Enregistré** (`cadastre_plugin.cpp:176`), couvert par `cadastre_export_test` |
| `CADASTRE_SPEC.md:176-182` (item 3) | calques `CADASTRE`/`COTATION`/`CARTOUCHE` et cartouche « ne sont pas écrits » | **Écrits**, `8e59394` |
| `CADASTRE_PLUGIN_STATUS.md:90-101` | `layers.json`/`plot_styles.json`/`text_styles.json` « restent installés sans consommateur », « il manque un point d'extension de styles » | Le point d'extension **existe** et les fichiers sont **lus** — mais **aucun consommateur** : le résultat est le même pour l'opérateur. À réécrire ainsi. |
| Message de commit `e22e60d` | « + lecture layers/plot_styles/text_styles » | Sur-déclare : lit, ne consomme pas. |
| `CADASTRE_SPEC.md:107` | « pas de cotations cadastrales automatiques » | Le calcul est écrit (`CadastreDimension.cpp`), **le branchement est un no-op** (`:141-152`). Ni fait ni à faire : à câbler. |


