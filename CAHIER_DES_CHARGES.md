# Cahier des charges — bcad

Document vivant : à mettre à jour à chaque nouvelle fonctionnalité ou
changement de priorité. Voir aussi [`README.md`](README.md) (build/usage),
[`ARCHITECTURE.md`](ARCHITECTURE.md) (mapping code) et
[`DESIGN_NOTES.md`](DESIGN_NOTES.md) (comparaison LibreCAD/QCAD/FreeCAD).

## 1. Objectif du projet

Logiciel de CAO 2D personnalisé dans l'esprit d'AutoCAD : moteur géométrique
robuste (CGAL), rendu OpenGL performant sur de grands dessins, système de
layers façon DXF, interopérabilité DXF, format de projet natif, interface Qt
moderne (thème sombre).

## 2. État actuel — fonctionnalités implémentées

### 2.1 Moteur géométrique (`geometry/`)

- [x] Types de base : `Point2`, `Vector2`, `BoundingBox`, `Color`, kernel CGAL
      `Exact_predicates_inexact_constructions_kernel` (robuste + rapide pour
      l'interactif).
- [x] Entités : `LineEntity`, `CircleEntity`, `ArcEntity`, `PolylineEntity`
      (ouverte/fermée), `PointEntity`. Chaque entité stocke sa représentation
      exacte (ex. un arc garde centre/rayon/angles, pas une polyligne figée).
- [x] `Entity::tessellate(maxDeviation)` : approximation polyligne à densité
      variable selon une tolérance — c'est le point d'entrée unique utilisé
      par le rendu, l'export DXF et le hit-testing de secours.
- [x] `Entity::distanceTo(point)` : distance exacte point→entité, utilisée
      pour la sélection au clic.
- [x] Transformations : translation, rotation (autour d'un pivot), échelle
      (uniforme et non-uniforme), miroir X/Y (`Transform2D`).
- [x] Calculs géométriques : distance, angle de vecteur, angle entre deux
      vecteurs, projection point→segment, distance point→droite
      (`GeometryUtils`).
- [x] Opérations booléennes sur polygones : union, intersection, différence,
      différence symétrique, via `CGAL::Boolean_set_operations_2`
      (gère les trous, normalise l'orientation CCW automatiquement).
- [x] Triangulation : Delaunay simple (nuage de points) et Delaunay
      contrainte avec trous (remplissage de polygone non convexe) via
      `CGAL::Constrained_Delaunay_triangulation_2` + marquage de domaine par
      parcours en largeur.
- [x] Testé (`tests/smoke_test.cpp`) mais **pas encore relié à une commande
      GUI** — voir §3.

### 2.2 Layers (`layers/`)

- [x] `Layer` : nom, couleur, épaisseur de trait, visibilité, verrouillage,
      type de trait (continu/tirets/pointillé/tiret-point — le type n'est
      pas encore utilisé au rendu, voir §3).
- [x] `LayerManager` : création, suppression (layer `"0"` protégé comme sous
      AutoCAD/LibreCAD), renommage, recherche, layer courant, callback
      `onChanged` pour rafraîchir la GUI sans polling.
- [x] Entités référencent leur layer **par nom** (convention DXF group code
      8), pas par pointeur — renommage sans toucher aux entités.

### 2.3 Indexation spatiale et rendu (`render/`)

- [x] `Quadtree` : index spatial sur les bounding boxes des entités,
      insertion/suppression/mise à jour/requête par région, grandit
      automatiquement si une entité sort des limites initiales.
- [x] `Camera2D` : pan, zoom au curseur (le point sous la souris reste fixe),
      zoom-to-fit, conversions monde↔écran.
- [x] LOD (`LevelOfDetail`) : tolérance de tessellation dérivée du zoom
      courant (déviation cible en pixels) — les cercles/arcs ont peu de
      segments dézoomé, beaucoup zoomé.
- [x] Pipeline OpenGL 3.3 Core (`GlRenderer`) : shaders GLSL minimalistes
      (position + couleur uniforme), un seul upload de VBO par frame, tracé
      via `glMultiDrawArrays` — un draw call par couleur distincte visible,
      pas par entité.
- [x] Threading : `TessellationWorker` tourne sur un `QThread` dédié, calcule
      les lots de vertices (culling via quadtree + tessellation LOD) sans
      jamais toucher OpenGL ; le thread GL ne fait qu'uploader/dessiner le
      résultat déjà calculé. `Document` protégé par `std::shared_mutex`
      (lecture partagée pour le worker, écriture exclusive pour les édits
      GUI) pour que ça reste sûr en concurrence.
- [x] Debounce des requêtes de tessellation (16 ms) pour ne pas saturer le
      worker pendant un pan/zoom continu.

### 2.4 Document central (`core/`)

- [x] `Document` : possède les entités, le `LayerManager` et le `Quadtree`,
      assigne les id, notifie les changements (`onChanged`).
- [x] `addEntity` / `removeEntity` / `notifyEntityChanged` (après une
      transformation) / `pickEntity` (sélection au clic avec tolérance) /
      `entitiesInRegion` / `extents` (bounding box globale, pour zoom-to-fit).
- [x] `buildTessellation(region, tolerance)` : construit les lots de rendu
      groupés par couleur — cœur du pipeline de rendu, thread-safe.
- [x] Volontairement non copiable/déplaçable (à cause du mutex) : les
      chargeurs `io::` peuplent un `Document&` existant plutôt que d'en
      retourner un par valeur.

### 2.5 Import/export (`io/`)

- [x] DXF (sous-ensemble R2000 ASCII) : écriture et lecture de
      `LINE`/`CIRCLE`/`ARC`/`LWPOLYLINE` + table `LAYER` (couleur ACI,
      visibilité, verrouillage). Lecture tolérante : accepte aussi
      l'ancien style `POLYLINE`/`VERTEX`/`SEQEND`.
- [x] Format de projet natif `.bcad` (SQLite) : tables `layers` et
      `entities`, géométrie sérialisée en CSV compact. Inspectable avec
      n'importe quel outil SQLite.
- [x] Correspondance couleur RVB ↔ palette ACI (8 couleurs de base) pour le
      DXF — pas de fidélité totale avec la palette 255 couleurs d'AutoCAD.

### 2.6 Interface Qt (`app/`)

- [x] `MainWindow` : thème sombre façon AutoCAD (`COLORTHEME` par défaut),
      menu File/Edit/View/Modify, `RibbonBar` (voir §2.13) sous la barre de
      menu, ligne de commande dans la barre de statut (voir §2.14), barre de
      statut (coordonnées curseur + outil actif).
- [x] `Viewport` (`QOpenGLWidget`) : rendu + overlay `QPainter` pour
      l'aperçu de l'outil en cours (points cliqués, ligne élastique, cercle
      de prévisualisation) et les marqueurs de snap.
- [x] Outils interactifs : Select (clic = sélection), Move (pick puis
      clic-destination), Line (2 clics), Circle (centre + rayon au 2ᵉ clic),
      Arc (centre, point de départ, point de fin), Polyline (clics
      multiples, Entrée ou clic-droit pour terminer, Échap pour annuler).
      Chaque point de placement accepte aussi la saisie clavier (§2.14).
- [x] Pan (clic molette + glisser), zoom (molette, centré sur le curseur),
      zoom-to-fit (touche `F`).
- [x] `LayerPanel` (dock) : liste des layers avec cases visibilité/
      verrouillage, pastille de couleur (double-clic → sélecteur), nom
      éditable en ligne, boutons +/- layer.
- [x] Build et exécution vérifiés dans cet environnement (WSLg) : l'app
      compile, les tests passent, le rendu s'affiche correctement (confirmé
      visuellement).

### 2.7 Tests

- [x] `tests/smoke_test.cpp` : un contrôle par module (géométrie, entités,
      booléens, triangulation, layers, Document/quadtree, round-trip DXF,
      round-trip SQLite). Pas un vrai framework de tests unitaires — sert de
      garde-fou anti-régression de build, pas de couverture exhaustive.

### 2.8 Accrochage (`SnapEngine`)

- [x] Accrochage extrémité/milieu/centre : `SnapEngine::findSnap` interroge
      le quadtree autour du curseur et retient le point le plus proche dans
      une tolérance écran, avec priorité extrémité/centre > milieu (comme
      dans la plupart des CAO : les coins sont plus "collants" qu'un milieu
      même légèrement plus proche).
- [x] Utilisé par tous les outils de dessin (Line/Circle/Arc/Polyline) et par
      le point de destination de l'outil Move — pas par le pick initial
      (Select, 1er clic de Move), qui reste une sélection d'entité normale.
- [x] Indicateur visuel dans le viewport (carré = extrémité, cercle = centre,
      triangle = milieu, losange = intersection, équerre = perpendiculaire,
      croix = grille), activable/désactivable via le menu View → Toggle
      Object Snap (`F3`).
- [x] Accrochage intersection (`geometry::entityIntersections`, dans
      `geometry/SnapGeometry.h`) : intersections exactes (pas tessellées)
      entre paires d'entités proches du curseur — segment/segment (solveur
      linéaire direct), segment/cercle et cercle/cercle (formules
      analytiques classiques), filtrées par le secteur angulaire pour les
      arcs. Même tier de priorité qu'extrémité/centre.
- [x] Accrochage perpendiculaire (`geometry::perpendicularFoot`) : actif
      seulement quand un point de référence existe (dernier point posé par
      l'outil en cours) — contrairement aux autres snaps, l'activation se
      fait par proximité à l'**entité** (survoler la ligne/le cercle
      n'importe où), pas par proximité du point résultat, qui peut être
      loin du curseur le long de l'entité. Reproduit le comportement PER
      d'AutoCAD. Priorité la plus basse (seulement si rien d'autre ne
      correspond).

### 2.9 Undo/Redo

- [x] `QUndoStack` (Qt) possédé par `MainWindow`, exposé au `Viewport` via
      `setUndoStack`. `AddEntityCommand`/`RemoveEntityCommand`/
      `TransformEntityCommand` (`include/bcad/app/Commands.h`) encapsulent
      chaque mutation du `Document` — c'est la pile d'actions polymorphes
      évoquée dans `DESIGN_NOTES.md` en comparaison avec
      `RS_ActionInterface` de LibreCAD.
- [x] Toutes les commandes de dessin (Line/Circle/Arc/Polyline), le
      déplacement (Move) et la suppression passent par la pile — `Ctrl+Z` /
      `Ctrl+Y` (menu Edit) annulent/rejouent chaque étape individuellement.
- [x] Suppression d'entité(s) sélectionnée(s) : menu Edit → Delete (touche
      `Suppr`), regroupe plusieurs suppressions en une seule macro undo si
      plusieurs entités sont sélectionnées.
- [x] La pile est vidée à New/Open/Import DXF (les commandes existantes
      référencent des id d'entités qui n'ont plus de sens après un
      remplacement complet du contenu du document).

### 2.10 Comparatif avec une check-list noyau CAO générique

Comparaison avec une liste type "fonctionnalités d'un noyau CAO" (B-Rep,
coordonnées, primitives, tolérance, snap, courbes, booléens, contraintes,
accélération spatiale, précision arbitraire). Sert de repère externe en plus
de la comparaison LibreCAD/QCAD/FreeCAD de `DESIGN_NOTES.md`.

