# TODO — BCAD

> File de travail. Les documents faisant autorité restent `CAHIER_DES_CHARGES.md`
> (fonctionnalités), `docs/ROADMAP_MARKET.md` (ordre marché),
> `docs/CONSOLIDATION_STATUS.md` et `docs/RUST_STATUS.md` (avancement consigné).
> Ce fichier n'arbitre rien : il rassemble ce qui est **ouvert**, avec sa source.
> Dernière passe : 2026-09-30, **sur code compilé et testé**, et non sur lecture.
>
> ## État mesuré, pas déduit
>
> ```
> cmake --build build -j          → OK
> ctest --test-dir build          → 48/49 verts
> ctest -R schema                 → ROUGE
>   BcadSchemaTest.cpp:312: testLegacyV1LoadsWithoutPlugin():
>   Assertion `enriched->vertices().size() == 4' failed
> ```
>
> **La branche est cassée.** Le lot des trous n'est pas « à compléter » : il fait
> régresser un test de contrat qui protégeait les anciens fichiers. Voir §0.9.

---

## 0. Bloquant — chantier en cours dans l'arbre de travail

Les trous (anneaux intérieurs) des polygones sont **engagés mais non commités** :
`include/bcad/geometry/Polyline.h`, `src/serialization/NativeSerializers.cpp`,
`tests/roundtrip_test.cpp`, `tests/CMakeLists.txt`,
`tests/unit/geometry/HoleTest.cpp` (nouveau).

Le modèle de données et la sérialisation sont écrits. **Rien ne les consomme.**

- [ ] **0.1 `applyTransform` ignore les trous** (`Polyline.h:32-34`) — déplacer,
      tourner, changer l'échelle ou miroiter une parcelle à trou laisse le trou
      à sa position d'origine. Corruption silencieuse, la plus grave du lot.
- [ ] **0.2 `boundingBox` ignore les trous** (`Polyline.h:26-30`) — un trou ne
      peut pas agrandir la boîte, donc l'index spatial reste correct, MAIS la
      présélection et l'élagage du viewport ignorent la géométrie du trou. À
      trancher : la boîte englobante d'un trou est déjà dans celle de l'anneau
      extérieur tant que le trou est intérieur ; le problème n'apparaît que si
      un trou mal formé dépasse. Décider si un trou invalide est **rejeté** ou
      **signalé**, pas ignoré.
- [ ] **0.3 `tessellate` ignore les trous** (`Polyline.h:40-44`) — le rendu
      affiche un polygone plein. Le trou est invisible à l'écran : l'opérateur
      ne peut pas savoir qu'il existe.
- [ ] **0.4 `distanceTo` ignore les trous** (`Polyline.h:46-58`) — impossible de
      sélectionner ou accrocher un bord de trou.
- [ ] **0.5 DXF aller-retour perd les trous** — ni `io/DxfWriter.cpp`, ni
      `io/DxfReader.cpp`, ni `src/plugins/cadastre/io/DxfExporter.cpp` ne
      mentionnent les trous. Contredit le risque n° 1 de `ROADMAP_MARKET.md` §5
      (« un aller-retour qui perd disqualifie l'outil »).
- [ ] **0.6 GeoPackage / GeoJSON / CSV perdent les trous** —
      `src/plugins/cadastre/io/GeoPackageSerializer.cpp`,
      `GeoJsonSerializer.cpp` : un anneau extérieur seul est écrit. Un export
      destiné à QGIS est donc **faux** (la parcelle est trop grande de la
      surface du trou).
- [ ] **0.7 Opérations booléennes et `ParcelOps` ignorent les trous** —
      `geometry/BooleanOps.cpp`, `src/plugins/cadastre/ParcelOps.cpp` : une
      union/intersection/différence et une `parcelArea()` traitent l'entité comme
      un anneau simple. **`contenance` (la surface légale, portée par
      `ParcelEntity`) serait fausse** sur une parcelle à trou. C'est la valeur
      qui va dans le cartouche et la nomenclature.
- [ ] **0.8 Les validateurs cadastraux ignorent les trous** —
      `ParcelTopologyValidator` (anneau simple, auto-intersection) et
      `ParcelOverlapRuleValidator` ne regardent pas les anneaux intérieurs : une
      parcelle dont le trou recouvre une parcelle voisine passe la validation.
- [ ] **0.9 Perte totale de géométrie à la relecture des anciens `.bcad` — MESURÉE, test rouge.**
      Le CSV passe de `closed,x0,y0,…` à `closed,N,x0,y0,…,H,…`. Une ligne de
      fixture v1 porte `'1,0,0,20,0,20,10,0,10'` (polygone fermé de 4 sommets).
      Le nouveau parseur lit `v[1] = 0` comme `vertexCount`, la boucle ne s'exécute
      pas, puis lit `v[2] = 0` comme `holeCount`. **Résultat : une polyligne à 0
      sommet.** Ce n'est pas un décalage ni un compteur faux, c'est le polygone
      **entier qui disparaît**, et `Database::load` retourne `true` — l'hôte ne
      signale rien. Lignes touchées dans les fixtures : `legacy_v1.bcad` ids 1, 2, 5
      et `reference_v2.bcad` id 2.
      *Fixe : bump de version de format, ou parseur qui distingue les deux.
      **Tant que `bcad_schema_test` est rouge, rien de ce lot est committable.***
- [ ] **0.9bis Le troisième parseur du même format n'a pas été touché** —
      `makePolylineEntity` (`src/registry/NativeEntityRegistration.cpp:90-105`)
      exige `(parts.size() - 1) % 2 == 0` : sur la nouvelle sortie
      (`1,4,0,0,20,0,20,10,0,10` = 10 champs) il **rejette la donnée en retournant
      `nullptr`**. Voir §2-bis ci-dessous : cette voie est morte aujourd'hui, ce
      qui est la seule raison pour laquelle elle n'a pas cassé davantage.
- [ ] **0.10 Nettoyage avant commit** — retirer les `std::cerr << "DEBUG: …"`
      ajoutés dans `tests/roundtrip_test.cpp` (dont un dump de `serialized` pour
      toutes les entités) ; supprimer le `if (v.size() < 2) return nullptr;`
      écrit deux fois dans `NativeSerializers.cpp` ; remplacer
      `std::move(const_cast<std::vector<Point2>&>(hole))` (`NativeSerializers.cpp`,
      boucle finale) par une itération non-const — UB formel, et inutile.
- [ ] **0.11 `HoleTest.cpp` n'exerce pas le vrai désérialiseur** — il
      ré-implémente le parseur en local (`parsePolylineWithHoles`) et ne peut
      donc pas attraper une régression de `SerializerRegistry`. Le convertir en
      round-trip via le registre.
- [ ] **0.12 Le format CSV des polylignes existe à TROIS endroits** —
      `Polyline.h:71-84` (`serializeParams`, l'écrivain),
      `NativeSerializers.cpp:185-197` (`PolylineSerializer`, la voie **réellement
      utilisée** par `.bcad`, dans les deux sens) et
      `NativeEntityRegistration.cpp:90-105` (`makePolylineEntity`, la troisième —
      voir §0.9bis et §2-bis). Ce lot en a modifié deux sur trois ; c'est la cause
      directe de 0.9. Une seule implémentation appelée par les autres.

**À décider avant d'aller plus loin** : la fonction est-elle voulue comme un
simple portage de données (alors 0.1 est obligatoire et 0.3→0.8 sont reportés),
ou comme une géométrie réelle (alors 0.1 → 0.8 sont une seule et même tâche,
et la surface doit passer par les booléens CGAL qui savent déjà gérer les trous) ?

**Ce que l'exécution change à ce arbitrage** : 0.9 n'est plus une question de
complétude, c'est une **régression d'un test de contrat existant**. Quel que soit
le choix, il faut (a) réparer 0.9 et 0.9bis, (b) repasser `bcad_schema_test` au
vert, (c) faire le ménage 0.10. Les points 0.1→0.8 peuvent être reportés avec une
décision écrite, pas 0.9.

Précision mesurée : 0.1→0.8 ne sont **pas encore observables** — aucune voie de
l'application ne crée de trou (`grep` sur `addHoles`/`holes()` ne donne que
l'en-tête, le test et la sérialisation). Ils sont réels dès qu'un appelant
existera. 0.9 en revanche casse **les fichiers déjà sur le disque**, maintenant.

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
      - bump de `PLUGIN_API_VERSION` (actuellement 10, `plugin/PluginRegistry.h:47`) ;
      - réécriture du module cadastre sur la nouvelle API générique ;
      - garde `check_arch.sh` étendue à `src/layout/`.
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

- [ ] **`TypeId` : pied de mine, pas de bug actif** (corrigé après exécution).
      Tous les opérateurs de `geometry/TypeId.h:19-48` appellent `std::strcmp`
      sans test de nullité alors que `value` vaut `nullptr` par défaut, et
      `EntityRegistry::{contains,find,unregisterType}` (`EntityRegistry.cpp:108-117`)
      font `getMap().find(typeId.value)` = construction d'un `std::string` du
      pointeur nul. **Vérifié : aucune de ces voies n'est atteignable avec un
      `TypeId{}` aujourd'hui** — le seul site qui en produit un
      (`EventBus.h:163-165`) teste `!eventTypeId` via `explicit operator bool()`
      **avant** de comparer. Et `internTypeId` (`Database.cpp:252-256`) rend un
      `c_str()` issu d'un `static std::set<std::string>` : terminaison NUL et
      durées de vie garanties, code soigné et documenté.
      Donc : **à corriger pour ne pas tendre le piège au prochain rédacteur**
      (un `string_view` avec comparaisons explicites), et à traiter comme un
      changement d'en-tête public → mesurer l'impact ABI, sinon attendre le bump
      déjà prévu au point 1 (ADR-017).
- [ ] **Les entités de cotation sont dégradées en `UnknownEntity` à chaque
      ouverture** (corrigé après exécution — ce n'est pas « non persistées »).
      Sauvegarde : `serializeParamsOf` (`Database.cpp:135-141`) ne trouve pas de
      serializer, retombe sur `e.serializeParams()` — les dimensions l'implémentent,
      **les octets sont donc bien écrits**. Relecture : `Database.cpp:267-273` ne
      trouve toujours pas de serializer et fabrique un `UnknownEntity`.
      L'entité survit, mais devient : **invisible** (`tessellate` rend `{}`),
      **incrochable** (`distanceTo` rend `infinity()`), **hors des bornes**
      (`boundingBox` rend une emprise vide), **non transformable**
      (`applyTransform` ne fait rien) et **perdue en export DXF** (`writeDxf`
      n'écrit rien — `UnknownEntity.h:64-67`).
      Or le `UnknownEntity.h:12-19` est conçu pour *un module absent*. Un type du
      **core** ne devrait jamais transiter par là. `initializeNativeSerializers()`
      n'enregistre que 6 types (`NativeSerializers.cpp:298-303`) alors que
      `registerNativeTypes()` en déclare 11 (`EntityRegistry.cpp:145-166`, dont
      `LinearDimension`, `AlignedDimension`, `AngularDimension`, `RadiusDimension`,
      `DiameterDimension`). **Les 5 dimensions sont enregistrées comme types et
      oubliées comme serializers.** C'est le trou, et un aller-retour
      enregistrement/rechargement sur un dessin coté le prouve.
- [ ] **Cotations cadastrales non persistantes** — `TODO: Créer les entités de
      cotation persistantes` (`src/plugins/cadastre/layout/CadastreDimension.cpp:147`).
      Même sujet que le point précédent, côté module.
- [ ] **Deux voies d'enregistrement concurrentes, et une couche de fabriques morte.**
      Découvert en traçant quel parseur s'applique réellement :
      - `registerNativeEntities()` (`NativeEntityRegistration.cpp:123`) enregistre
        **6 types avec `paramsFactory`** et est déclenchée **toute seule par un
        initialisateur statique** (`:138-141`) au chargement de la bibliothèque ;
      - `EntityRegistry::registerNativeTypes()` (`EntityRegistry.cpp:142`) enregistre
        **11 types sans `paramsFactory`**, et n'a **qu'un seul appelant :
        `tests/sdk_abi_test.cpp:57`**.
      `registerType` fait `getMap().emplace(...)` (`:56`) : **un doublon est ignoré
      en silence**, donc l'ordre d'exécution décide quelle table gagne — sans
      diagnostic et sans test qui verrouille cet ordre.
      Et surtout : **la surcharge `EntityRegistry::create(TypeId, string_view params)`
      (`EntityRegistry.h:78`) n'a aucun appelant** — ni dans `src/`, ni dans
      `tests/`, ni dans `examples/`. Tout l'étage `paramsFactory` (les six
      `makeXxxEntity` de `NativeEntityRegistration.cpp:40-105`, ~65 lignes de
      parseurs CSV écrits à la main) est **mort, mais exporté dans le SDK** et
      documenté comme la voie d'extension (`EXTENDING_BCAD.md`, ADR-003).
      À trancher : le supprimer, ou le rendre réellement utilisé par `Database`
      (auquel cas 0.9bis devient un quatrième échec). Ne pas le laisser dans le
      limbo : c'est lui qui a été oublié par le lot des trous, précisément parce
      qu'il a l'air d'être l'API officielle.
- [ ] **`EventBus` sans verrou — contrainte à écrire, pas un bug démontré.**
      Vérifié : `include/bcad/events/EventBus.h` ne mentionne `mutex` **nulle
      part** (0 occurrence), alors que c'est un singleton global. Vérifié aussi :
      le thread de tessellation (`TessellationWorker.h:11`, `QThread` dans
      `Viewport.cpp:53`) **ne publie aucun événement** — il communique par
      signaux/slots Qt. **Aucune course n'est donc établie**, et je ne la
      présente pas comme telle. Le risque est pour le prochain auteur qui
      publiera depuis un thread. *À faire, dans l'ordre de coût :* (a) consigner
      la règle « `EventBus` n'est pas thread-safe, thread UI uniquement » dans
      l'en-tête et dans `docs/EVENT_SYSTEM.md` ; (b) le coût réel si un jour il
      faut publier ailleurs — un `std::mutex` sur la table d'abonnés.
      Seconde chose, mesurée à la lecture cette fois : `EventFilter::matches`
      (`:189-242`) enchaîne **une `dynamic_cast` par type d'événement** pour
      extraire document/type/id/calque, soit ~15 RTTI par événement filtré et par
      abonné. Fonctionnel, mais c'est le chemin chaud de chaque `publish`.
- [ ] **`Document::extents()` est un scan O(n)** (`src/core/Document.cpp:117-122`)
      alors que le quadtree est à côté. Appel probable à chaque zoom sur
      l'ensemble (`F`).
- [ ] **`CommandStack`** (`commands/Command.h:84-157`) — historique non borné,
      `doc_` en pointeur nu, et le `shared_mutex` du `Document` n'est pas utilisé
      par les commandes.
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
- [ ] **`CAHIER_DES_CHARGES.md` §3 est périmé** — il coche « à faire » du texte
      simple (`TEXT`/`DTEXT`), des cotes linéaires/alignées et de l'export PDF,
      alors que les quatre `*DimensionEntity`, `layout/PdfExport` et les cotations
      cadastrales existent. README déclare ce document **source de vérité** : un
      écart entre les deux est un défaut d'audit, cf. la passe déjà faite en
      `de4cc78`.

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

Mesures brutes de la passe : build `-j` OK · `ctest` **48/49** ·
`scripts/check_arch.sh` **PASSED** · `hole_test` **vert** (il ne prouve rien,
cf. §0.11).

---

## 7. Chantiers du module cadastre (`src/plugins/cadastre/`)

Cinq sources se contredisent sur l'état du module (`CADASTRE_SPEC.md` §6,
`CADASTRE_PLUGIN_STATUS.md`, `CADASTRAL_AUDIT_2026.md`, les messages de commit,
et le code). **Ce qui suit est vérifié dans le code et par l'absence d'appelant**,
pas récrit d'un document.

### 7.1 — `IStyleProvider` : le point d'extension est écrit, personne ne le lit

Le plus ouvert des chantiers, et le plus trompeur.

- Le module enregistre un provider : `cadastre_plugin.cpp:178`
  (`registerStyleProvider(std::make_unique<CadastreStyleProvider>())`).
- `StyleProviderRegistry` est complet côté hôte : `instance()`,
  `registerProvider`, `unregisterProvider`, `clear`, `providers()`, `find(id)`
  (`plugin/StyleProvider.h:73-90`, implémenté `PluginManager.cpp:688-730`).
- `CadastreStyleProvider` lit bien `layers.json`, `plot_styles.json`,
  `text_styles.json` (commit `e22e60d`).
- **Aucun consommateur.** `grep` sur `providers()` et `find()` de ce registre ne
  remonte **que** `FileExporterRegistry` et `ValidatorRegistry`
  (`MainWindowPlugins.cpp:325`, `ProfilCommand.cpp:28`). Ni `src/app/`, ni
  `src/io/`, ni `src/layout/`, **aucun test** (`grep -l StyleProvider tests/` :
  rien). Les trois JSON sont donc lus et jetés.

État réel des documents : `CADASTRE_PLUGIN_STATUS.md:90-101` (« un seul des
quatre gabarits est lu, `layers.json` reste sans consommateur ») est **périmé
sur la forme** — il y a maintenant un lecteur — mais **vrai sur le fond** : lire
sans consommer ne change rien pour l'opérateur. Le commit `e22e60d` est titré
« lecture layers/plot_styles/text_styles », ce qui sur-déclare ce qui est livré.

*À faire, dans l'ordre :* (a) décider **qui** consomme — `LayerManager` à la
création d'un calque, `GlRenderer` pour le type de trait, `layout/FurniturePaint`
pour les polices, `DxfExporter` pour les épaisseurs ; (b) brancher **un**
consommateur et l'** testers ** (le point d'extension n'a pas de preuve, contrairement
aux six autres qui ont tous leur test) ; (c) permettre au module de **déclarer ses
calques** — aujourd'hui « les calques sont créés par l'hôte à la demande de
l'utilisateur » (`STATUS:93`), donc `layers.json` ne peut pas atterrir dans le
document. C'est la condition de l'**étape 3 de `ROADMAP_MARKET.md`** (gabarits
par pays), notée « meilleur rapport valeur/effort, quasi aucun C++ » : tant que
personne ne lit les styles, ajouter un pays ajoute des JSON que rien n'applique.

### 7.2 — Cotations cadastrales : la « base » est une fonction qui ne fait rien

`caa960f feat(cadastre): cotations cadastrales (H1-H5) - base`. Vérifié :

- Le calcul existe : `generateParcelDimensions` produit linéaire, angulaire et
  étiquette (`layout/CadastreDimension.cpp`), et
  `generateDocumentDimensions` parcourt le document.
- **Le branchement est vide** : `addParcelDimensionsToDocument`
  (`CadastreDimension.cpp:141-152`) est un no-op —
  `(void)document; (void)dims;` avec `// TODO: Créer les entités de cotation
  persistantes`. **Aucune cotation n'entre dans le document.**
- `generateParcelDimensions` retourne
  `ParcelDimensions{linear, angular, label, {}, {}}` : **deux des cinq champs
  restent remplis de vide** (H4 bornes, H5 — la ligne renvoie le travail fait
  ailleurs, cf. commentaire « déjà géré par `buildSheetFurniture` »).
- Ce trou est le **même** que §2 (les 5 types de dimension n'ont pas de
  serializer). Brancher `addParcelDimensionsToDocument` sans réparer §2 donnerait
  des cotations **dégradées en `UnknownEntity`** à la première sauvegarde :
  invisibles, incrochables, et muettes en export DXF. **§2 est donc un prérequis
  de 7.2, pas un point séparé.**
- Export DXF : `grep DIMENSION src/plugins/cadastre/io/DxfExporter.cpp` → **0
  occurrence**. L'exporteur délègue à `Entity::writeDxf` (`:233`), donc les
  cotations partent en géométrie + texte séparés. C'est l'avant-dernier item
  « Cotations et annotations » de `CADASTRAL_AUDIT_2026.md:35-36`, et il reste
  dû.

### 7.3 — Lotissement : algorithme écrit, commande absente

`ParcelOps::subdivideParcel` existe (`ParcelOps.h:15`, `ParcelOps.cpp:89`) et
`parcelops_test` le couvre. **Aucune commande ne l'enregistre** : les 7 commandes
du module sont `create_parcel`, `split_parcel`, `merge_parcels`,
`edit_parcel_boundary`, `generate_plan_sheet`, `find_parcel`, `set_profile`
(`cadastre_plugin.cpp:137-151`). **C3 de `CADASTRE_SPEC.md` n'est pas atteignable
par l'opérateur.** C'est exactement le défaut que `STATUS:17` relate pour les
validateurs (« ces trois règles étaient compilées sans aucun appelant ») — il se
reproduit ici. Coût faible : une `Command` + une `WorkbenchAction`.

### 7.4 — Trous : le module est hors du changement (§0, rappel)

`CADASTRAL_AUDIT_2026.md:47-48` pose le trou comme non représentable faute de
relation conteneur/enfant. Le lot en cours ajoute bien `holes_`, mais :

- `ParcelEntitySerializer::serialize` (`entities/ParcelEntity.cpp:18-33`) **écrit
  son propre CSV de polygone dans l'ancien format** et n'appelle pas
  `PolylineEntity::serializeParams()`. `BoundaryEntity.cpp:23` idem.
  → **la parcelle ne reçoit jamais de trou**, et la grammaire du polygone existe
  à **quatre** endroits dont deux hors du lot.
- `parcelArea()` / `ParcelOps` / les booléens ne soustraient pas les trous :
  **`cadastre.contenance`, la surface légale du cartouche et du tableau, serait
  fausse.**
- `ParcelTopologyValidator` et `ParcelOverlapRuleValidator` ne regardent que
  l'anneau extérieur : un trou recouvrant une parcelle voisine passe la validation.
- `GeoJsonSerializer.cpp:33-36` écrit un `Polygon` à un seul anneau. **Or dans la
  spec GeoJSON un anneau supplémentaire EST un trou** : l'export est donc
  silencieusement faux dès qu'un trou existe, et il serait quasi gratuit de le
  rendre correct. Le DXF ne permet pas le même raccourci (`LWPOLYLINE` n'a pas de
  trou, il faut `REGION` ou un `HATCH` à trous) — c'est un choix de conception à
  assumer, pas une ligne à ajouter.

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


