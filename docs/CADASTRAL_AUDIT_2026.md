# Audit fonctionnel cadastral BCAD — 24 septembre 2026

## Périmètre

Cet audit confronte la feuille de route fournie dans l’audit fonctionnel joint
au dépôt réel, sans considérer une fonction comme acquise uniquement parce
qu’elle est mentionnée dans un document.

## Ce qui existe et est vérifié

| Domaine | État réel |
|---|---|
| Core C++ indépendant de Qt et du renderer | Implémenté et contrôlé par `scripts/check_arch.sh` |
| Registries d’entités, commandes et sérialiseurs | Implémentés |
| Plugins dynamiques et SDK installable | Implémentés, avec preuves externes |
| Plugin cadastral | Parcelle, limite, borne/repère et servitude présents |
| Propriétés cadastrales | portées par `PropertyMap` (`cadastre.section`, `cadastre.numero`, …) et éditées par le panneau `Propriétés` **générique** de l'hôte : il n'existe aucun panneau spécialisé fourni par un module, et aucune API d'extension du panneau n'est nécessaire aujourd'hui |
| Création, scission, fusion et modification de parcelles | Commandes présentes et branchées à l’interface |
| Validations cadastrales de base | **Branchées et déclenchables** : trois règles déclarées par le module (`cadastre.topologie`, `cadastre.recouvrement`, `cadastre.identification`) sur le point d'extension `plugin::IValidator`, exécutées par l'action « Vérifier le document » et rendues dans le dock « Vérifications ». Couvertes par `cadastre_validators_test` et `cadastre_external_test` |
| Interface française | Menus, ruban, accès rapide et actions cadastrales présents |
| Formats actuels | `.bcad` (natif SQLite, attributs cadastraux inclus), DXF, GeoJSON, CSV des coordonnées et **GeoPackage cadastral**. Le menu `Fichier → Exporter` dresse la liste des exporteurs **enregistrés** (`plugin::IFileExporter`, sixième point d'extension) : les formats du noyau comme celui du module passent par le même registre, l'hôte ne nomme aucun format. GeoJSON et CSV sont écrits par `src/io/Exchange.cpp` à partir du `PropertyMap` — **section, numéro et contenance sortent sans littéral cadastral dans l'hôte**. Couvert par `exchange_test`, `file_exporter_test`, `cadastre_export_test`. Détail dans `CADASTRE_PLUGIN_STATUS.md` |
| Mise en page et cartouche | **Composée** : `layout::composeSheet` découpe la feuille (A4–A0, orientation, marges) en zone de plan, bandeau de cartouche et colonne de tableau, et `layout::drawSheet` rend le tout — plan, étiquettes, bornes, flèche nord, barre d'échelle, tableau parcellaire, cartouche, cadre. Le même peintre sert à l'export PDF et à l'aperçu d'impression. Le contenu métier (`cadastre.*` → étiquettes et tableau) est construit par le module, jamais par `src/layout` ni `src/app` (ADR-016). Couvert par `composition_test`, `sheet_render_test`, `cadastre_sheet_test`, `cadastre_plan_sheet_test` |
| Impression PDF vectorielle | Présente ; lignes ouvertes et surfaces sont distinguées |
| Cotations | Linéaire, alignée, angulaire, rayon et diamètre sont interactives |
| Textes de cotation | Désormais entités `bcad.Text`, sérialisées, visibles dans le canevas et exportées en PDF |
| Tests | 39 tests CTest, `scripts/check_arch.sh` (désormais étape de la CI) et les deux preuves externes (`sdk_external_test`, `cadastre_external_test`) passants |
| Découverte des modules | L'hôte énumère ses répertoires de plugins et ne nomme aucun module (ADR-016), contrôlé par `check_arch.sh` §10bis et `discovery_test` |

## Ce qui reste à faire

### Cotations et annotations

- Ajouter des flèches normalisées et des styles de cotation configurables.
- Persister la relation sémantique entre texte, lignes d’attache et mesure.
- Exporter les cotations natives en objets DXF `DIMENSION`, plutôt qu’en
  géométrie et texte séparés.
- Gérer le placement et la rotation des textes sur les segments obliques.

### Topologie cadastrale

Fait depuis l’audit : auto-intersections, chevauchements d’emprise et doublons
d’identifiants sont détectés et affichés (`cadastre.topologie`,
`cadastre.recouvrement`, `cadastre.identification`).

Reste à faire :

- Trous (polygones à contours multiples) : le modèle n’a pas de relation
  conteneur/enfant entre parcelles, donc un trou n’est pas représentable.
- Contrôle de compatibilité de limites communes et rattachement obligatoire aux
  bornes selon le référentiel choisi.
- Rapport de tolérance et d’incertitudes de levé : `SurveyToleranceValidator`
  compare deux emprises sommet à sommet mais **rien, dans le modèle, n’associe
  une référence juridique à une parcelle** (`BoundaryEntity` ne porte ni section
  ni numéro). La règle est en attente du contrat de données, pas de l’algorithme.

### Levé et référentiels

- Import GNSS/station totale avec codes terrain et Z.
- Transformations entre systèmes de coordonnées et contrôle de projection.
- Tableau de coordonnées, calculs de gisement/fermeture et rapport opérateur.

### Interopérabilité et production

- GeoPackage : le sérialiseur existe et son round-trip est testé, mais il
  n'est **pas enregistré** par le module — aucun document ne part en
  GeoPackage aujourd'hui. Shapefile, DWG et les services ArcGIS/WMS/WMTS
  n'existent pas.
- Composition de la feuille : `NorthArrow`, `Scale`, `ParcelTable`, `Borne` et
  `Label` sont **composés et imprimés** — `layout::composeSheet` réserve la place
  de chacun, `layout::drawSheet` les rend, et la commande
  `cadastre.generate_plan_sheet` les remplit depuis les propriétés cadastrales.
  Ce qui n'existe pas encore : la légende et la grille de carroyage (G3), et le
  fait que ces objets ne vivent que sur le papier — ni dans le canevas, ni dans
  `.bcad`.
- Templates administratifs configurables avec styles, épaisseurs, transparence,
  verrouillage et imprimabilité par calque.
- Fait depuis l'audit (C3) : les **motifs d'identification** ne sont plus écrits
  en dur dans le module, ils viennent de `templates/cadastre_togo.json` lu au
  chargement (`PluginRegistry::resolveDataFile`, répertoires annoncés par l'hôte,
  `$BCAD_PLUGIN_DATA` prioritaire). Un gabarit invalide ou d'une `schema_version`
  inconnue n'est pas appliqué. Reste non lu, et assumé comme tel : les styles de
  calque et de texte (il faudrait un point d'extension de styles que rien d'autre
  n'attend), `survey_tolerance.default_m` (aucune règle à alimenter tant
  qu'une référence juridique n'est pas associée à une parcelle) et `units.*`
  (aucun consommateur ; lire des unités sans conversion à faire serait inventer
  une règle métier).
- Signatures et archivage dans une GED.

## Écarts importants de l’audit fourni

L’audit joint décrit des entités comme `TextEntity`, `NorthArrowEntity`,
`ScaleBarEntity` et des formats comme DWG ou GeoPackage comme des objectifs.
`bcad.Text` est devenu une entité native (nécessaire à la persistance des
cotations). La flèche nord et la barre d’échelle existent comme **objets de
mise en page** (`layout::NorthArrow`, `layout::Scale`) avec leurs tests
unitaires, et sont désormais **composées et imprimées** sur la feuille. Elles ne
sont pas pour autant des entités du document : elles ne sont ni éditables dans le
canevas, ni persistées dans `.bcad`, ni exportées en DXF. Ce sont deux statuts
différents, et le premier n’est pas atteint — ce n’est pas un objectif, la
feuille étant un rendu, pas une source de données.

Régression tranchée depuis l'audit : la **recherche par section+numéro** (F4 de
`CADASTRE_SPEC.md`) existait dans le module monolithique (`cadastre::findByRef`,
testée) et avait été supprimée sans successeur lors de la migration en module
dynamique (`fcb2e24`). Elle est rétablie depuis : `cadastre.find_parcel` est une
commande du module, atteinte par une action du workbench, et l'hôte reste sans
littéral métier (la saisie passe par la stratégie générique `PromptText`).

## Validation attendue après cette évolution

```text
cmake --build build -j2
ctest --test-dir build --output-on-failure
scripts/check_arch.sh
git diff --check
```

Les avertissements de dépréciation liés à `EntityType` restent compatibles
avec l’ADR de rétrocompatibilité ; les nouveaux chemins utilisent `TypeId`.