| Fonctionnalité | État | Détail |
|---|---|---|
| B-Rep (Boundary Representation) | ❌ | Pas de topologie faces/edges/vertices — concept surtout utile en 3D/solides, hors périmètre (2D par choix, §4). |
| Coordonnées 2D + matrices/transfo | 🟡 | 2D complet (`geometry::Transform2D`). 3D absent, volontairement (§4). |
| Primitives : point, ligne, arc, cercle | ✅ | `PointEntity`/`LineEntity`/`ArcEntity`/`CircleEntity` + `PolylineEntity` en plus. |
| Tolérance numérique (epsilon) | ✅ | Centralisé dans `geometry::Tolerance` (voir §2.11) — avant cette passe, epsilons codés en dur dispersés. |
| Snap-to-grid | ✅ | Voir §2.11 — vient compléter le snap aux objets déjà en place (§2.8). |
| Bézier / NURBS / splines | ❌ | Reporté, §3 P2. |
| Opérations booléennes | ✅ | Moteur (`geometry::BooleanOps`) + commande GUI (§2.12) : union/intersection/différence/différence symétrique sur les polylignes fermées sélectionnées. |
| Système de contraintes géométriques | ❌ | Hors périmètre assumé (§4) — équivalent d'un solveur type `planegcs` de FreeCAD, gros ajout d'architecture. |
| Accélération spatiale (BVH/quadtree/octree) | 🟡 | Quadtree fait et utilisé (culling + pick). Pas de BVH ; octree = 3D, hors périmètre. |
| Précision arbitraire (big decimals) | ❌ | Kernel CGAL à prédicats exacts / constructions en `double` (`Exact_predicates_inexact_constructions_kernel`) — pas d'arithmétique arbitraire, choix délibéré pour la vitesse en interactif. |

### 2.11 Tolérance numérique et grille

- [x] `geometry::Tolerance` (`include/bcad/geometry/Tolerance.h`) :
      constantes nommées centralisées (`kLinear`, `kDegenerateLength`,
      `kAngular`) + helpers `nearlyZero`/`nearlyEqual`. `GeometryUtils.h`
      (garde de longueur dégénérée dans `distancePointToLine` et
      `closestPointOnSegment`) refactoré pour s'appuyer dessus au lieu
      d'epsilons `1e-12` en dur.
- [x] Grille adaptative affichée dans le viewport (`render::
      adaptiveGridSpacing`, séquence 1-2-5 × 10ⁿ selon le zoom pour rester
      entre ~ni trop dense ni trop clairsemée) — points + axes surlignés à
      l'origine, dessinés en `QPainter` sous la géométrie GL (voir note
      technique ci-dessous). Toggle : menu View → Toggle Grid (`F7`).
- [x] Snap-to-grid (`render::snapToGrid`) : accrochage au point de grille le
      plus proche quand le snap aux objets ne trouve rien à portée — priorité
      au snap objet, la grille ne prend le relais que s'il échoue. Toggle
      indépendant : menu View → Toggle Grid Snap (`F9`), désactivé par
      défaut (comme sous AutoCAD, le snap objet suffit la plupart du temps).
- [x] Note technique : le fond + la grille sont maintenant peints par
      `QPainter` **avant** d'entrer dans le rendu GL natif (`GlRenderer` ne
      fait plus de `glClear` — il dessinerait par-dessus le fond/la grille
      déjà peints, sur le même framebuffer). Sinon la grille se serait
      retrouvée au-dessus du dessin au lieu d'en dessous.

### 2.12 Opérations booléennes en commande GUI

- [x] `Viewport::booleanOperation(geom::BooleanOp)` : applique
      union/intersection/différence/différence symétrique aux deux
      polylignes fermées actuellement sélectionnées, remplace les deux
      entités d'origine par le(s) résultat(s) — le tout comme une seule
      macro undo (`RemoveEntityCommand` × 2 + `AddEntityCommand` × N
      résultats). Message d'avertissement si la sélection ne correspond pas
      (pas exactement deux polylignes fermées) ou si l'opération ne produit
      aucune géométrie.
- [x] Exposé à la fois dans le menu **Modify** et dans le panneau ruban
      "Modifier → Booléen" (§2.13) — mêmes `QAction`, donc aucune logique
      dupliquée entre les deux points d'entrée.

### 2.13 Ruban (`RibbonBar`)

- [x] `include/bcad/app/RibbonBar.h` : approximation légère du ruban des
      versions récentes d'AutoCAD, construite sur `QTabWidget`/`QToolButton`
      standards (pas de bibliothèque de ruban externe). API : une seule
      méthode `addPanel(tabName, panelTitle, actions)`, qui crée l'onglet à
      la première utilisation et ajoute un panneau titré (boutons +
      légende) avec séparateur avant les panneaux suivants du même onglet.
- [x] Onglets/panneaux actuels : **Home** → Draw (Select/Move/Line/Circle/
      Arc/Polyline), Panels (bascule visibilité du dock Layers via
      `dock->toggleViewAction()`) ; **Modify** → Boolean, Edit (Delete/Undo/
      Redo) ; **View** → Navigate (Zoom to Fit), Snapping (Object Snap/
      Grid/Grid Snap).
- [x] Les actions du ruban sont les **mêmes objets `QAction`** que celles
      des menus classiques (File/Edit/View/Modify, conservés pour la
      découvrabilité et les raccourcis clavier) — cliquer un bouton du ruban
      ou l'entrée de menu équivalente déclenche exactement le même code.
- [x] Pas d'icônes (boutons texte uniquement, `Qt::ToolButtonTextOnly`) —
      pas de jeu d'icônes dans le projet ; amélioration visuelle possible
      plus tard sans changer l'architecture du widget.
- [x] **Bug corrigé (signalé par capture d'écran par l'utilisateur)** : les
      boutons avaient un plancher de largeur (60px) plus étroit que ce que
      leurs libellés demandaient — sous pression d'espace (l'onglet Modify
      avec ses ~16 boutons), Qt compressait tout vers ce plancher, rendant
      "Boolean Union"/"Boolean Intersection" identiques une fois tronqués
      en "Boo...ion". Une première tentative de correction (envelopper
      chaque onglet dans un `QScrollArea`) a cassé l'affichage plus qu'elle
      ne l'a réparé — styliser le fond du viewport interne d'un
      `QScrollArea` via des sélecteurs CSS imbriqués s'est révélé fragile,
      annulé. Correction retenue : suppression du plancher de largeur (les
      boutons prennent leur taille naturelle) + raccourcissement des
      libellés eux-mêmes (les 4 actions booléennes perdent leur préfixe
      redondant "Boolean", regroupées dans un sous-menu Modify → Boolean).

### 2.14 Saisie clavier de coordonnées

