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
| Propriétés cadastrales | `PropertyMap` et panneau d’extension plugin présents |
| Création, scission, fusion et modification de parcelles | Commandes présentes et branchées à l’interface |
| Validations cadastrales de base | Fermeture, aire et contrôles métier partiels présents |
| Interface française | Menus, ruban, accès rapide et actions cadastrales présents |
| Formats actuels | `.bcad`, SQLite, DXF, GeoJSON et CSV présents selon les surfaces couvertes |
| Mise en page et cartouche | Formats A4 à A0, orientation, marges et cartouche présents |
| Impression PDF vectorielle | Présente ; lignes ouvertes et surfaces sont distinguées |
| Cotations | Linéaire, alignée, angulaire, rayon et diamètre sont interactives |
| Textes de cotation | Désormais entités `bcad.Text`, sérialisées, visibles dans le canevas et exportées en PDF |
| Tests | Build, 25 tests CTest et contrôle architectural validés avant cette évolution ; à rejouer après modification |

## Ce qui reste à faire

### Cotations et annotations

- Ajouter des flèches normalisées et des styles de cotation configurables.
- Persister la relation sémantique entre texte, lignes d’attache et mesure.
- Exporter les cotations natives en objets DXF `DIMENSION`, plutôt qu’en
  géométrie et texte séparés.
- Gérer le placement et la rotation des textes sur les segments obliques.

### Topologie cadastrale

- Détection complète des auto-intersections, chevauchements, trous et doublons
  d’identifiants.
- Contrôle de compatibilité de limites communes et rattachement obligatoire aux
  bornes selon le référentiel choisi.
- Rapport de tolérance, fermeture et incertitudes de levé.

### Levé et référentiels

- Import GNSS/station totale avec codes terrain et Z.
- Transformations entre systèmes de coordonnées et contrôle de projection.
- Tableau de coordonnées, calculs de gisement/fermeture et rapport opérateur.

### Interopérabilité et production

- GeoPackage, Shapefile, DWG et services ArcGIS/WMS/WMTS.
- Espaces papier complets avec viewports à échelle fixe, grille, nord,
  barre d’échelle, légende et tableaux automatiques.
- Templates administratifs configurables avec styles, épaisseurs, transparence,
  verrouillage et imprimabilité par calque.
- Signatures et archivage dans une GED.

## Écarts importants de l’audit fourni

L’audit joint décrit des entités comme `TextEntity`, `NorthArrowEntity`,
`ScaleBarEntity` et des formats comme DWG ou GeoPackage comme des objectifs.
Avant cette mise à jour, plusieurs n’étaient pas des entités natives du dépôt.
La présente évolution ajoute uniquement `bcad.Text`, car il est nécessaire à la
persistance des cotations. Les autres éléments restent explicitement dans la
liste des travaux à planifier.

## Validation attendue après cette évolution

```text
cmake --build build -j2
ctest --test-dir build --output-on-failure
scripts/check_arch.sh
git diff --check
```

Les avertissements de dépréciation liés à `EntityType` restent compatibles
avec l’ADR de rétrocompatibilité ; les nouveaux chemins utilisent `TypeId`.
