# Notes de conception : études préalables

Avant/pendant l'écriture de ce projet, j'ai comparé les frontières de
modules de bcad à trois bases de code CAO open source établies et à la
direction UI actuelle d'Autodesk, pour vérifier la pertinence du découpage
d'`architecture bcad.txt` plutôt que d'inventer des frontières de modules
dans le vide.

## LibreCAD

Basé sur Qt, C++17, le cousin architectural le plus proche de bcad (2D,
Qt, DXF-first). Son découpage en couches correspond presque un pour un au
nôtre :

| LibreCAD | bcad | Notes |
|---|---|---|
| `RS_Entity` / `RS_EntityContainer` | `geom::Entity` / `core::Document` | LibreCAD imbrique les conteneurs (blocs, polylignes *et* le document lui-même dérivent tous de la même base conteneur) ; bcad garde `PolylineEntity` comme une simple liste de sommets et `Document` comme un propriétaire distinct, non-`Entity`. Plus simple, au prix de ne pas pouvoir imbriquer un `Document` dans une autre entité (les blocs, une fois ajoutés, auront besoin de leur propre petit type conteneur). |
| `RS_LayerList` / `RS_BlockList` (possédés par `RS_Graphic`) | `layers::LayerManager` (possédé par `core::Document`) | Même idée : les calques sont un objet gestionnaire suspendu au document, pas un type d'entité de premier ordre. |
| `RS_GraphicView` | `app::Viewport` + `render::GlRenderer` | LibreCAD combine vue/interaction/dessin dans une seule classe ; bcad sépare le dessin GL (`GlRenderer`) de la gestion des événements (`Viewport`) et pousse la tessellation vers un thread travailleur — LibreCAD précède l'attente que cela devienne nécessaire aux tailles de dessin visées par LibreCAD. |
| `RS_ActionInterface` | énum `ToolMode` + switch dans `Viewport` | Celui de LibreCAD est une vraie pile d'objets d'action polymorphes (favorable à l'undo, une classe par outil). Celui de bcad, énum+switch, est la version MVP délibérément plus petite — signalée dans la feuille de route comme devant devenir polymorphe une fois l'undo/redo en place, puisqu'une pile d'objets `Action` est ce qui rend l'undo praticable. |
| `RS_FilterDXFRW` | `io::DxfReader` / `io::DxfWriter` | LibreCAD délègue à la bibliothèque externe `libdxfrw` pour une couverture DXF/DWG complète. bcad écrit à la main un sous-ensemble ASCII DXF R2000 (LINE/CIRCLE/ARC/LWPOLYLINE + calques) — surface bien plus réduite, mais pas de DWG ni d'entités spline/hachure/cotation pour l'instant. Si la fidélité DXF devient une priorité, vendoriser `libdxfrw` est le chemin éprouvé par LibreCAD plutôt que de faire grandir indéfiniment le parseur écrit à la main. |

Source : [LibreCAD/LibreCAD sur DeepWiki](https://deepwiki.com/LibreCAD/LibreCAD).

## QCAD

Également Qt+C++ au cœur, mais pousse presque toute la logique des
*outils* dans une couche ECMAScript (`scripts/`, point d'entrée
`autostart.js`) — le cœur C++ reste petit et générique, les
outils/menus/dialogues sont scriptés. bcad ne fait pas ça (les outils sont
du C++ compilé dans `Viewport`), ce qui est le bon choix pour un projet
personnel à cette échelle — une couche de script est beaucoup
d'infrastructure à mettre en place pour racheter une extensibilité dont
bcad n'a pas encore besoin — mais ça vaut la peine de s'en souvenir comme
réponse le jour où « permettre aux utilisateurs d'écrire des outils
personnalisés sans recompiler » deviendra un vrai besoin.

Source : [qcad/qcad sur GitHub](https://github.com/qcad/qcad), [documentation développeur ECMAScript de QCAD](https://qcad.org/doc/qcad/3.0/developer/group__ecma__scripts.html).

## FreeCAD

Paramétrique 3D, une portée bien plus large que bcad, mais son principe de
répartition en couches de haut niveau est celui qui vaut la peine d'être
emprunté indépendamment de la dimensionnalité : une séparation stricte
entre un modèle de données document-objet et le noyau géométrique/solveur
de contraintes en dessous (`Sketcher::SketchObject` stocke géométrie +
contraintes ; `planegcs` les résout ; le GUI ne parle jamais qu'à l'objet
document, jamais directement au solveur). Le découpage `core::Document` /
`geometry::` de bcad suit le même principe à l'échelle 2D — le GUI
(`app::Viewport`) ne touche jamais aux types CGAL directement, seulement à
`geom::Entity`. Là où bcad n'a aujourd'hui *aucun* équivalent, c'est le
solveur de contraintes de FreeCAD : il n'y a pas de couche
paramétrique/contraintes dans bcad actuellement (les entités sont dessinées
à des coordonnées fixes, pas résolues à partir de contraintes) — à garder
en tête comme le plafond de ce que bcad peut devenir en termes de
« paramétrique » sans un ajout bien plus conséquent.

Source : [FreeCAD/FreeCAD sur DeepWiki](https://deepwiki.com/FreeCAD/FreeCAD).

## UI : thème sombre

AutoCAD est livré avec un thème de ruban sombre par défaut depuis
l'introduction de la variable système `COLORTHEME` dans AutoCAD 2016, sur
le principe qu'un canevas sombre est plus reposant pour les yeux sur de
longues sessions — les couleurs d'entités pleinement saturées ressortent
clairement sur un chrome sombre à faible chroma.
`app::MainWindow::applyDarkTheme()` suit la même logique (chrome neutre
sombre `#2b2d31`/`#202124`, pas encore de sélecteur de thème). Le ruban
d'AutoCAD (onglets → panneaux → outils) a d'abord été estimé comme un
investissement UI nettement plus lourd que ce qu'une barre d'outils plate
justifiait, et a été reporté. Reconsidéré une fois davantage d'outils à
organiser existants (outils de dessin, opérations booléennes, bascules
vue/accrochage) : `app::RibbonBar` (onglets de panneaux légendés,
construits sur `QTabWidget`/`QToolButton` standards, sans bibliothèque de
ruban externe) s'est avéré assez bon marché à construire directement
plutôt que de rester reporté — voir `CAHIER_DES_CHARGES.md` §2.13. Il
réutilise les mêmes objets `QAction` que la barre de menus classique
(conservée à ses côtés, pas remplacée, pour la découvrabilité et les
raccourcis clavier), donc aucune logique n'est dupliquée entre les deux.

Source : [AutoCAD 2025 Help — About the Ribbon](https://help.autodesk.com/view/ACD/2025/ENU/?guid=GUID-D20EF1D7-4135-48A7-B68E-65BF3BFF3D70), [Color Theme (COLORTHEME) in AutoCAD](https://knowledge.autodesk.com/support/autocad/getting-started/caas/simplecontent/content/color-theme-autocad.html).
