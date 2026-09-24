# État du plugin cadastral

## Fonctionnalités opérationnelles

- Entités `ParcelEntity`, `BoundaryEntity`, `SurveyMarkEntity` et
  `EasementEntity`.
- Enregistrement dynamique des entités, commandes et serializers via
  `PluginRegistry`.
- Retrait des extensions avant `dlclose()`.
- Validateurs de topologie, chevauchement, identifiant et tolérance.
- Commandes de création, séparation, fusion et modification de limite avec
  undo/redo.
- Génération de plan cadastral PDF A3 paysage via
  `cadastre.generate_plan_sheet`.
- Actions du menu et du ruban `Cadastre` câblées au canevas : création,
  scission médiane, fusion de deux parcelles et modification d'un sommet de
  parcelle sélectionnée.
- Les actions utilisent le `QUndoStack` de l'application ; les sélections
  invalides sont refusées avec un message visible dans la barre d'état.
- Chargement automatique depuis `BCAD_PLUGIN_PATH` dans `MainWindow`.
- Templates JSON installables pour le profil Togo, les calques et les styles.

## Formats

Le format GeoJSON et l'export CSV des coordonnées sont fournis par le plugin.
L'application expose également ces deux exports dans le menu `Fichier` pour
exporter le document courant sans connaître le nom interne du plugin.
Le format `.bcad` reste le format natif SQLite de BCAD.

GeoPackage est implémenté par SQLite pour les parcelles polygonales.
ArcGIS reste volontairement indisponible tant qu'un connecteur GDAL/OGR
explicite n'est pas intégré. Une opération IO indisponible doit échouer
explicitement ; elle ne doit pas retourner `true` sans produire de fichier.

## Interface utilisateur

Le shell Qt conserve un canevas central, les docks `Calques` et `Propriétés`,
une ligne de commande, une barre d'état et l'undo/redo. Le chargement du
plugin est explicite/configurable et ne rend pas le Core dépendant du
cadastre.

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