- [x] `bcad::app::parseCoordinateInput` (`include/bcad/app/CoordinateInput.h`,
      header-only, sans dépendance Qt — directement testable) : accepte
      `x,y` (absolu), `@dx,dy` (relatif au point de référence),
      `@dist<angle` (polaire relatif, degrés, 0° = +X, sens trigonométrique)
      et `dist<angle` (polaire absolu depuis l'origine). Rejette poliment
      les formes relatives sans point de référence actif et toute entrée
      malformée.
- [x] Ligne de commande dans la barre de statut de `MainWindow`
      (`QLineEdit`, Entrée = valider). Taper directement un chiffre, `@` ou
      `-` dans le viewport pendant un outil de dessin actif donne le focus
      à la ligne de commande et y recopie le caractère déjà tapé — imite
      l'activation de la saisie dynamique d'AutoCAD sans exiger un clic
      préalable dans le champ.
- [x] `Viewport::placePoint(world)` : nouveau point d'entrée partagé entre
      un clic souris (après snapping) et la saisie clavier — évite de
      dupliquer la logique par outil (Line/Circle/Arc/Polyline/destination
      de Move) entre les deux chemins.

### 2.15 Comparatif Viewport / Rendu / 3D

Vérifié directement dans le code (pas de suppositions) le 2026-07-20.

| Fonctionnalité | État | Détail |
|---|---|---|
| Viewport 2D avec zoom/pan (infini) | ✅ | `Camera2D` : pan, zoom au curseur, zoom-to-fit. Coordonnées en `double`, quadtree qui grandit à la demande — pas de limite pratique de zone de travail. |
| Rendu vectoriel (lignes, arcs, polylignes) | ✅ | Pipeline OpenGL 3.3 (§2.3) : tessellation LOD + `glMultiDrawArrays`. |
| Système de calques | ✅ | `LayerManager` (§2.2). |
| Couleurs : index ACI + RGB true color | 🟡 | RGB true color complet en interne (`geom::Color`, panneau layers). ACI seulement une table de correspondance à 8 couleurs (`io::DxfColor`) pour l'interop DXF — pas les 255 couleurs ACI d'AutoCAD, et pas de concept "ByLayer/ByBlock" façon ACI côté modèle. |
| Types de ligne (continuous, dashed, hidden...) | 🟡 | `Layer::LineType` existe comme donnée (§2.2) mais **n'est pas utilisé par `GlRenderer`** — toutes les entités se dessinent en trait continu quel que soit le type déclaré. Déjà noté en P1. |
| Régénération dynamique (pan/zoom fluide) | ✅ | Tessellation sur thread dédié + debounce 16 ms (§2.3) — pas de blocage UI pendant pan/zoom. |
| Viewports multiples dans un même dessin | ❌ | Un seul `Viewport` par fenêtre ; pas de model space/paper space ni de vues multiples d'un même document. |
| Système UCS (User Coordinate System) | ❌ | Coordonnées toujours exprimées dans le repère monde ; pas d'origine/rotation utilisateur déplaçable. |
| Rendu 3D avec éclairage | ❌ | Hors périmètre assumé (2D par choix, §4). |
| Anti-aliasing / LOD | 🟡 | LOD complet et adaptatif (§2.3, §2.11). Anti-aliasing : MSAA global via `QSurfaceFormat` (4 échantillons, `main.cpp`) mais pas de line-smoothing dédié en shader — déjà noté en P3. |
| Mode filaire / ombré / réaliste | ❌ | Filaire uniquement (seul mode qui a un sens sans remplissage de surfaces ni 3D). Un mode "rempli" deviendrait pertinent avec Hatch (§2.17). |

### 2.16 Comparatif Interface Utilisateur

| Fonctionnalité | État | Détail |
|---|---|---|
| Barre de commande (entrée texte + historique) | 🟡 | Entrée de coordonnées fonctionnelle (§2.14). **Pas d'historique** (pas de rappel ↑/↓ des saisies précédentes, pas de fenêtre de log des commandes façon texte AutoCAD). |
| Barre d'outils principale | ✅ | Remplacée par le ruban (§2.13), qui joue ce rôle avec plus de structure — pas de barre plate séparée en plus. |
| Panneaux latéraux (propriétés, calques) | ✅ | `LayerPanel` (§2.6) et `PropertiesPanel` (nouveau, voir plus bas) tabifiés ensemble dans la même zone, comme Calques/Propriétés dans ZWCAD/AutoCAD. |
| Raccourcis clavier configurables | ❌ | Raccourcis fonctionnels (`F3`/`F7`/`F8`/`F9`/`Ctrl+Z`/...) mais codés en dur, aucune UI de remapping. |
| Menu contextuel (clic droit) | ❌ | Le clic droit dans le viewport termine/annule l'outil en cours (§2.6) — ce n'est pas un menu contextuel Qt (`QMenu` popup). Aucun menu contextuel nulle part dans l'appli. |
| Ruban style AutoCAD moderne | ✅ | `RibbonBar` (§2.13). Comparé directement à des captures de ZWCAD 2026 (référence utilisateur) — organisation par onglets/panneaux confirmée cohérente avec ce qui est prévu (Annoter→texte/cotes en §2.20, Insérer→blocs/xref/images en §2.21/§2.26, tous deux déjà en roadmap). |
| Barre d'état (coordonnées, snap, ortho, osnap) | 🟡 | Coordonnées ✅, outil actif ✅, ligne de commande ✅ (§2.6, §2.14), mode Ortho ✅ (§2.19). **Toujours pas d'indicateur d'état snap/grille visible en permanence** dans la barre (le seul retour visuel reste le marqueur ponctuel dans le viewport). |
| Palette de propriétés dynamique | ✅ | `PropertiesPanel` (`include/bcad/app/PropertiesPanel.h`) : calque (combo, éditable, détecte la sélection mixte), couleur (swatch + sélecteur + bouton "ByLayer"), infos géométriques en lecture seule selon le type (longueur, rayon, centre, sweep, **aire pour les polylignes fermées** — réutilise `geom::polygonArea`, connexion directe avec le besoin cadastre §2.28). Édition avec undo (`SetLayerCommand`/`SetColorOverrideCommand`, nouveaux). Se rafraîchit sur le nouveau signal `Viewport::selectionChanged` plutôt que sur `Document::onChanged`/`LayerManager::onChanged` (déjà pris par `Viewport`/`LayerPanel` — ce sont des callbacks mono-abonné, pas des signaux Qt ; les toucher aurait cassé le rafraîchissement existant. Dette technique notée : passer ces callbacks en multi-abonné serait la vraie correction). |
| Tooltips et aide contextuelle | 🟡 | Les boutons du ruban ont un tooltip (texte complet de l'action, utile en particulier pour Undo/Redo dont le libellé est tronqué à dessein). Pas encore d'aide contextuelle plus riche ailleurs (barre de statut, dialogues). |
| Interface personnalisable (CUI) | ❌ | Disposition du ruban/menus fixée dans le code, pas de UI de personnalisation. |
| Thèmes clair/sombre | 🟡 | Sombre fait et complet (§2.6). Pas de thème clair ni de sélecteur de thème (déjà en P2). |
| Multi-écrans / multi-fenêtres | ❌ | Une seule `MainWindow`, un seul document ouvert à la fois — pas de MDI ni de multi-fenêtrage. |

### 2.17 Comparatif Commandes de dessin / entités

| Fonctionnalité | État | Détail |
|---|---|---|
| Ligne (LINE) | ✅ | `LineEntity` + outil (§2.6). |
| Polyligne (POLYLINE) | ✅ | `PolylineEntity` + outil. |
| Cercle (CIRCLE) — 6 méthodes | 🟡 | `CircleEntity` existe et l'outil couvre **1 méthode** (centre + rayon, 2 clics). Les 5 autres méthodes AutoCAD (centre+diamètre, 2 points, 3 points, tangente-tangente-rayon, tangente-tangente-tangente) ne sont pas exposées comme variantes d'outil — la géométrie sous-jacente (`CircleEntity`) les supporterait toutes, il manque juste les workflows de saisie. |
| Arc (ARC) | 🟡 | `ArcEntity` existe, l'outil couvre **1 méthode** (centre + point de départ + point de fin, 3 clics). AutoCAD en propose une dizaine (3 points, début-centre-fin, début-centre-angle, début-fin-rayon, etc.) — même remarque que Circle : c'est un manque de workflows d'outil, pas de moteur géométrique. |
| Rectangle (RECTANGLE) | ❌ | Pas d'outil dédié. Un rectangle se dessine aujourd'hui "à la main" avec l'outil Polyligne (4 clics + fermeture) — fonctionnellement possible mais sans le raccourci 2-coins. |
| Saisie de coordonnées (absolues, relatives, polaires) | ✅ | §2.14 — les trois formes sont couvertes. |
| Ellipse (ELLIPSE) | ❌ | Pas de `EllipseEntity`. |
| Spline (SPLINE) | ❌ | Déjà noté en P2 ("Courbes de Bézier/NURBS/splines"). |
| Hachures (HATCH) — motifs ANSI, utilisateur | ❌ | Pas d'entité Hatch ni de bibliothèque de motifs. La triangulation contrainte (§2.1) donnerait la base géométrique d'un remplissage solide, mais aucun motif ni GUI dessus. |
| Point (POINT) avec styles | 🟡 | `PointEntity` existe dans le moteur géométrique (tessellation, distanceTo, DXF/SQLite I/O tous supportés) mais **aucun outil GUI ne le place**, et pas de "styles" de point (façon `PDMODE` d'AutoCAD : croix, cercle, point plein...). Entité prête, juste pas branchée à l'UI. |
| Région (REGION) | ❌ | Pas de type d'entité "région" formel (aire plane bornée par des courbes fermées) distinct d'une polyligne fermée. Les opérations booléennes (§2.12) produisent des `PolylineEntity`, pas un type Region dédié. |
| Tableau (TABLE) | ❌ | Pas implémenté — dépendrait d'abord d'un moteur de texte (déjà en P2). |
| Nuage de points (POINT CLOUD) | ❌ | Pas implémenté — pipeline de données et de rendu entièrement différent (nuages de millions de points, souvent avec un format binaire dédié) ; disproportionné par rapport au reste du projet à ce stade. |
| Ligne multiple (MLINE) | ❌ | Pas implémenté — entité multi-lignes parallèles offset, distincte de la polyligne. |

### 2.18 Comparatif Outils de modification

Vérifié directement dans le code le 2026-07-20.

| Fonctionnalité | État | Détail |
|---|---|---|
| Sélection fenêtre/croisement/dernière/tout | ✅ | Rubber-band dans l'outil Select : glisser gauche→droite = fenêtre (entités entièrement contenues), droite→gauche = croisement (tout ce qui est touché), distinction visuelle (bleu plein / vert pointillé) — convention AutoCAD standard. `Shift`/`Ctrl` + clic ajoute à la sélection (ou la retire si déjà sélectionnée). `Viewport::selectAll`/`selectLast` (menu Edit, `Ctrl+A` pour tout). |
| Déplacer (MOVE) | ✅ | Outil Move + `TransformEntityCommand` (§2.6, §2.9). |
| Copier (COPY) | ✅ | `ToolMode::Copy` (§3 P1) : base + destination, clone chaque entité sélectionnée via `AddEntityCommand`. |
| Effacer (ERASE) | ✅ | `Viewport::deleteSelected` (§2.9). |
| Annuler/Rétablir (UNDO/REDO) | ✅ | `QUndoStack` (§2.9) — déjà fait, critique et couvert. |
| Rotation (ROTATE) | ✅ | `ToolMode::Rotate` (§3 P1) : 3 clics (pivot, référence, cible), angle relatif appliqué à la sélection courante. |
| Mise à l'échelle (SCALE) | ✅ | `ToolMode::Scale` (§3 P1) : 3 clics (base, référence, cible), facteur = ratio des deux distances. |
| Miroir (MIRROR) | ✅ | `ToolMode::Mirror` (§3 P1) + `Transform2D::mirrorAcrossLine` (ligne arbitraire, testé) ; non destructif par défaut comme AutoCAD. |
| Décalage (OFFSET) | ❌ | Déjà noté en P2. |
| Ajuster/Prolonger (TRIM/EXTEND) | ✅ *(lignes uniquement)* | `ToolMode::Trim`/`Extend` : Trim coupe la ligne cliquée à la plus proche intersection réelle avec le reste du document (`geometry::entityIntersections`) ; Extend prolonge l'extrémité la plus proche du clic jusqu'à la première entité que croiserait son prolongement (sondé via un segment tendu très loin plutôt qu'un nouveau calcul d'intersection de droite infinie). Polylignes/arcs pas encore supportés (message explicite si on clique dessus). |
| Congé/Chanfrein (FILLET/CHAMFER) | ❌ | Pas implémenté. |
| Décomposer (EXPLODE) | ✅ *(cas simple)* | `Viewport::explodeSelected` (§3 P1) éclate une `PolylineEntity` en `LineEntity` individuelles. Le cas général (éclater un bloc) dépend toujours des blocs (§2.21), pas encore faits. |
| Copier en multiple / Matrice (ARRAY) | ❌ | Déjà noté en P2 ("réseau"). |
| Étirer (STRETCH) | ❌ | Pas implémenté — nécessite une sélection par grips/fenêtre (désormais possible depuis la ligne 1 ci-dessus) pour ne déplacer qu'une partie des sommets d'une entité — reste un chantier à part (édition de sommets individuels). |
| Jointure (JOIN) | ✅ *(lignes uniquement)* | `Viewport::joinSelected` : chaîne les `LineEntity` sélectionnées dont les extrémités se touchent (tolérance de pick) en une ou plusieurs `PolylineEntity`, une macro undo. `ArcEntity` volontairement exclu — `PolylineEntity` n'a pas de segments courbes (bulges), les inclure aplatirait leur courbure silencieusement. |
| Coupe (BREAK) | ✅ *(lignes + polylignes ouvertes)* | Scinde l'entité cliquée en deux au point le plus proche du clic sur le segment concerné. Polylignes fermées explicitement rejetées (message) — nécessiteraient deux points de coupure pour rester cohérentes, pas encore fait. |

### 2.19 Comparatif Accrochages (snaps) avancés

| Fonctionnalité | État | Détail |
|---|---|---|
| Endpoint, Midpoint, Center | ✅ | §2.8. |
| Intersection, Perpendicular | ✅ | §2.8. |
| Grid snap | ✅ | §2.11. |
| Ortho mode (`F8`) | ✅ | `Viewport::toggleOrtho`/`orthoEnabled_` : contraint à l'axe dominant (X ou Y) depuis la référence active ; snap objet toujours prioritaire, comme AutoCAD. |
| Quadrant, Nearest | ✅ | Ajoutés à `SnapType`. **Nearest** réutilise `geometry::perpendicularFoot(entity, cursor, cursor)` — le point le plus proche et le pied de la perpendiculaire depuis ce point sont la même construction. **Quadrant** : points à 0°/90°/180°/270° sur cercle/arc. |
| Tangent | ❌ | Toujours pas fait — plus cher que Nearest/Quadrant (résoudre la droite tangente à un cercle passant par le point de référence, un vrai calcul géométrique nouveau). |
| Apparent intersection | ❌ | Intersection des entités **prolongées** (au-delà de leurs points réels), pas seulement de leur géométrie réelle comme le fait `entityIntersections` (§2.8) aujourd'hui. |
| Extension, Parallel | ❌ | Pas fait. "Extension" (accroche sur le prolongement virtuel d'une ligne/d'un arc) et "Parallel" (accroche qui garde le segment en cours parallèle à une entité existante) demandent tous deux un état de survol préalable façon AutoCAD (survoler l'entité de référence avant de chercher le point), pas juste une requête ponctuelle comme le `SnapEngine` actuel. |
| Object Snap Tracking | ❌ | Pas fait — lignes de guidage temporaires depuis un ou plusieurs points d'accroche déjà identifiés. |
| Polar tracking | ❌ | Pas fait — guides d'alignement à des angles fixes (0/45/90°...) depuis le point de référence, indépendamment de la géométrie existante (contrairement à Ortho qui ne contraint qu'à 0°/90°). |

### 2.20 Comparatif Texte, cotes et annotations

Rien de cette liste n'est implémenté — le projet n'a encore aucun concept de
texte. Détaillé ici pour remplacer l'entrée générique "Texte/Cotations" de
l'itération précédente par la liste réelle des sous-fonctionnalités
d'AutoCAD, utile pour prioriser une fois le texte de base commencé.

| Fonctionnalité | État | Détail |
|---|---|---|
| Texte simple (TEXT/DTEXT) | 🟡 | `TextEntity` existe dans le moteur (tessellation, DXF I/O, round-trip) mais **aucun outil GUI ne place de texte**. Prérequis de tout le reste de cette section. |
| Texte multiligne (MTEXT) | ❌ | Dépend du texte simple + mise en page multi-lignes (retour à la ligne, alignement). |
| Cotes linéaires (DIMLINEAR) | 🟡 | `LinearDimensionEntity` existe (moteur, sérialisation, round-trip `.bcad`). DXF exporte en géométrie brute (pas en entité `DIMENSION` native AutoCAD). Pas de GUI. |
| Cotes alignées (DIMALIGNED) | 🟡 | `AlignedDimensionEntity` existe — même statut que DIMLINEAR. |
| Styles de cotes (DIMSTYLE) | ❌ | Système de présentation configurable (flèches, texte, unités) — vient après que les cotes elles-mêmes existent, pas avant. |
| Cotes radiales/angulaires/diamétrales | 🟡 | `RadialDimensionEntity` (radius/diameter) et `AngularDimensionEntity` existent — même statut que DIMLINEAR. |
| Cotes coordonnées (ordinate) | ❌ | Variante liée à un système UCS (§2.15) pour avoir un sens (mesure depuis une origine définie par l'utilisateur) — dépend indirectement de l'UCS, pas fait. |
| Repères (LEADER/MULTILEADER) | ❌ | Ligne d'annotation avec flèche + texte — dépend du texte simple. |
| Annotation scale (échelles annotatives) | ❌ | Fonctionnalité avancée liée aux mises en page/viewports multiples (§2.15, pas fait) — n'a de sens qu'une fois les viewports multiples/UCS en place. |
| Tableaux annotatifs | ❌ | Cf. Tableau (§2.17), hors périmètre pour l'instant. |
| Champs (FIELDS) | ❌ | Texte dynamique calculé (dates, aires, formules) — dépend du texte simple + d'un mini-moteur d'évaluation, fonctionnalité avancée. |

### 2.21 Comparatif Blocs et références externes

| Fonctionnalité | État | Détail |
|---|---|---|
| Création de blocs (BLOCK) | ❌ | Pas de type "définition de bloc" dans `geometry/` ni `core::Document` — remplace l'entrée générique "Blocs/groupes" de l'itération précédente par le détail réel. |
| Insertion de blocs (INSERT) | ❌ | Dépend de BLOCK. |
| Attributs de blocs (ATTDEF/ATTEDIT) | ❌ | Dépend de BLOCK + Texte (§2.20). |
| Blocs dynamiques (paramètres + actions) | ❌ | Fonctionnalité avancée d'AutoCAD (contraintes + règles sur une définition de bloc) — dépend de BLOCK et, indirectement, du système de contraintes déjà classé hors périmètre (§4). |
| Bibliothèques de blocs (palettes) | ❌ | UI de parcourir/glisser-déposer des blocs prédéfinis — dépend de BLOCK + d'un panneau dédié (§2.16). |
| Références externes (XREF) | ❌ | Pas fait. Signalé "critique pour projets" par l'utilisateur — c'est vrai en contexte pro multi-fichiers/multi-utilisateurs, mais bcad est aujourd'hui un outil mono-document, mono-utilisateur : XREF n'apporte de valeur qu'une fois un vrai flux multi-fichiers en jeu. À reconsidérer sérieusement si/quand ce besoin apparaît concrètement, pas avant BLOCK/INSERT de base. |
| XREF overlay vs attach | ❌ | Variante de XREF (propagation ou non des xrefs imbriqués) — n'a pas de sens avant XREF lui-même. |
| Blocs paramétriques | ❌ | Cf. "blocs dynamiques" ci-dessus. |
| Outils Design Center | ❌ | Explorateur de contenu réutilisable (blocs, layers, styles) entre dessins — dépend de blocs + bibliothèques + probablement multi-documents (§2.16, P3). |

### 2.22 Comparatif I/O avancé et décision stratégique DWG

Vérifié dans le code le 2026-07-20.

| Fonctionnalité | État | Détail |
|---|---|---|
| Format natif .bcad (JSON ou binaire compressé) | ✅ *(techno différente)* | Fait, mais en SQLite plutôt que JSON/binaire compressé (`io::Database`, §2.5) — choix délibéré et déjà motivé dans `DESIGN_NOTES.md` : inspectable avec n'importe quel outil SQLite, base solide pour un futur undo-log/historique (§3 P3) sans changer de format. |
| Import/Export DXF (ASCII) | ✅ | §2.5 — sous-ensemble R2000, déjà l'interop principale du projet. |
| Sauvegarde automatique + recovery | ❌ | Rien — pas de timer d'autosave, pas de détection/récupération après crash. |
| Lecture DWG (via ODA) | ❌ | Pas fait. Voir décision stratégique ci-dessous. |
| Export PDF/SVG/PNG | 🟡 | `layout/PdfExport` existe (feuille cadastrale, plan coté). Export SVG via `Exchange.cpp`. PNG : non. L'export de la vue courante (pas de la feuille mise en page) reste à faire. |
| Versions de fichier (2018, 2021, 2024...) | 🟡 | `.bcad` porte désormais `PRAGMA user_version = 1` et refuse les versions futures inconnues ; les migrations de schéma et la détection des variantes DXF restent à faire. |
| Import IFC, STEP, IGES (3D) | ❌ | Formats d'échange 3D (BIM/CAO mécanique) — hors périmètre, le projet est 2D par choix (§4). |
| Export DWF/DWFx | ❌ | Format web propriétaire Autodesk, niche pour un outil personnel. |

**Décision stratégique DWG/ODA** (suite à la question directe de l'utilisateur,
2026-07-20) : l'adhésion à l'Open Design Alliance est la voie légale et
robuste pour du DWG en contexte commercial visant la compatibilité marché,
mais représente un coût (abonnement, conditions de redistribution, SDK
volumineux) disproportionné pour bcad en l'état — projet personnel,
mono-développeur, sans objectif commercial actuel. **Décision : rester sur
DXF comme format d'interop principal** (déjà fait, spec ouverte, couvre
l'essentiel des besoins réels), ne pas adhérer à l'ODA maintenant. Piste
intermédiaire si un besoin DWG concret apparaît avant de justifier
l'adhésion complète : appeler en sous-processus l'**ODA File Converter**
(outil gratuit DWG↔DXF en ligne de commande) plutôt que d'intégrer la SDK —
à vérifier selon leur EULA en cas de redistribution avec bcad. Revoir cette
décision si/quand un vrai besoin DWG ou une intention de commercialisation
apparaît (même logique que XREF, §2.21).

### 2.23 Comparatif Modélisation 3D (Module 10)

Vérifié dans le code le 2026-07-20 — rien de ce module n'existe, sans
surprise : `geom::Kernel` est `CGAL::Exact_predicates_inexact_
constructions_kernel` en **2D** (`Point_2`, `Aff_transformation_2`...), et
tout le reste (`Entity`, `Camera2D`, `GlRenderer`, `Quadtree`) est construit
dessus. Ce n'est pas une liste de fonctionnalités manquantes au même sens
que les précédentes — **c'est un changement de nature du produit.**

| Fonctionnalité | État | Détail |
|---|---|---|
| Primitives 3D (boîte, sphère, cylindre, cône, tore) | ❌ | Demanderait un `Kernel3` (`CGAL::Simple_cartesian<double>` ou noyau 3D dédié) et une famille d'entités entièrement nouvelle — rien de réutilisable depuis `geometry::Line/Circle/Arc`. |
| Extrusion, révolution, balayage (sweep) | ❌ | Dépend des primitives 3D + d'un noyau B-Rep 3D (§2.10 : bcad n'a même pas de B-Rep 2D). |
| Opérations booléennes 3D | ❌ | CGAL a bien un module 3D (`Nef_polyhedron_3`, `Polygon_mesh_processing`) mais c'est un moteur différent de `Boolean_set_operations_2` (§2.1) — pas une extension, un second moteur à intégrer. |
| Orbite 3D (3DORBIT) | ❌ | Nécessite une caméra 3D (perspective, quaternions/matrices de vue) — `Camera2D` est structurellement 2D (pan/zoom orthographique, pas de notion de profondeur). |
| Surfaces NURBS | ❌ | Distinct des splines 2D déjà notées en P2 (§2.17) — les NURBS 3D sont un sujet à part entière. |
| Subdivision surfaces | ❌ | Technique de modélisation organique (type Catmull-Clark) — aucun équivalent dans le projet. |
| Maillages (meshes) | ❌ | Pas de structure de données maillage (vertices/faces/normales) — `Entity::tessellate` produit des polylignes 2D pour l'affichage, pas des maillages. |
| Historique paramétrique (type Fusion 360) | ❌ | Équivaut à un solveur de contraintes + un graphe de features réévaluable — déjà classé hors périmètre pour la version 2D (§4, comparaison FreeCAD/Sketcher dans `DESIGN_NOTES.md`) ; s'appliquerait autant sinon plus en 3D. |

**Avis direct** : ce module n'est pas un P3 "gros chantier" comme UCS ou les
viewports multiples — c'est une **refonte complète du moteur géométrique,
du pipeline de rendu et de la caméra**, comparable à repartir sur un second
projet qui partagerait peut-être l'UI (layers, undo, DXF) mais rien du
cœur géométrique actuel. Voir §4.

### 2.24 Comparatif Rendu & Visualisation 3D (Module 11)

Entièrement dépendant du Module 10 (§2.23) — sans géométrie 3D, aucun de ces
points n'a de sens indépendamment.

| Fonctionnalité | État | Détail |
|---|---|---|
| Moteur de rendu raytracing (Mental Ray/V-Ray-like) | ❌ | Pipeline entièrement différent du rasterizer OpenGL actuel (`GlRenderer`, §2.3) — BVH de triangles, tracé de rayons, souvent sur GPU via compute shaders/OptiX/Vulkan RT. |
| Matériaux PBR | ❌ | Suppose des surfaces 3D texturées/shadées — rien d'équivalent en 2D filaire. |
| Éclairage (soleil, artificiel, IES) | ❌ | Idem — pas de notion de lumière dans un pipeline 2D filaire sans profondeur. |
| Caméras et animations | ❌ | `Camera2D` n'a ni profondeur ni notion de trajectoire/keyframes. |
| Rendu temps réel (path tracing GPU) | ❌ | Version GPU-accélérée du raytracing ci-dessus — même dépendance au Module 10. |

### 2.25 Comparatif Automatisation & Scripting (Module 12)

Contrairement aux modules 10/11, **celui-ci reste compatible avec un outil
2D** — aucune dépendance à de la 3D. Certains points ont même un socle déjà
en place dans bcad.

| Fonctionnalité | État | Détail |
|---|---|---|
| Langage de script natif (Lisp-like ou Lua) | ❌ | Pas fait, mais légitime : `DESIGN_NOTES.md` note déjà que QCAD pousse toute sa logique d'outils dans une couche ECMAScript par-dessus un noyau C++ générique — le même schéma s'appliquerait à bcad. Gros morceau tout de même (interpréteur + API exposée). |
| Macros et scripts .scr | ❌ | Plus simple que ci-dessus : rejouer une séquence de commandes textuelles. Pourrait s'appuyer sur le parseur de saisie clavier déjà écrit (§2.14) plutôt que d'inventer un nouveau format. |
| API de plugins (C++/C#) | ❌ | Pas fait — nécessiterait de stabiliser une frontière API/ABI publique, aujourd'hui tout est interne (`bcad_core`, `bcad_render`... sont des libs statiques, pas une API de plugin). |
| Intégration Python | ❌ | Pas fait, mais **probablement le point le plus rentable de ce module** : `pybind11` sur `core::Document`/`geometry::` (déjà découplés de Qt, §1) donnerait un scripting Python sans toucher à l'UI — chemin déjà emprunté par FreeCAD. |
| Action Recorder (enregistrement de macros) | ❌ | Pas fait, mais **le moins cher de la liste** : `QUndoStack`/`Commands` (§2.9) encodent déjà chaque action comme un objet rejouable — enregistrer/exporter la séquence de commandes poussées sur la pile est une extension naturelle, pas une nouvelle architecture. |
| .NET API complète (compatibilité AutoCAD) | ❌ | Sans objet : bcad n'est pas et ne vise pas à être un plugin-host compatible ObjectARX/.NET d'AutoCAD — cette ligne n'a de sens que pour un concurrent visant l'écosystème de plugins AutoCAD existant. |
| COM/ActiveX (legacy) | ❌ | Techno Windows historique, hors sujet pour une appli Qt multiplateforme construite Linux-first dans ce projet. |

### 2.26 SIG (Map 3D) et fonctionnalités collaboratives

| Fonctionnalité | Priorité indiquée | État | Détail |
|---|---|---|---|
| SIG / données géospatiales (BCAD Map 3D) | — | ❌ | **Mise à jour 2026-07-20** : initialement classé hors périmètre ci-dessous (§4) faute de besoin concret identifié. L'utilisateur a confirmé qu'un usage cadastre/géomètre est un vrai axe pour bcad — voir §2.28 pour le détail (CRS/PROJ, parcelles, topologie, import de levés) et §3/§4 pour la reclassification en priorités réelles. Le SIG "complet" façon BCAD Map 3D (couches raster, très gros volumes, écosystème PostGIS) reste, lui, hors périmètre — la distinction importante est entre "outils cadastre/géomètre ciblés" (maintenant réels) et "plateforme SIG généraliste" (toujours hors périmètre, personne ne l'a demandé). |
| Verrouillage de fichiers (xLock) | 🟠 P1 (utilisateur) | ❌ | **Désaccord avec la priorité indiquée** : xLock n'a de sens que dans un contexte de fichier partagé sur un serveur/réseau accédé par plusieurs personnes — bcad est mono-utilisateur, mono-machine, sans aucune notion de stockage partagé. Zéro valeur tant que ce contexte n'existe pas ; le classer P1 reviendrait à construire une serrure pour une porte qui n'existe pas encore. Voir §3/§4. |
| Édition simultanée (type Google Docs) | 🟡 P2 (utilisateur) | ❌ | Nécessite un serveur, un protocole de synchronisation (OT/CRDT) et une gestion de conflits — infrastructure multi-utilisateur complète, plusieurs ordres de grandeur au-dessus du reste du projet. |
| Comparaison de dessins (DWG COMPARE) | 🟡 P2 (utilisateur) | ❌ | Le plus faisable de ce groupe : comparer deux `Document` (deux fichiers `.bcad`/DXF) entité par entité et surligner les différences ne demande ni serveur ni multi-utilisateur — juste un algorithme de diff géométrique. Détaché du reste du bloc collaboratif ci-dessous en termes de coût. |
| Commentaires / markups | 🟡 P2 (utilisateur) | ❌ | Faisable sans infra serveur (annotations attachées au document, pas de la géométrie) mais dépend du texte (§2.20, lui-même pas commencé). |
| Intégration cloud (BCAD Cloud) | 🟡 P2 (utilisateur) | ❌ | Nécessite un service backend (comptes, stockage, sync) — hors périmètre d'un outil desktop personnel. |
| Versioning (git-like pour DWG) | 🟡 P2 (utilisateur) | ❌ | Version light déjà notée en P3 ("Undo-log/historique dans le format `.bcad`", §3) — un vrai système façon git (branches, diff binaire) est un chantier bien plus lourd que ce qui est déjà prévu. |

### 2.27 Comparatif Impression & Mise en page (Module 15)

Vérifié dans le code le 2026-07-20 — rien de ce module n'existe (pas de
`QPrinter`/`QPrintDialog`, pas de notion de layout/espace papier).

| Fonctionnalité | Priorité indiquée | État | Détail |
|---|---|---|---|
| Espace papier (layouts) | 🔴 P0 (utilisateur) | ❌ | Recoupe "viewports multiples dans un même dessin" déjà noté en P3 (§2.15, §3) — `core::Document` suppose aujourd'hui un espace de dessin unique, pas plusieurs feuilles nommées. |
| Viewports flottants avec échelles | 🔴 P0 (utilisateur) | ❌ | **Dépend entièrement** de l'espace papier ci-dessus — ne peut pas être une priorité indépendante, techniquement un sous-produit du même chantier. |
| Styles de tracé (CTB/STB) | 🔴 P0 (utilisateur) | ❌ | Contrôle l'apparence à l'impression (couleur/épaisseur par style) — **n'a de sens qu'une fois qu'il existe une impression** ; rien à styliser avant que le pipeline d'export existe. |
| Configuration de traceurs/imprimantes | 🔴 P0 (utilisateur) | ❌ | Le plus faisable des quatre "P0" indiqués : Qt fournit `QPrinter`/`QPrintDialog` prêts à l'emploi pour le dialogue d'impression et la sélection d'imprimante — pas un gros chantier en soi, contrairement aux trois autres lignes P0. |
| Tracé par lots (batch plot) | 🟠 P1 (utilisateur) | ❌ | Dépend des layouts (imprimer plusieurs feuilles d'affilée n'a pas de sens sans plusieurs feuilles). |
| Publication (PUBLISH) multi-fichiers | 🟠 P1 (utilisateur) | ❌ | Idem — variante "export PDF multi-page" du batch plot, même dépendance aux layouts. |
| eTransmit (packaging de projet) | 🟡 P2 (utilisateur) | ❌ | Empaquette un dessin + ses dépendances (xrefs, polices, images) — dépend directement de XREF (§2.21), déjà classé P3/à reconsidérer selon besoin, ce qui repousse eTransmit d'autant. |

**Sur les priorités indiquées** : les quatre lignes marquées P0 ne sont pas
indépendantes entre elles — trois des quatre (viewports flottants, styles
de tracé, et dans une moindre mesure la config imprimante) **dépendent**
techniquement de la première (espace papier), qui est elle-même un gros
chantier déjà identifié en P3 (§2.15, model/paper space). Les traiter comme
4 P0 parallèles donnerait l'impression qu'elles sont indépendamment
attaquables, ce qui n'est pas le cas. Le vrai premier pas bon marché pour
"la sortie papier" reste ce qui est **déjà noté en P2** : un export PDF/SVG
simple de la vue courante (§3) via `QPrinter`/`QPdfWriter` — sans layout ni
échelle de viewport, juste "imprimer ce qu'on voit". C'est ce qui débloque
le plus de valeur pour le moins d'effort ; les layouts/CTB/batch plot ne
prennent tout leur sens qu'après.

### 2.28 Module Cadastre / Géomètre — axe confirmé (2026-07-20)

Contrairement aux modules précédents, **celui-ci reflète un vrai besoin
d'usage** (confirmé par l'utilisateur), pas un exercice de comparaison —
donc pas de statut "hors périmètre" par défaut ici. Ça touche directement
le SIG déjà noté en §2.26 : cette section-là est mise à jour en
conséquence (voir la note en fin de §2.26).

| Fonctionnalité | État | Détail |
|---|---|---|
| Calculs double étendu / BigDecimal | 🟡 *(à nuancer)* | Le kernel CGAL (`Exact_predicates_inexact_constructions_kernel`, §2.1) utilise des `double` pour les constructions. **BigDecimal n'est probablement pas le bon correctif** : un `double` garde ~15-17 chiffres significatifs, largement suffisant pour du sub-millimétrique même à l'échelle d'un système de coordonnées projeté (coordonnées à 6-7 chiffres avant la virgule, ex. UTM/Lambert) — le vrai risque de précision en topométrie n'est pas le type numérique mais l'accumulation d'erreur quand on manipule directement de grandes coordonnées brutes sans origine locale. Un `Document` avec une **origine locale (fausse origine)** soustraite pour l'affichage/les calculs réglerait le vrai problème sans payer le coût CPU de BigDecimal ni remettre en cause le choix de kernel déjà motivé (§2.1). |
| Unité native (mm/cm) | ❌ | Pas de concept d'unité aujourd'hui — les coordonnées sont des `double` nus, sans étiquette. Nécessaire : un `Document::unit` (enum ou facteur d'échelle) qui pilote l'affichage (coordonnées, futures cotes) sans changer la représentation interne. |
| Tolérance de snapping configurable | 🟡 | Le mécanisme existe (`SnapEngine`, §2.8) mais la tolérance est un `constexpr` en pixels écran (`kSnapToleranceScreenPx`, `Viewport.cpp`), pas un réglage utilisateur en unités réelles (ex. 0.01 m). À exposer comme préférence, et envisager une tolérance *aussi* exprimée en unités monde pour les usages où la précision légale prime sur le confort visuel au zoom. |
| Intégration PROJ (CRS) | ❌ | Aucune dépendance PROJ, aucune notion de système de coordonnées de référence sur `Document`. Nouveau sous-système à part entière : bibliothèque externe (`libproj-dev`), état "CRS du document" (ex. code EPSG), conversion à la demande projeté↔géographique pour l'affichage. |
| Projections locales (Lambert 93, UTM) | ❌ | Dépend de PROJ ci-dessus — PROJ les fournit nativement une fois intégré, pas de travail spécifique par projection. |
| Affichage Lat/Lon + projeté | ❌ | Dépend de PROJ — extension de la barre de statut (§2.6) qui affiche déjà X/Y en continu. |
| Création de polygones fermés (parcelles) | ✅ | Couvert génériquement par `PolylineEntity` fermée (§2.1) — pas de sémantique "parcelle" dédiée (numéro, propriétaire...) mais la géométrie de base est là. |
| Calcul de surface légale (Gauss/shoelace) | ✅ | **Déjà fait** : `geom::polygonArea` (`BooleanOps.cpp`) délègue à `CGAL::Polygon_2::area()`, qui *est* la formule du lacet — testé (`smoke_test.cpp`). Manque juste un affichage dans une palette de propriétés (§2.16, déjà en P1) pour être exploitable directement par un géomètre. |
| Lotissement (division d'une parcelle par une ligne) | ❌ | Pas d'outil dédié, mais **construisible avec l'existant** : `geometry::entityIntersections` (§2.8/§2.19) trouve déjà les points de coupe polygone/ligne ; il manque la découpe en deux polygones de part et d'autre (variante de `geometry::BooleanOps`, §2.1, avec les deux demi-plans définis par la ligne). |
| Réunion (fusion de parcelles) | ✅ | **Déjà fait** : `Viewport::booleanOperation(BooleanOp::Union)` (§2.12), directement applicable à deux parcelles adjacentes. |
| Validation "pas de chevauchement" | 🟡 | Constructible avec l'existant pour la détection (intersection non vide entre polygones d'un même calque via `geometry::BooleanOps`, §2.1) — juste une passe de validation à écrire, pas un nouveau moteur. |
| Validation "pas d'espace vide" | ❌ | Plus dur : vérifier qu'un ensemble de parcelles couvre entièrement une zone de référence sans trou est un problème de couverture, pas juste des tests d'intersection par paires. |
| Snapping auto aux sommets de parcelles | ✅ | Déjà couvert génériquement par le snap Endpoint (§2.8) — aucune parcelle n'est traitée différemment d'une autre polyligne fermée. |
| Import de points de levé (CSV/TXT : Num,X,Y,Z,Code) | ❌ | Pas fait, mais **le point le plus rentable de tout ce module** : ne dépend ni de PROJ ni d'aucun autre prérequis (les exports de stations totales sont déjà en coordonnées projetées) — un parseur CSV + une boucle `PointEntity`/`Document::addEntity` suffit. `Z` n'a pas de sens en 2D (à stocker comme attribut, pas comme coordonnée) ; `Code` se prêterait bien à un layer ou une propriété d'entité. |
| Export Shapefile (.shp) / GeoPackage | ❌ | Pas fait. Synergie à noter : GeoPackage est un format **SQLite** avec un schéma normalisé — le projet a déjà tout l'outillage SQLite (`io::Database`, §2.5), donc un export GeoPackage est plus proche d'une variante de schéma que d'un nouveau sous-système. Shapefile est un format plus ancien/plus simple à écrire en minimal, comparable en effort à l'écrivain DXF déjà fait (§2.5). |
| Export DXF/DWG | ✅ / ❌ | DXF déjà fait (§2.5). DWG : cf. décision ODA (§2.22), inchangée par ce module. |

### 2.29 Module Cadastre/Géomètre — suite (attributs, GDAL, correction, habillage, cloud)

Vérifié dans le code le 2026-07-20. Modules 6–9 confirmés comme faisant
partie de l'axe cadastre réel (§2.28) ; module 10 (intégration Django)
confirmé comme **projection long terme**, pas un système existant à
intégrer maintenant — reste séparé et rejoint la position déjà prise en
§2.26 sur le cloud/multi-utilisateur.

| Fonctionnalité | État | Détail |
|---|---|---|
| Gestion des attributs (ID_Parcelle, Propriétaire, Surface_Calculée, Nature_Culture, Section...) | ❌ | Aucun système d'attributs aujourd'hui : `Entity` n'a que `id`/`layer`/`colorOverride`/`selected` (`Entity.h`) — pas de table clé-valeur ni de champs métier. Item réel et contenu : soit une map générique `string→string` sur `Entity` (simple, flexible), soit des champs dédiés pour un futur type `ParcelleEntity` (plus structuré, moins générique). `Surface_Calculée` réutiliserait directement `geom::polygonArea` déjà fait (§2.28). |
| GDAL/OGR pour GeoJSON/KML/GML/MapInfo | ❌ | Pas de dépendance GDAL. **Reconsidère les items déjà notés** "Export Shapefile"/"Export GeoPackage" (§2.28, P2) : GDAL/OGR est une bibliothèque bien plus lourde que PROJ seul (gros build, beaucoup de formats embarqués), mais couvre GeoJSON/KML/GML/MapInfo **et** Shapefile en une seule dépendance plutôt que d'écrire un lecteur/écrivain par format à la main — même arbitrage que DXF-maison vs `libdxfrw` déjà discuté pour le DWG (`DESIGN_NOTES.md`). Décision à prendre au moment de l'implémentation : commencer par un ou deux formats écrits à la main (Shapefile, GeoPackage via SQLite déjà en place) et ne basculer sur GDAL que si le nombre de formats à couvrir le justifie. |
| Outil d'ajustement de limite (déplace une borne, recalcule les surfaces adjacentes) | ❌ | **Le plus dur de ce lot** : suppose une vraie topologie planaire (un sommet partagé entre deux parcelles voisines, déplacé une seule fois pour les deux). Le modèle actuel n'a pas ça — chaque `PolylineEntity` est indépendante ; déplacer un sommet avec Move/futur Stretch (§2.18) ne touche pas la parcelle voisine, qui se retrouve désolidarisée. C'est un vrai chantier de modèle de données (nœuds/arêtes partagés entre entités), pas juste un outil de plus. |
| Fermeture automatique de polygones | ❌ | Beaucoup plus simple que la ligne au-dessus : détecter qu'une polyligne presque fermée (premier/dernier point à moins d'une tolérance) doit l'être, et le faire automatiquement à la validation/l'import. Indépendant du reste de ce tableau. |
| Cartouches, échelles graphiques, flèche du Nord, légendes dynamiques | ❌ | Dépend en cascade de prérequis déjà différés : un cartouche est essentiellement un bloc (§2.21, pas fait) contenant du texte (§2.20, pas fait) ; une échelle graphique *dynamique* liée au viewport suppose l'espace papier (§2.27, P3). Version statique/dégradée possible plus tôt : un bloc de cartouche fixe placé directement en espace modèle, sans liaison dynamique à une échelle de viewport — mais reste après blocs + texte de base. |
| Intégration Django (API REST, multi-tenant) | ❌ | **Confirmé comme projection long terme**, pas un système à intégrer maintenant — cohérent avec la position déjà prise sur le cloud/multi-utilisateur (§2.26/§4). Reste hors périmètre tant qu'un vrai backend concret n'existe pas côté utilisateur. |
| Audit logging (qui a fait quoi, quand) | 🟡 | La version "multi-tenant vers un serveur" dépend de Django ci-dessus (hors périmètre pour l'instant). Mais une **version locale** est presque gratuite : `QUndoStack`/`Commands` (§2.9) décrivent déjà chaque action ; journaliser leur description + horodatage (+ utilisateur OS, à défaut de vrais comptes) dans une table du `.bcad` ou un fichier journal capturerait une bonne partie de la valeur sans dépendre d'un backend. |

### 2.30 Modules 11–15 — différenciation appels d'offres publics

Vérifié dans le code le 2026-07-20 (rien de ce lot n'existe : pas de GEOS,
PostGIS, SpatiaLite, CityGML/IndoorGML, ni de client réseau
`QNetworkAccessManager`/WMS/WFS). Cadrage explicite de l'utilisateur cette
fois : viser les appels d'offres publics (cadastre/géomètre, contexte
France sous-entendu par la référence au PCI vecteur). **Avis stratégique
avant le tableau** : tous les items n'ont pas le même poids dans un vrai
dossier de réponse à appel d'offres — un jury regarde surtout la conformité
réglementaire et l'interopérabilité avec le référentiel officiel, beaucoup
moins les technologies exotiques (3D volumétrique). J'ai noté ce jugement
dans la colonne "Détail" plutôt que de tout aligner au même niveau de
priorité.

| Fonctionnalité | État | Détail |
|---|---|---|
| Moteur topologique avancé (GEOS/PostGIS, règles de plan cadastral national) | ❌ | GEOS et CGAL (§2.1, déjà en place) se recoupent largement pour les primitives topologiques 2D (booléens, intersections) — adopter GEOS en plus n'apporterait pas de capacité nouvelle ici, ce serait une deuxième bibliothèque pour faire ce que CGAL fait déjà. **Le vrai gap, ce sont les règles métier** ("plan cadastral national" = conventions françaises précises : tolérances de fermeture, numérotation des parcelles, seuils de surface...) — un moteur de règles à écrire par-dessus les primitives déjà là (`BooleanOps`, validation de chevauchement déjà en P1/P2), pas une nouvelle dépendance géométrique. |
| Cadastre 3D (volumes, CityGML/IndoorGML) | ❌ | En tension directe avec le choix 2D du projet (§4, §2.23). **Probablement prématuré pour l'objectif appels d'offres** : le cadastre 3D volumétrique reste expérimental même dans les pays qui le pilotent (Pays-Bas, pays nordiques) — la quasi-totalité des appels d'offres cadastraux français portent sur le PCI vecteur, qui est 2D. À garder en tête si un dossier précis l'exige explicitement, pas à anticiper. |
| PostgreSQL/PostGIS ou SpatiaLite | ❌ | Deux poids très différents. **SpatiaLite** = extension spatiale de SQLite : `io::Database` (§2.5) est déjà du SQLite pur, donc charger `mod_spatialite` et ajouter des colonnes géométriques est une extension naturelle et peu coûteuse. **PostgreSQL/PostGIS** = vraie base client-serveur (réseau, identifiants, pooling de connexions) — pertinent si l'objectif est un déploiement institutionnel multi-utilisateur (cohérent avec "appels d'offres publics"), mais un chantier bien plus lourd, à ne faire qu'une fois le besoin de données centralisées confirmé (même logique que Django, §2.29). |
| Génération de rapports (fiche de renseignement de parcelle, PDF sommets/distances/surfaces conforme aux normes notariales) | ❌ | **Probablement l'item le plus rentable de tout ce lot pour l'objectif visé** : toute la donnée nécessaire existe déjà (sommets = `PolylineEntity::vertices()`, distances = `geom::distance`, surface = `geom::polygonArea` déjà fait, §2.28) — il ne manque qu'une couche de mise en page de rapport (`QPdfWriter`/`QTextDocument`) par-dessus, pas de nouveau calcul géométrique. C'est aussi le genre de livrable concret qu'un jury d'appel d'offres évalue directement. |
| Flux WMS/WFS des cadastres nationaux (PCI vecteur) | ❌ | Deux briques de coût différent. **WMS** (fond de plan raster) : le plus simple — récupérer des tuiles image via HTTP (`QNetworkAccessManager`, déjà dans Qt) et les afficher comme calque de fond, pas de parsing complexe. **WFS** (import vectoriel réel des parcelles officielles) : plus lourd, suppose un parseur GML — mais c'est la fonctionnalité qui permet de superposer/comparer un plan avec le référentiel officiel, ce qui est directement pertinent pour un usage géomètre/administration. Aucune des deux n'existe : le projet n'a aujourd'hui **aucun client réseau**, aucune notion d'I/O autre que fichiers locaux. |

## 3. Reste à faire — priorités réorganisées

Repris après l'analyse §2.10 et §2.15–§2.17. Tiers **P0** (prochain, en
cours) → **P3** (plus tard / gros chantiers). Priorité = valeur d'usage
immédiate rapportée au coût d'implémentation, pas seulement l'impact.

### P0 — fait (dernières itérations)

- [x] ~~Système de tolérance numérique centralisé~~ (`geometry::Tolerance`,
      §2.11).
- [x] ~~Grille adaptative + snap-to-grid~~ (§2.11).
- [x] ~~Accrochage intersection et perpendiculaire~~ (§2.8).
- [x] ~~Saisie clavier de coordonnées~~ (§2.14).
- [x] ~~Booléens en commande GUI~~ (§2.12).
- [x] ~~Ruban façon AutoCAD~~ (§2.13) — révisé : le coût estimé était
      surévalué, `QTabWidget`+`QToolButton` suffisent sans bibliothèque
      externe, donc remonté de P2 et fait cette itération.

### P1 — prochaines itérations naturelles (forte valeur, coût contenu)

**Outils de modification** (§2.18) — le premier lot s'appuie sur des
primitives déjà écrites et testées (`Transform2D`, `entityIntersections`),
donc surtout du travail d'outil/UX, pas de nouveau moteur géométrique :

- [x] ~~Rotation/échelle interactives~~ — `ToolMode::Rotate`/`Scale`,
      3 clics (base, référence, cible) appliqués à la sélection courante
      via `TransformEntityCommand`, angle/facteur *relatifs* (delta entre
      référence et cible) plutôt qu'absolus, comme AutoCAD.
- [x] ~~Miroir interactif avec ligne de miroir arbitraire~~ —
      `geom::Transform2D::mirrorAcrossLine` (nouveau, testé) +
      `ToolMode::Mirror` (2 clics), **non destructif par défaut** (clone +
      transforme, garde les originaux — comportement par défaut d'AutoCAD
      MIRROR).
- [x] ~~Copier (COPY)~~ — `ToolMode::Copy`, même mécanique que Move (base +
      destination) mais clone via `AddEntityCommand` au lieu de
      transformer en place ; opère sur la sélection courante comme
      Rotate/Scale/Mirror (pas un pick dédié comme Move).
- [x] ~~Ajuster/Prolonger (TRIM/EXTEND)~~ — lignes uniquement pour
      l'instant, s'appuie sur `geometry::entityIntersections` (Trim) et un
      segment-sonde très long (Extend) plutôt qu'un nouveau calcul de
      droite infinie. "Trim/Extend contre tout le document" (pas de
      sélection préalable des arêtes de coupe) — workflow qu'AutoCAD
      propose aussi quand on ne présélectionne pas de bords coupants.
- [x] ~~Jointure (JOIN)~~ — `Viewport::joinSelected`, lignes uniquement
      (les arcs aplatiraient leur courbure dans une `PolylineEntity`, qui
      n'a pas de segments courbes — exclus explicitement plutôt que de
      produire une géométrie fausse).
- [x] ~~Coupe (BREAK)~~ — lignes et polylignes **ouvertes** ; les
      polylignes fermées sont rejetées avec un message (nécessiteraient
      deux points de coupure pour rester cohérentes).
- [x] ~~Décomposer (EXPLODE)~~, cas simple : `Viewport::explodeSelected()`
      éclate chaque `PolylineEntity` sélectionnée en `LineEntity`
      individuelles (une macro undo). Le cas "bloc" attend toujours §2.21,
      hors périmètre pour l'instant.
- [x] ~~Sélection fenêtre/croisement/tout/dernière~~ — rubber-band dans
      l'outil Select (fenêtre gauche→droite, croisement droite→gauche,
      convention AutoCAD), `Shift`/`Ctrl`+clic pour ajouter/retirer de la
      sélection, `selectAll`/`selectLast`. Copy/Rotate/Scale/Mirror
      (ci-dessus) profitent automatiquement du multi-select désormais
      disponible, sans changement de leur code — c'était déjà prévu ainsi.

**Snapping** (§2.19) — les moins chers d'abord :

- [x] ~~Nearest~~ — réutilise `geometry::perpendicularFoot(entity, cursor,
      cursor)` : le point le plus proche sur une entité et le pied de la
      perpendiculaire depuis ce même point sont la même construction
      géométrique, donc pas de nouveau calcul.
- [x] ~~Quadrant sur cercle/arc~~ — points à 0°/90°/180°/270°, filtrés par
      secteur angulaire pour les arcs (même logique que les extrema de
      `ArcEntity::boundingBox`).
- [x] ~~Mode Ortho (`F8`)~~ — contraint le point suivant à l'axe (X ou Y,
      celui avec le plus grand delta) par rapport à la référence active ;
      le snap objet garde la priorité sur Ortho (comme dans AutoCAD).

