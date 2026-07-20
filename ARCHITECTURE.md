# Architecture

Ce document fait correspondre l'esquisse de conception d'origine dans
[`architecture bcad.txt`](architecture%20bcad.txt) à l'organisation réelle
des modules. Voir [`DESIGN_NOTES.md`](DESIGN_NOTES.md) pour la comparaison
avec LibreCAD/QCAD/FreeCAD.

```
┌──────────────────────────────────────────────────────────────┐
│ app/        MainWindow, Viewport, LayerPanel, TessellationWorker │
│             Interface Qt, outils interactifs, colle de threading │
├──────────────────────────────────────────────────────────────┤
│ render/     Camera2D, LevelOfDetail, Quadtree, GlRenderer,      │
│             TessellationTypes                                   │
│             Pipeline OpenGL 3.3 core, indexation spatiale, LOD  │
├──────────────────────────────────────────────────────────────┤
│ core/       Document                                            │
│             Possède les entités + LayerManager + Quadtree,      │
│             thread-safe via un shared_mutex (lecteurs :          │
│             rendu/pick, écrivains : add/remove/transform)        │
├──────────────────────────────────────────────────────────────┤
│ io/         DxfReader, DxfWriter, Database                      │
│             Sous-ensemble ASCII DXF R2000 ; format natif         │
│             SQLite .bcad                                        │
├──────────────────────────────────────────────────────────────┤
│ layers/     Layer, LayerManager                                 │
│             Références de calques par nom (convention DXF)       │
├──────────────────────────────────────────────────────────────┤
│ geometry/   Entity, Line/Circle/Arc/Polyline/Point, Transform2D,│
│             BooleanOps, Triangulation, GeometryUtils (CGAL)      │
│             Le seul module sans dépendance vers les autres      │
└──────────────────────────────────────────────────────────────┘
```

Le sens des dépendances est strictement descendant — `geometry` ne dépend
de rien d'autre dans ce projet, `layers` ne dépend que de `geometry`,
`render` dépend de `geometry` (+ Qt/GL), `core` dépend de `layers` +
`render`, `io` dépend de `core`, `app` dépend de tout. C'était une
correction délibérée en cours d'implémentation : une version antérieure
avait le `TessellationWorker` de `render` qui incluait `core::Document`,
ce qui est à l'envers (`core` dépend déjà de `render` pour
`Quadtree`/`TessellationTypes`) et aurait créé une dépendance circulaire
entre cibles CMake. `TessellationWorker` vit désormais dans `app`, puisque
c'est en réalité de la colle de threading Qt entre un `core::Document` et
un `render::TessellationResult`, pas une primitive de rendu en soi.

## Décisions de conception clés

**Choix du kernel.** `geometry::Kernel` est
`Exact_predicates_inexact_constructions_kernel` de CGAL — suffisamment
exact pour des opérations booléennes et une triangulation robustes,
suffisamment rapide (constructions en double précision) pour un
glisser-déposer interactif. `Simple_cartesian` serait plus rapide mais
dangereux pour les booléens ; un kernel totalement exact serait sûr mais
trop lent pour le glisser-déposer en direct.

**Les entités possèdent leur représentation exacte.** `ArcEntity` stocke
centre/rayon/angles, pas une polyligne pré-tessellée — `tessellate(double
maxDeviation)` est appelée à la volée au moment du rendu/export avec une
tolérance de déviation pilotée par le zoom courant
(`render::worldToleranceForZoom`). C'est ce qui fait fonctionner le LOD :
les cercles sont des boucles de lignes bon marché en dézoomé, denses en
zoomé, et l'export DXF écrit une vraie entité `ARC`/`CIRCLE` plutôt qu'une
polyligne à résolution fixe.

**Document est délibérément non copiable.** Il détient un
`std::shared_mutex` qui protège la liste d'entités + le quadtree, pour
qu'un thread `TessellationWorker` en arrière-plan puisse l'interroger en
sécurité (`buildTessellation`, verrou partagé) pendant que le thread GUI
le modifie (`addEntity`/`removeEntity`/`notifyEntityChanged`, verrou
exclusif). À cause de ce mutex, `Document` ne peut être ni déplacé ni
copié — les fonctions de chargement d'`io::` prennent un `Document&`
(remplissage en place) plutôt que de renvoyer un `Document` par valeur.

**Le rendu se fait en deux étapes.** `TessellationWorker` (thread
travailleur) transforme les entités visibles en tableaux de sommets
`ColorBatch` plats — aucun appel GL, seulement `Entity::tessellate()`
regroupé par couleur résolue. `GlRenderer` (thread GL/principal) ne fait
qu'envoyer ces données et émettre `glMultiDrawArrays` — un appel de dessin
par couleur distincte à l'écran, pas par entité. Ce sont les cases
« multi-threading rendering » et « frustum culling » de l'esquisse
d'origine : le culling est la requête de région du quadtree à l'intérieur
de `buildTessellation`, le threading consiste à exécuter cette requête +
la tessellation hors du thread GL.

**Les calques sont référencés par nom, pas par pointeur.** Correspond
directement à la convention du code groupe `8` de DXF, donc
l'import/export n'a pas besoin d'une table d'indirection, et les calques
peuvent être renommés sans parcourir chaque entité.

## Écarts connus par rapport à l'esquisse d'origine

- Vulkan : non implémenté (OpenGL 3.3 core uniquement). À reconsidérer si
  le multi-GPU ou la tessellation par compute shader devient pertinent au
  vu de la complexité.
- DWG : non implémenté — c'est un format binaire fermé et en évolution
  constante ; les chemins réalistes sont de lier une bibliothèque DWG
  (par ex. `libdxfrw`, sous LGPL, ne lit que le DXF, pas le DWG) ou de
  passer par un convertisseur externe.
- Les opérations booléennes / la triangulation sont implémentées et
  testées unitairement (`tests/smoke_test.cpp`) mais pas encore exposées
  en tant que commande GUI.
