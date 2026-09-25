# État du plugin cadastral

## Fonctionnalités opérationnelles

- Entités `ParcelEntity`, `BoundaryEntity`, `SurveyMarkEntity` et
  `EasementEntity`.
- Enregistrement dynamique des entités, commandes et serializers via
  `PluginRegistry`.
- Retrait des extensions avant `dlclose()`.
- **Vérifications rendues exécutables par le point d'extension de validation**
  (`plugin::IValidator`, action « Vérifier le document » du panneau Contrôle,
  résultats dans le dock « Vérifications ») :
  - `cadastre.topologie` — au moins 3 sommets, polygone simple, aire non nulle ;
  - `cadastre.recouvrement` — deux parcelles ne partagent pas d'emprise ;
  - `cadastre.identification` — section/numéro conformes au motif, non vides
    (avertissement) et portés par une seule parcelle.
  Ces trois règles étaient **compilées sans aucun appelant** avant ce branchement.
- `SurveyToleranceValidator` (écart entre emprise mesurée et emprise juridique)
  n'est **pas** une règle déclarée : le module n'a de quoi associer une reference
  juridique a aucune parcelle (`BoundaryEntity` ne porte ni section ni numero), et
  l'eriger en `IValidator` voudrait dire inventer le contrat de donnees que l'on
  est cense verifier. La classe est couverte par
  `tests/unit/plugins/cadastre/CadastreValidatorsTest.cpp` en tant que primitive
  geometrique, et restera inutilisee jusqu'a une couche de reference.
- Les regles ci-dessus ne lisent **aucun** template JSON : les quatre fichiers de
  `templates/` sont installes mais jamais ouverts, et le motif de section comme la
  tolerance de 0,02 sont ecrits en dur dans les validateurs (cf. la section
  « Restent a faire » de `CADASTRAL_AUDIT_2026.md`).
- Commandes de création, séparation, fusion et modification de limite avec
  undo/redo.
- Génération de plan cadastral PDF A3 paysage via
  `cadastre.generate_plan_sheet`. La feuille est **composée** : `layout/CadastreSheet.cpp`
  convertit les propriétés `cadastre.*` des parcelles en étiquettes au centroïde,
  bornes (sommets dédupliqués) et tableau des parcelles, que `layout::drawSheet`
  imprime avec le cartouche, la flèche nord et la barre d'échelle. `src/layout`
  ne connaît aucun de ces noms de propriété (ADR-016) : il dessine ce qu'on lui
  fournit. Couvert par `cadastre_sheet_test` et `cadastre_plan_sheet_test`.
- Actions du menu et du ruban `Cadastre` câblées au canevas : création,
  scission médiane, fusion de deux parcelles et modification d'un sommet de
  parcelle sélectionnée.
- Les actions utilisent le `QUndoStack` de l'application ; les sélections
  invalides sont refusées avec un message visible dans la barre d'état.
- Chargement automatique : l'hôte énumère ses répertoires de modules
  (`$BCAD_PLUGIN_PATH`, `lib/bcad/plugins` de l'installation, arbre de build,
  `$XDG_DATA_HOME/bcad/plugins`) et ne nomme aucun plugin (ADR-016). La découverte
  est testée par `discovery_test` et prouvée de bout en bout par
  `cadastre_external_test`, qui charge le module, exécute ses validateurs depuis
  l'hôte, puis le décharge.
- Templates JSON du profil Togo, des calques et des styles : **installés mais
  jamais lus**. Ils ne pilotent ni le motif de section ni la tolérance de lever,
  tous deux écrits en dur dans les validateurs.

## Formats

**Le module exporte un format, et l'application l'affiche.** `GeoPackageSerializer`
est atteint par l'action `GeoPackage cadastral` du menu `Fichier → Exporter`, via
le sixième point d'extension (`plugin::IFileExporter`, déclaré dans
`cadastre_plugin.cpp:registerFileExporter`) — couvert par `cadastre_export_test`,
qui relit le fichier produit et vérifie que les attributs cadastraux y sont.