**Interface** (§2.16) :

- [x] ~~Propriétés d'entité éditables / palette de propriétés~~ —
      `PropertiesPanel` : calque + couleur éditables (avec undo), infos
      géométriques en lecture seule (dont l'aire pour les polylignes
      fermées). Épaisseur de trait pas encore éditable (dépend du rendu du
      type de trait, ci-dessous, pas encore fait). Inspiré directement de
      captures ZWCAD fournies par l'utilisateur.
- [ ] Menu contextuel (clic droit → `QMenu` selon sélection/outil actif).
- [ ] Indicateur d'état snap/grille dans la barre de statut (visible en
      permanence, pas seulement le marqueur ponctuel dans le viewport).
- [x] ~~Tooltips sur les boutons du ruban~~ — texte complet de l'action en
      tooltip (`RibbonBar::addPanel`) ; corrige au passage le bug de
      libellés tronqués signalé par l'utilisateur (voir note plus bas).
- [ ] Historique de la ligne de commande (rappel ↑/↓).

**Rendu et outils de dessin** (§2.15, §2.17) :

- [ ] Type de trait (tirets/pointillés) au rendu — le champ existe déjà
      dans `Layer::LineType`, pas encore utilisé par `GlRenderer`.
- [ ] Outil Rectangle (2 clics = coins opposés) — géométrie déjà là
      (`PolylineEntity` fermée), pur travail d'outil.
