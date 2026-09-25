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
- Les règles ci-dessus **lisent leur gabarit** : `templates/cadastre_togo.json`
  fournit les motifs `section_pattern` / `number_pattern` (lus par
  `src/plugins/cadastre/Templates.cpp`), et le validateur d'identification est
  construit avec ces motifs au chargement du module. Le module ne devine pas où
  sont ses données : l'hôte annonce ses répertoires
  (`$BCAD_PLUGIN_DATA` en tête, `share/bcad/plugins` de l'installation et de
  l'arbre de build, `$XDG_DATA_HOME/bcad/plugins`) et le module résout
  `cadastre/templates/…` dans cet ordre. Les valeurs par défaut du code
  (`^[A-Z]{1,3}$`, `^[0-9]+$`) ne restent que le repli quand aucun gabarit n'est
  trouvé : poser un autre profil change désormais le contrôle sans recompiler le
  module. Un gabarit illisible (JSON invalide, `schema_version` inconnu) n'est
  **pas** appliqué silencieusement ; la valeur par défaut tient lieu de contrat.
- **Le profil est désigné par l'opérateur**, pas par le code : l'action
  « Profil cadastral du dossier... » (`cadastre.set_profile`, paramètres
  `PromptText`) demande une chaîne que seul le module sait interpréter. La
  commande assume ce nom comme un **nom** et non comme un chemin (`..`,
  séparateurs, segment qui ne commence pas par une lettre sont refusés — sans
  quoi la saisie ouvrirait un fichier hors des données du module), puis range le
  profil retenu dans les attributs du dossier (`cadastre.dossier.profil`, en
  mémoire jusqu'à v3) et remplace les motifs de la règle
  `cadastre.identification` enregistrée chez l'hôte. Annuler la commande défait
  les deux. Comme le gabarit est ouvert **après** l'initialisation, le module a
  dû copier la liste des répertoires de données (`dataDirectories()`) : le
  `PluginRegistry` meurt à la sortie de `bcad_plugin_init` et les *factories* de
  commande ne captent rien.
- Le libellé du lot `cadastre.identification` **nomme le profil appliqué** (et
  le dit sans gabarit quand le fichier manque) : la règle change de valeur à
  chaud, l'opérateur doit donc pouvoir lire laquelle a répondu. Un profil hérité
  d'un dossier précédent — `IValidator::validate` ne voit pas le document, et
  l'hôte ne recharge pas une règle par dossier — se lit ainsi dans le panneau au
  lieu de fausser la réponse en silence. Le lien profil ↔ document est à la
  tranche 2 de l'ADR-017.
- Commandes de création, séparation, fusion et modification de limite avec
  undo/redo.
- **Recherche par référence cadastrale** : `cadastre.find_parcel` sélectionne les
  parcelles d'une section et d'un numéro donnés, depuis l'action « Rechercher une
  parcelle... » du workbench. La référence est saisie en texte libre (stratégie
  `PromptText`, question rédigée par le module) et acceptée sous ses formes
  écrites (`A 007`, `A-7`, `A|7`, `A7`) ; la section et le numéro sont comparés
  après normalisation, donc `A 7` trouve la parcelle enregistrée `007`. L'action
  ne déplace que la sélection : elle ne marque pas le document comme modifié et
  n'entre pas dans la pile d'annulation. C'est le rétablissement de la
  régression F4, supprimée sans successeur lors de la migration en module
  dynamique.
- Génération de plan cadastral PDF A3 paysage via
  `cadastre.generate_plan_sheet`. La feuille est **composée** : `layout/CadastreSheet.cpp`
  convertit les propriétés `cadastre.*` des parcelles en étiquettes au centroïde,
  bornes (sommets dédupliqués) et tableau des parcelles, que `layout::drawSheet`
  imprime avec le cartouche, la flèche nord et la barre d'échelle. `src/layout`
  ne connaît aucun de ces noms de propriété (ADR-016) : il dessine ce qu'on lui
  fournit. Couvert par `cadastre_sheet_test` et `cadastre_plan_sheet_test`.
- Actions du menu et du ruban `Cadastre` câblées au canevas : création,
  scission médiane, fusion de deux parcelles, modification d'un sommet de
  parcelle sélectionnée et recherche par référence.
- Les actions qui changent le dessin utilisent le `QUndoStack` de
  l'application ; celles qui ne font que déplacer la sélection ou produire un
  livrable extérieur en restent hors (voir `modifiesDocument`) et ne marquent pas
  le document comme modifié. Les sélections invalides sont refusées avec un
  message visible dans la barre d'état.
- Chargement automatique : l'hôte énumère ses répertoires de modules
  (`$BCAD_PLUGIN_PATH`, `lib/bcad/plugins` de l'installation, arbre de build,
  `$XDG_DATA_HOME/bcad/plugins`) et ne nomme aucun plugin (ADR-016). La découverte
  est testée par `discovery_test` et prouvée de bout en bout par
  `cadastre_external_test`, qui charge le module, exécute ses validateurs depuis
  l'hôte, puis le décharge.
- Templates JSON du profil Togo, des calques et des styles : **un seul des quatre
  est lu**. `cadastre_togo.json` pilote les motifs d'identification, comme dit
  plus haut. `layers.json`, `plot_styles.json` et `text_styles.json` restent
  installés sans consommateur : le module ne déclare aucun calque (les calques
  sont créés par l'hôte à la demande de l'utilisateur) et les épaisseurs comme
  les polices de la feuille sont choisies par `src/layout/` — lire ces fichiers
  voudrait dire créer un point d'extension (styles de document, calques déclarés
  par un module) que rien d'autre n'attend. Dans le profil lu,
  `survey_tolerance.default_m` et `units.*` ne sont **pas** non plus consommés :
  la première faute de règle à alimenter tant que `BoundaryEntity` ne porte ni
  section ni numéro, les secondes faute de consommateur — inventer une conversion
  pour les lire serait inventer une règle métier.

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
`check_arch.sh` est devenu une **étape de la CI** (`.github/workflows/ci.yml`,
avant l'installation des dépendances) : ces règles portent sur les sources
seules, donc une violation doit coûter quelques secondes et non un build complet.

Ce que chacun couvre côté validation et découverte :

| Test | Prouve |
|------|--------|
| `cadastre_cartouche_test` | résolution du cartouche : l'attribut du dossier prime, une valeur de parcelle ne remonte que si **toutes** les parcelles la portent, deux parcelles discordantes laissent le champ vide, aucun titre n'est inventé |
| `cadastre_profil_test` | un code de profil reste un nom (refus de `..`, des séparateurs, d'un segment non alphabétique), le gabarit est résolu dans les répertoires memorisés **apres** l'initialisation, une commande rendue `nullptr` pour un nom inconnu, et la regle enregistree chez l'hote applique le motif choisi puis reprend le precedent a l'annulation |
| `document_properties_test` | les attributs du dossier portent la saisie, ne passent par aucun chemin geometrique (ni entite, ni index, ni emprise), et sont vides avec le document |
| `cadastre_validators_test` | les trois règles rendent les diagnostics attendus (huit, recouvrement, identification) et un document propre n'en rend aucun |
| `validator_test` | cycle de vie `IValidator` : enregistrement, doublon refusé, **destruction avant `dlclose`** |
| `discovery_test` | sélection dans les répertoires, priorité de `$BCAD_PLUGIN_PATH`, déduplication, **ordre déterministe** |
| `cadastre_external_test` | un plugin construit hors arbre (`find_package(BCAD)`) déclare un validateur que l'hôte exécute, puis est déchargé ; la sortie du processus ne plante pas |
| `cadastre_search_test` | la référence cadastrale est normalisée dans ses formes écrites, une saisie vide ou ambiguë est refusée, la recherche remplace la sélection, ne touche qu'elle et s'annule ; un polygone importé de DXF et porteur des propriétés est retrouvé |
| `cadastre_templates_test` | le gabarit est lu depuis le répertoire de données, une clé absente laisse la valeur par défaut, un JSON invalide ou un `schema_version` inconnu n'est pas appliqué, et le motif lu remonte vraiment dans le diagnostic rendu par la règle |
| `check_arch.sh` §10bis | `src/app/` ne nomme aucun module et n'écrit aucun littéral métier, y compris dans les `tr()` |

Ce que chacun couvre côté mise en page :

| Test | Prouve |
|------|--------|
| `composition_test` | découpage de la feuille en millimètres : le plan ne descend pas sur le cartouche, la colonne du tableau est retranchée de sa place, flèche et barre d'échelle restent sur le plan, barre d'échelle à distances rondes |
| `sheet_render_test` | le peintre est bien au millimètre de la **zone imprimable** : l'encre mesure la feuille, et la résolution du périphérique n'est pas supposée (elle est vérifiée sur `QPrinter`) |
| `cadastre_sheet_test` | les propriétés cadastrales deviennent étiquettes, bornes dédupliquées et tableau, et une polygone sans attribut n'est pas une parcelle |
| `cadastre_plan_sheet_test` | la commande `cadastre.generate_plan_sheet` produit un vrai PDF, l'annule, et ne laisse aucun fichier sur un document vide |