`GeoJsonSerializer`, `CsvCoordinateExporter` et `ArcGisAdapter` ne sont appelés
que par `cadastre_io_test` : ce n'est **pas** un manque de branchement, le noyau
exporte désormais ces deux formats pour **toutes** les entités (voir ci-dessous)
et un second écrivain GeoJSON dans le module serait un doublon.

Les entrées GeoJSON et CSV du menu `Fichier` ne sont plus écrites à la main dans
`MainWindow` : `io::toGeoJson` et `io::toCoordinateCsv` (`src/io/Exchange.cpp`)
construisent un `FeatureCollection` à partir de `PropertyMap::listNames()`, donc
**section, numéro et contenance sortent sans qu'aucun nom cadastral n'apparaisse
dans l'hôte** (ADR-016). Une géométrie inconnue est tessellée plutôt qu'oubliée.
Couvert par `exchange_test`.

Le format `.bcad` reste le format natif SQLite de BCAD, et c'est lui qui
persiste les attributs cadastraux (serializer enregistré, round-trip couvert par
`roundtrip_test`).

ArcGIS est un stub de 9 lignes qui retourne `false` : comportement honnête pour
une opération non implémentée, rien à brancher tant que GDAL/OGR n'est pas une
décision.

## Interface utilisateur

Le shell Qt conserve un canevas central, les docks `Calques`, `Propriétés` et
`Vérifications`, une ligne de commande, une barre d'état et l'undo/redo. Le
chargement des modules est automatique et par découverte de répertoires :
l'hôte ne nomme aucun plugin et n'affiche aucun nom de domaine (ADR-016), ce que
vérifient `check_arch.sh` et `discovery_test`.

Les conventions retenues pour la suite sont celles communes à FreeCAD,
LibreCAD et QCAD : menus français Dessin/Modifier/Calque, barres d'outils dockables,
commandes annulables avec `Esc`, snaps visibles et aperçu d'impression.
La cotation linéaire générale est disponible dans l'onglet `Annoter` et le
menu `Cotation`. Elle crée les deux lignes d'attache et la ligne de cote sur
le calque `Dimensions`, avec une entrée unique dans l'historique undo/redo.
Le parcours dessin → sauvegarde → rechargement → impression PDF est couvert
par un test d'intégration, y compris les lignes ouvertes et les polygones.
Le contrat complet est documenté dans `docs/UI_CONVENTIONS.md`.

## Vérification

Le build, `scripts/check_arch.sh`, `sdk_external_test` et
`cadastre_external_test` doivent rester passants après chaque lot.

Ce que chacun couvre côté validation et découverte :

| Test | Prouve |
|------|--------|
| `cadastre_validators_test` | les trois règles rendent les diagnostics attendus (huit, recouvrement, identification) et un document propre n'en rend aucun |
| `validator_test` | cycle de vie `IValidator` : enregistrement, doublon refusé, **destruction avant `dlclose`** |
| `discovery_test` | sélection dans les répertoires, priorité de `$BCAD_PLUGIN_PATH`, déduplication, **ordre déterministe** |
| `cadastre_external_test` | un plugin construit hors arbre (`find_package(BCAD)`) déclare un validateur que l'hôte exécute, puis est déchargé ; la sortie du processus ne plante pas |
| `check_arch.sh` §10bis | `src/app/` ne nomme aucun module et n'écrit aucun littéral métier, y compris dans les `tr()` |

Ce que chacun couvre côté mise en page :

| Test | Prouve |
|------|--------|
| `composition_test` | découpage de la feuille en millimètres : le plan ne descend pas sur le cartouche, la colonne du tableau est retranchée de sa place, flèche et barre d'échelle restent sur le plan, barre d'échelle à distances rondes |
| `sheet_render_test` | le peintre est bien au millimètre de la **zone imprimable** : l'encre mesure la feuille, et la résolution du périphérique n'est pas supposée (elle est vérifiée sur `QPrinter`) |
| `cadastre_sheet_test` | les propriétés cadastrales deviennent étiquettes, bornes dédupliquées et tableau, et une polygone sans attribut n'est pas une parcelle |
| `cadastre_plan_sheet_test` | la commande `cadastre.generate_plan_sheet` produit un vrai PDF, l'annule, et ne laisse aucun fichier sur un document vide |