- [ ] Outil Point + styles d'affichage simples (croix/cercle/point plein) —
      `PointEntity` existe déjà côté moteur et I/O, juste pas exposé en
      GUI. Le moins cher de tous ces ajouts.
- [ ] Méthodes de construction supplémentaires pour Circle (2 points,
      3 points) et Arc (3 points) — la géométrie supporte déjà tous les
      cas, seuls les workflows de clics manquent.
- [ ] Saisie clavier : mode "distance seule pendant le drag" (taper un
      nombre pour fixer la longueur dans la direction déjà pointée à la
      souris) — §2.14 ne couvre encore que les formes explicites.

**I/O** (§2.22) :

- [ ] **Sauvegarde automatique + recovery** — le plus haut rapport valeur/
      coût de cette liste : perdre du travail non sauvegardé est le pire
      défaut possible pour un outil de dessin, et `io::Database::save`
      existe déjà (§2.5) — "juste" un timer + une détection de reprise
      après crash au démarrage (fichier `.bcad.autosave` à côté du projet,
      proposé à l'ouverture s'il est plus récent que le dernier save).
- [x] Numéro de schéma dans `.bcad` (`PRAGMA user_version` ; version courante 1)
      dédiée) — cheap, évite de casser silencieusement les fichiers
      existants le jour où le schéma SQLite change.

**Cadastre/Géomètre** (§2.28, axe confirmé) — le lot le plus rentable
d'abord, aucun ne dépend de PROJ :

- [ ] **Import CSV/TXT de points de levé** (`Num,X,Y,Z,Code`) — le point le
      plus rentable de tout le module : aucun prérequis, juste un parseur +
      `Document::addEntity` en boucle. Débloque un vrai usage géomètre
      immédiatement.
- [ ] Afficher la surface légale (`geom::polygonArea`, déjà fait) dans la
      palette de propriétés déjà prévue plus haut — quasi gratuit une fois
      la palette elle-même faite, gros gain d'usage pour un géomètre.
- [ ] Validation "pas de chevauchement" entre parcelles d'un même calque —
      construit sur `geometry::BooleanOps::Intersection` déjà en place,
      juste une passe de contrôle à écrire.
- [ ] Tolérance de snapping configurable (préférence utilisateur, unités
      réelles) — extension directe du `SnapEngine` existant (§2.8).
- [ ] Origine locale (fausse origine) sur `Document` — corrige le vrai
      risque de précision en topométrie (grandes coordonnées UTM/Lambert
      brutes) sans le coût d'un BigDecimal ni remise en cause du kernel
      CGAL déjà motivé (§2.1, §2.28).
- [ ] Outil de lotissement (division d'une parcelle par une ligne) —
      combine `entityIntersections` (déjà là) + une variante de
      `BooleanOps` par demi-plans.
- [ ] Unité native du document (mm/cm, facteur d'affichage) — pilote
      l'affichage des coordonnées/futures cotes sans changer la
      représentation interne en `double`.
- [ ] **Gestion des attributs** (map générique clé-valeur sur `Entity`,
      pour ID_Parcelle/Propriétaire/Nature_Culture/Section...) — §2.29 ;
      item contenu, réutilise `Surface_Calculée` = `geom::polygonArea`
      déjà fait. Naturellement lié à la palette de propriétés déjà en P1
      (l'un affiche/édite ce que l'autre stocke).
- [ ] Fermeture automatique de polygones (détecter une polyligne presque
      fermée à la tolérance près, la fermer) — §2.29 ; simple et
      indépendant du reste du lot cadastre.

### P2 — extensions de fonctionnalités (plus lourdes, pas bloquantes)

- [ ] Congé/Chanfrein (FILLET/CHAMFER) — §2.18 ; contrairement à
      Trim/Extend/Join/Break, demande un nouveau calcul géométrique
      (arc/segment tangent à deux entités), pas juste une combinaison de
      primitives déjà écrites.
- [ ] Étirer (STRETCH) — §2.18 ; dépend d'une sélection par grips/fenêtre
      partielle, elle-même pas encore faite.
- [ ] Réseau (ARRAY) rectangulaire/polaire, décalage (OFFSET),
      copier/coller.
- [ ] Snaps Tangent, Parallel, Extension, Apparent intersection — §2.19 ;
      demandent un état de survol préalable et/ou un nouveau calcul
      géométrique (tangente), contrairement à Nearest/Quadrant (P1).
- [ ] Object Snap Tracking, Polar tracking — §2.19 ; guides d'alignement
      temporaires, fonctionnalité UX à part entière.
- [ ] Édition de sommets d'une polyligne existante (grips).
- [ ] Texte simple (TEXT/DTEXT) → prérequis de tout le reste de §2.20
      (MTEXT, cotes, leaders, champs).
- [ ] Cotes linéaires et alignées (DIMLINEAR/DIMALIGNED) — une fois le
      texte simple en place.
- [ ] Hachures (hatch) — motifs solides d'abord (s'appuie sur la
      triangulation déjà en place), motifs ANSI/utilisateur ensuite.
- [ ] Ellipse (`EllipseEntity` + outil) — pas commencé.
- [ ] Blocs de base : définition (BLOCK) + insertion (INSERT) — §2.21 ;
      préalable à tout le reste de cette section (attributs, blocs
      dynamiques, bibliothèques, XREF).
- [ ] Région (`Region`) comme type d'entité formel, si les usages en aval
      (hachures, propriétés de surface) le justifient — sinon une
      polyligne fermée + booléens (§2.12) couvre déjà l'essentiel.
- [ ] Ligne multiple (MLINE) — niche.
- [ ] Courbes de Bézier / NURBS / splines.
- [ ] Étendre le DXF (`SPLINE`, `TEXT`/`MTEXT`, `DIMENSION`, `HATCH`,
      `INSERT`) au fur et à mesure que ces types d'entités existent.
- [ ] Fidélité couleur DXF complète (palette ACI 255 ou vraie couleur via
      group code 420) plutôt que la table à 8 couleurs actuelle.
- [ ] **Export PDF/SVG/PNG** de la vue courante — §2.27 ; entrée la plus
      rentable pour "la sortie papier" (Module 15), avant tout layout/
      échelle. Note technique : pas un simple screenshot du widget GL —
      `GlRenderer` dessine en OpenGL, donc un export vectoriel propre
      demande un second chemin de rendu par `QPainter` directement sur un
      `QPdfWriter`/`QSvgGenerator` (parcourir les entités du `Document` et
      les dessiner en 2D, comme le fait déjà `drawToolPreview`/`drawGrid`
      pour l'overlay, §2.6) plutôt que de rasteriser la sortie GL.
- [ ] Icônes pour les boutons du ruban (actuellement texte seul) ; thème
      clair ; raccourcis configurables.
- [ ] **Action Recorder** — §2.25 ; le moins cher du module scripting,
      s'appuie directement sur `QUndoStack`/`Commands` déjà en place
      (§2.9) : enregistrer/exporter la séquence de commandes poussées sur
      la pile plutôt qu'inventer une nouvelle architecture.
- [ ] **Intégration Python** (`pybind11` sur `core::Document`/`geometry::`)
      — §2.25 ; probablement le point le plus rentable du module
      scripting, chemin déjà emprunté par FreeCAD (`DESIGN_NOTES.md`) et
      rendu plus simple ici par le fait que `core`/`geometry` sont déjà
      découplés de Qt.
- [ ] Comparaison de dessins (diff géométrique entre deux `Document`) —
      §2.26 ; faisable sans infra serveur/multi-utilisateur, détaché du
      reste du bloc collaboratif.
- [ ] Commentaires/markups — §2.26 ; faisable sans serveur mais dépend du
      texte (§2.20) ci-dessus.
- [ ] **Intégration PROJ (CRS)** — §2.28 ; nouvelle dépendance externe
      (`libproj-dev`) + état "CRS du document" (code EPSG) + conversion à
      la demande — plus lourd que le lot P1 ci-dessus car ça touche une
      vraie nouvelle brique, mais contenu (pas de rearchitecture du
      moteur géométrique, juste une couche de conversion à l'affichage).
- [ ] Projections locales (Lambert 93, UTM) et affichage Lat/Lon — §2.28 ;
      vient gratuitement une fois PROJ intégré ci-dessus.
- [ ] Export GeoPackage — §2.28 ; synergie avec `io::Database` (§2.5) déjà
      en SQLite, donc plus proche d'une variante de schéma que d'un
      nouveau sous-système. **Décision d'implémentation à prendre à ce
      moment-là** (§2.29) : écrit à la main (comme ici et Shapefile
      ci-dessous) tant que 1-2 formats suffisent, bascule vers GDAL/OGR
      seulement si GeoJSON/KML/GML/MapInfo (§2.29) deviennent aussi
      nécessaires — GDAL est une dépendance nettement plus lourde que ce
      que ces deux formats seuls justifient.
- [ ] Export Shapefile (.shp) — §2.28 ; effort comparable à l'écrivain DXF
      déjà fait (§2.5).
- [ ] Habillage de plan statique (cartouche fixe, flèche du Nord) en espace
      modèle — §2.29 ; version dégradée sans échelle dynamique, mais
      dépend quand même de blocs (P2 ci-dessus) et texte de base (P2
      ci-dessus) — pas avant eux.
- [ ] Audit log **local** (horodatage + description de chaque `Command`
      déjà poussée sur `QUndoStack`, §2.9, écrite dans une table du
      `.bcad`) — §2.29 ; capture une partie de la valeur de l'audit
      logging sans dépendre du backend Django différé ci-dessous.
- [ ] Validation topologique "pas d'espace vide" — §2.28 ; plus dur que la
      détection de chevauchement (P1 ci-dessus), problème de couverture
      plutôt que de simples tests d'intersection par paires.
- [ ] **Génération de rapports** (fiche de renseignement de parcelle : PDF
      sommets/distances/surfaces) — §2.30 ; probablement l'item le plus
      rentable pour l'objectif "appels d'offres publics" — toute la donnée
      géométrique existe déjà, il ne manque qu'une mise en page de rapport
      (`QPdfWriter`/`QTextDocument`).
- [ ] Fond de plan WMS (tuiles raster du PCI vecteur ou équivalent, via
      `QNetworkAccessManager`) — §2.30 ; le plus simple des deux flux
      cadastre national, pas de parsing complexe, juste un client HTTP
      (le projet n'en a aujourd'hui aucun).
- [ ] Extension SpatiaLite sur `io::Database` — §2.30 ; naturel puisque déjà
      du SQLite pur (§2.5), contrairement à PostGIS ci-dessous.
- [ ] Règles métier de validation du plan cadastral national (tolérances de
      fermeture, numérotation, seuils de surface...) — §2.30 ; s'appuie
      sur les primitives déjà là (`BooleanOps`, validations P1/P2
      ci-dessus), pas une nouvelle dépendance géométrique (GEOS serait
      redondant avec CGAL déjà en place).

### P3 — qualité, robustesse, gros chantiers d'architecture

- [ ] Vraie suite de tests unitaires (Catch2/GTest) en remplacement/complément
      du smoke test actuel — notamment cas limites des booléens et de la
      triangulation.
- [ ] Gestion d'erreurs DXF plus fine (entités non reconnues actuellement
      ignorées silencieusement à la lecture).
- [ ] CI (build + tests) — pas encore de dépôt git initialisé dans ce dossier.
- [ ] Undo-log/historique dans le format `.bcad` (la base SQLite s'y prête
      bien, cf. `DESIGN_NOTES.md`).
- [ ] Outil d'ajustement de limite avec recalcul de surfaces adjacentes —
      §2.29 ; le plus dur du lot cadastre, suppose une vraie topologie
      planaire (sommets partagés entre entités voisines) que le modèle de
      données actuel n'a pas — chantier de modèle de données, pas
      simplement un nouvel outil.
- [ ] Import WFS (données vectorielles réelles du PCI ou équivalent, via
      parseur GML) — §2.30 ; plus lourd que le fond de plan WMS (P2
      ci-dessus), mais c'est ce qui permettrait de superposer/comparer un
      plan avec le référentiel officiel — pertinence directe pour l'usage
      administration/géomètre visé.
- [ ] Connexion PostgreSQL/PostGIS (client-serveur complet) — §2.30 ;
      pertinent seulement pour un déploiement institutionnel multi-
      utilisateur avec données centralisées — à ne faire qu'une fois ce
      besoin confirmé, comme Django (§2.29) ; SpatiaLite (P2 ci-dessus)
      couvre déjà l'essentiel pour un usage mono-poste.
- [ ] Cadastre 3D volumétrique (CityGML/IndoorGML) — §2.30 ; **probablement
      à ne pas anticiper** : encore expérimental dans les pays qui le
      pilotent, la quasi-totalité des appels d'offres cadastraux français
      portent sur du PCI vecteur 2D. À reconsidérer seulement si un
      dossier précis l'exige explicitement — sinon ce serait de l'effort
      investi sur un critère que peu d'appels d'offres réels demandent.
- [ ] Intégration Django (API REST, multi-tenant, audit logging serveur) —
      §2.29 ; confirmé comme projection long terme, pas un système
      existant à intégrer. Rejoint la position déjà prise sur le
      cloud/multi-utilisateur (§2.26/§4) — à revoir seulement si un vrai
      backend concret apparaît côté utilisateur.
- [ ] Lecture DWG — voir la décision stratégique en §2.22 : pas d'adhésion
      ODA pour l'instant (coût disproportionné pour un projet personnel non
      commercial), DXF reste le format d'interop principal. À revisiter en
      priorité si un vrai besoin DWG concret apparaît, via soit
      l'ODA File Converter en sous-processus (gratuit, à vérifier selon
      l'EULA pour la redistribution), soit une adhésion ODA complète si le
      projet devient commercial.
- [ ] Export DWF/DWFx — format web propriétaire Autodesk, niche pour un
      outil personnel (§2.22).
- [ ] Antialiasing des lignes en shader (MSAA global existe déjà via
      `QSurfaceFormat`, pas de line-smoothing dédié).
- [ ] Curseur avec réticule et affichage de l'accrochage actif.
- [ ] Multi-documents / multi-fenêtres (MDI) — §2.16.
- [ ] Espace papier / layouts + viewports flottants avec échelles — §2.27 ;
      même chantier que "viewports multiples" ci-dessous, formulé côté
      impression plutôt que côté écran — un seul développement couvre les
      deux usages. Prérequis de tout le reste du Module 15.
- [ ] Viewports multiples dans un même dessin (model/paper space) — §2.15 ;
      gros chantier d'architecture (`Document` suppose aujourd'hui un seul
      espace de dessin cohérent), à ne considérer que si le besoin
      d'agencer plusieurs vues d'un même plan devient réel.
- [ ] Styles de tracé (CTB/STB), configuration de traceurs/imprimantes,
      tracé par lots, Publish multi-fichiers — §2.27 ; tous dépendent des
      layouts ci-dessus sauf la configuration d'imprimante elle-même
      (`QPrinter`/`QPrintDialog`, plus simple) qui peut avancer dès l'export
      PDF de base (P2) sans attendre les layouts.
- [ ] eTransmit — §2.27 ; dépend de XREF (déjà différé ci-dessus), donc
      mécaniquement encore plus loin.
- [ ] Système UCS (repère utilisateur déplaçable/orientable) — §2.15 ;
      touche la conversion écran↔monde dans `Camera2D`/`Viewport` et tout
      ce qui en dépend (snap, saisie clavier) — à faire d'un bloc, pas par
      petites touches.
- [ ] Interface personnalisable (CUI) — §2.16 ; UI de remapping/réagencement
      du ruban et des menus, gros chantier pour un gain surtout pertinent
      à plusieurs utilisateurs/profils, pas prioritaire pour un projet
      personnel.
- [ ] Cotes radiales/angulaires/diamétrales, styles de cotes (DIMSTYLE),
      MTEXT, leaders/multileaders, champs (FIELDS) — §2.20 ; extensions de
      l'infrastructure de texte/cotation une fois son cœur (P2) en place.
- [ ] Cotes coordonnées (ordinate), annotation scale — §2.20 ; dépendent
      respectivement de l'UCS et des viewports multiples ci-dessus, donc
      forcément après eux.
- [ ] Attributs de blocs (ATTDEF/ATTEDIT), blocs dynamiques/paramétriques,
      bibliothèques de blocs, outils Design Center — §2.21 ; extensions du
      système de blocs de base (P2) une fois qu'il existe.
- [ ] Références externes (XREF), overlay vs attach — §2.21 ; signalé
      "critique pour projets" par l'utilisateur, ce qui est vrai en usage
      pro multi-fichiers — mais bcad reste aujourd'hui mono-document,
      donc sans valeur immédiate. À remonter en priorité dès qu'un vrai
      besoin multi-fichiers apparaît, pas avant.
- [ ] Macros/scripts `.scr` — §2.25 ; pourrait réutiliser le parseur de
      saisie clavier (§2.14) plutôt qu'un nouveau format, mais reste après
      l'Action Recorder/Python (P2) qui couvrent l'essentiel du besoin de
      scripting pour moins d'effort.
- [ ] API de plugins C++/C# — §2.25 ; suppose de stabiliser une frontière
      API/ABI publique que le projet n'a pas aujourd'hui (les libs
      internes ne sont pas conçues comme surface publique) — à ne faire
      qu'une fois l'intégration Python (P2) validée comme insuffisante.
- [ ] Langage de script natif (Lisp-like/Lua) — §2.25 ; plus gros que
      Python (interpréteur maison + API à exposer), à ne considérer que si
      l'intégration Python (P2) ne suffit pas.

## 4. Hors périmètre pour l'instant

- Rendu Vulkan (le sketch d'architecture le mentionnait ; OpenGL 3.3 suffit
  largement au besoin actuel — à revisiter seulement si le rendu devient le
  goulot d'étranglement).
- Contraintes paramétriques façon FreeCAD/Sketcher (solveur de contraintes) —
  ce serait un ajout majeur d'architecture, pas une simple fonctionnalité.
  Bloque aussi, par ricochet, les "blocs dynamiques" d'AutoCAD (§2.21), qui
  reposent sur le même genre de solveur.
- **Modélisation 3D complète** (Module 10, §2.23) et **rendu/visualisation
  3D** (Module 11, §2.24) — primitives 3D, extrusion/révolution/sweep,
  booléens 3D, orbite, NURBS/subdivision/maillages, historique
  paramétrique, raytracing, PBR, éclairage, caméras/animations. Pas un
  ensemble de fonctionnalités déferrées comme le reste de cette liste :
  refonte complète du noyau géométrique (`Kernel` 2D → 3D), du pipeline de
  rendu (`GlRenderer` rasterizer 2D → moteur 3D) et de la caméra
  (`Camera2D` orthographique 2D → caméra perspective avec profondeur) —
  revisiter seulement si le projet change explicitement d'objectif (2D par
  choix, §2.15).
- Compatibilité .NET/ObjectARX et COM/ActiveX (Module 12, §2.25) — n'a de
  sens que pour un concurrent visant l'écosystème de plugins AutoCAD
  existant ; sans objet pour bcad.
- SIG généraliste complet façon BCAD Map 3D (couches raster, très gros
  volumes, écosystème PostGIS) — toujours hors périmètre. **Ne couvre plus**
  les outils cadastre/géomètre ciblés (CRS/PROJ, parcelles, topologie,
  import de levés) : besoin confirmé le 2026-07-20, reclassés en priorités
  réelles (§2.28, §3).
- Édition simultanée temps réel et intégration cloud (§2.26) — supposent un
  service backend et une infra multi-utilisateur complète, hors périmètre
  d'un outil desktop personnel mono-utilisateur.
- Verrouillage de fichiers (xLock, §2.26) — signalé P1 par l'utilisateur,
  mais n'a de sens que dans un contexte de fichier partagé sur un
  réseau/serveur, qui n'existe pas dans bcad aujourd'hui ; classé ici
  plutôt qu'en P1 pour cette raison — à réévaluer si un vrai stockage
  partagé/multi-utilisateur est introduit un jour.
- Nuage de points (POINT CLOUD) — §2.17 ; pipeline de données/rendu
  entièrement différent (des millions de points, formats binaires dédiés,
  décimation/LOD spécifique) — disproportionné par rapport au reste du
  projet, à ne reconsidérer que si un besoin concret de scan 3D apparaît.
- Tableau (TABLE) — §2.17 ; dépend d'un moteur de texte/mise en page
  complet (P2) puis reste une fonctionnalité de niche pour un outil de
  dessin personnel — reconsidérer une fois le texte annoté en place, pas
  avant.
- Import IFC / STEP / IGES — §2.22 ; formats d'échange 3D (BIM, CAO
  mécanique), incompatibles avec un moteur géométrique 2D par choix (même
  raison que le rendu 3D ci-dessus) — n'aurait de sens que si le projet
  s'ouvrait un jour à la 3D, ce qui n'est pas prévu.
- Adhésion Open Design Alliance / lecture-écriture DWG native — §2.22 ;
  décision actée : coût (abonnement, conditions de redistribution) trop
  élevé pour un projet personnel non commercial. DXF reste le format
  d'interop principal ; revoir seulement si un besoin DWG concret ou un
  objectif commercial apparaît (piste intermédiaire sans adhésion : l'ODA
  File Converter en sous-processus, voir §2.22).
