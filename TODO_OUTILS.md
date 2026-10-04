# TODO — Outils de dessin, modification et annotation

> File de travail des **outils interactifs** de BCAD. Établie le 2026-10-03 par
> comparaison avec AutoCAD (référence du métier), QCAD et LibreCAD (CAO 2D
> libres, pour leurs bons outils absents d'AutoCAD) et Covadis (topographie /
> cadastre sous AutoCAD). L'état BCAD est **vérifié dans le code**, pas lu dans
> une documentation. Le reste du projet est suivi dans `TODO.md`.
>
> **Deuxième passe (même jour)** : recoupement contre la liste complète des
> outils LibreCAD (docs.librecad.org) et les menus Édition, Sélection,
> Accrochage, Info, Bloc et Modifier de QCAD. Elle a ajouté ce que la première
> avait manqué : saisie des commandes au clavier, presse-papiers, sélection
> avancée, accrochages et repérage, unités, édition de texte et de hachure, et
> les options de chaque commande. Hors périmètre de ce fichier : calques,
> vues/zoom, mise en page (voir `TODO.md`).
>
> **Partie 4 (2026-10-04)** : interface générale — ligne de commande, ruban
> contextuel, canevas, onglets, palettes, fichiers, impression, raccourcis,
> aide (préfixe `G-`).
>
> **Suivi** : chaque tâche livrée est cochée sur place avec une note « Fait »
> (date, ce qui a été fait, tests). La section **Avancement** ci-dessous en
> donne le bilan ; elle est mise à jour à chaque livraison.

## Légende

- **Priorité** — P0 indispensable au livrable d'un géomètre · P1 attendu par un
  utilisateur d'AutoCAD · P2 confort · P3 marginal. Arbitrage selon ADR-016
  (poste modeste, hors-ligne, livrable = document).
- **Effort** — S (≤ 1 jour, souvent le moteur est prêt) · M (2–4 jours) · L (≥ 1 semaine).
- **État BCAD** — ✅ dans l'interface · ⚠️ partiel · ⚙️ écrit dans le moteur,
  sans bouton · ❌ absent.
- **Source** — `ACAD` AutoCAD · `QCAD` · `LC` LibreCAD · `COV` Covadis.

## Avancement (au 2026-10-04)

**20 tâches livrées sur 286** recensées. Les P0 avancent (17 / 38), les P1 à
P3 attendent presque toutes. *(Comptage refait le 2026-10-04 : le premier
bilan omettait K-B01 et une tâche de la partie 2 ; S-08 est découpée en cinq
tâches ; S-10, S-11 à S-16, V-18 à V-22 et la partie 4 ajoutées.)*

| Partie | Livrées | Restantes |
|---|---|---|
| 1. Dessin, modification, annotation | 7 / 124 | 117 |
| 2. Affichage, calques, propriétés | 6 / 56 | 50 |
| 3. Module cadastre pour l'Afrique | 7 / 61 | 54 |
| 4. Interface générale | 0 / 45 | 45 |
| **Total** | **20 / 286** | **266** |

| Priorité | P0 | P1 | P2 | P3 | non notée |
|---|---|---|---|---|---|
| Livrées / recensées | 17 / 38 | 1 / 103 | 1 / 96 | 0 / 37 | 1 / 12 |

### Livré (dans l'ordre des commits)

| Tâche | Contenu | Commit |
|---|---|---|
| B-01 | « (Mixte) » ne déplace plus la sélection sur un calque fantôme | `7ae1c17` |
| V-09 | Plusieurs dessins ouverts, un onglet chacun, historique propre | `c68a775` |
| B-02 | Un calque occupé ne se supprime plus | `03c13bf` |
| B-03 | Libellés français des panneaux Calques et Propriétés | `8b96b35` |
| S-09 | Cycle de commande AutoCAD, la souris sélectionne au repos | `44da8ce` |
| B-04 | « Enregistrer sous » ajoute l'extension manquante | `c2aca55` |
| K-B01, K-01 | Nouvelle parcelle par ses sommets (clics ou coordonnées) | `8a8d352` |
| K-02 | Polylignes fermées converties en parcelles | `5bc8480` |
| K-09 | Contenance calculée contre déclarée, tolérance du gabarit | `56994aa` |
| K-04 | Tableau des coordonnées des bornes, plan PDF et CSV | `1dc0adb` |
| K-05 | Calcul de surface par les coordonnées, CSV | `8ecec83` |
| K-03, D-03 | Import d'un carnet de levé en bornes, import annulable | `22e6a3a` |
| V-00 | Ruban d'AutoCAD récent : Accueil, Insertion, Annoter ; affichage en barre d'état | `7fcdd2f`, `0a192b6` |
| D-01, D-01b | Outil Texte, modification d'un texte | `b8c869e` |
| A-01, A-02, A-12 | Cotations en vrais objets ; linéaire horizontale/verticale ; rayon/diamètre par désignation | `153ccf2` |

Hors tâches recensées, livrés en chemin : boîtes de message en texte brut
(injection HTML, `21784c2`), CSV protégés contre l'injection de formules
(`8ecec83`), fichier `.bcad` malveillant sans effet (nombres non finis ou
démesurés écartés, `153ccf2`).

### En cours : bloc Cotation (demande du mainteneur)

A-01 est **partiel** pour le DXF (la cote s'écrit en `LINE` + `TEXT`, pas en
entité `DIMENSION`, et les `DIMENSION` ne se lisent pas). Restent dans le
bloc : A-03 continue / ligne de base, A-04 ordonnée, A-10 longueur d'arc,
A-11 cotation rapide, A-15 interrompre / espacer, A-06 style de cote. Les
outils correspondants sont grisés dans le ruban `Annoter › Cotation`, leur
tâche en infobulle.

### Écarts constatés, à combler plus tard

- **S-10 Sélection à la souris fidèle à AutoCAD** (P1, ajoutée le 2026-10-04) :
  ajout par défaut, clic dans le vide qui garde la sélection, fenêtre en deux
  clics, lasso, surbrillance au survol, Maj qui retire, capture sur le tracé.
  Détail et définition de « fini » dans la section Sélection.

- **S-08a à S-08e Menus contextuels du clic droit** (P1 / P2, détaillées le
  2026-10-04) : menus au repos, avec une sélection, pendant une commande, des
  poignées, et réglage du clic droit ; **S-05** pour le menu d'accrochage
  (Maj + clic droit). Aujourd'hui le clic droit fait toujours Entrée.

- **S-11 à S-16 Pointeur, accrochage, double-clics, boîte Options** (P1 / P2,
  ajoutées le 2026-10-04) : réticule et cible de sélection, info-bulle et
  aimant d'accrochage, marqueurs d'AutoCAD (X pour l'intersection), zone de
  visée, boîte Options, double-clic pour modifier ; et, dans S-08e, le double
  clic droit (terminer puis relancer). Chaque tâche dit où AutoCAD place le
  réglage et où le placer dans BCAD.

- **V-18 à V-22 Barre d'état et coins inférieurs** (P1 / P2, ajoutées le
  2026-10-04) : ordre des outils d'AutoCAD avec coordonnées en tête, flèches
  d'options, menu de personnalisation (≡), touches de fonction, onglets
  Objet / Présentations ; tableau AutoCAD / BCAD bouton par bouton.

- **Partie 4, G-01 à G-38 Interface générale** (ajoutée le 2026-10-04) : ligne
  de commande (saisie semi-automatique, options cliquables, historique,
  rappel, expressions), ruban (onglets contextuels, panneaux déroulants,
  infobulles, onglets Sortie et Vue), canevas (infobulle au survol,
  calculatrice), palettes d'outils, menu de l'application et accès rapide,
  onglets des dessins et page « Début », gabarits, enregistrement automatique
  et récupération, boîte Tracer, raccourcis (dont le **conflit Ctrl+W**),
  aide hors ligne.
- **G-39 à G-45 Réglages et organisation du ruban** (ajoutées le 2026-10-04) :
  boutons à variantes (Ajuster, Cotation, Booléens regroupables tout de
  suite), boîte Options en dix onglets rapportés à BCAD, Paramètres de dessin,
  profils, accès aux styles, espaces de travail, ruban rangé panneau par
  panneau comme AutoCAD ; complément à U-01 (boîte Unités).

### P0 restantes (21)

- **Saisie** : C-01 commandes et alias au clavier.
- **Modification** : M-01 Décaler, M-02 Poignées, M-03 Étirer, M-04 Raccord,
  M-05 Tourner / échelle par valeur.
- **Mesures** : I-01 distance et angle, I-02 surface et périmètre, I-06
  coordonnées d'un point.
- **Dessin** : D-02 Hachures.
- **Affichage** : V-01 zoom fenêtre, V-02 zoom précédent / suivant, V-03 outil
  Panoramique.
- **Calques, propriétés** : L-01 rendu épaisseur et type de ligne, L-02 liste
  des calques dans le ruban, P-01 propriétés géométriques éditables.
- **Cadastre** : K-06 système de coordonnées, K-08 riverains, K-10 gabarits
  pays, K-45 choix du pays, K-46 contour du pays en fond.

## À vérifier (au 2026-10-04)

Ce qui est livré ou documenté mais **n'a pas été confirmé** : à vérifier avant
de s'y fier, puis à rayer ici.

### Par un géomètre agréé (règles métier supposées)

- [ ] **Écart admis de la contenance (K-09)** : périmètre (contour et trous) ×
      tolérance linéaire du gabarit (`survey_tolerance.default_m`, 0,02 m pour
      le Togo). Formule supposée, non tirée d'un texte officiel.
- [ ] **Gisements en grades (K-04)**, depuis le nord, sens horaire : convention
      française, à confirmer pays par pays (certains plans emploient les
      degrés) ; deviendra un réglage avec U-01.
- [ ] **Texte des cotes automatiques du plan cadastral** à 0,5 m (un tiers du
      décalage de 1,5 m) : lisible à quelle échelle d'impression ? à régler
      par le gabarit si besoin.
- [ ] **Procédures de la partie 3** : sources publiques ; Mali et Niger non
      documentés ; textes de loi non lus. Chaque gabarit pays à faire valider
      avant d'être écrit.

### Sur AutoCAD 2025 en français (libellés et emplacements de mémoire)

- [ ] Menus du clic droit (S-08a à S-08e), menu d'accrochage (S-05).
- [ ] Pointeur, accrochage, double-clics, boîte Options (S-11 à S-16, G-40) :
      noms des onglets, cadres, cases et curseurs, valeurs par défaut
      (réticule 5 %, cible 3 px, zone de visée 10 px).
- [ ] Barre d'état : ordre des boutons, éléments masqués par défaut (V-18 à V-22).
- [ ] Partie 4 : ligne de commande, ruban contextuel, panneaux et gros
      boutons de l'onglet Début (G-45), boutons à variantes (G-39), Paramètres
      de dessin (G-41), boîte Unités (complément à U-01), raccourcis (G-36).
- [ ] Gestes de sélection par défaut (S-10 : PICKADD, PICKDRAG, lasso).
- [ ] Double clic droit : aucun geste propre dans AutoCAD (deux Entrée) —
      à confirmer.

### Dans BCAD, non couvert par un test automatique

- [ ] **Rendu des cotes au PDF** (trait ouvert, texte centré, couleur) : aucun
      test ne lit le PDF produit ; vérifié seulement par relecture du code.
- [ ] **Aperçu de cotation sous le curseur** et **texte des cotes au canevas** :
      non testés (le canevas OpenGL ne s'affiche pas hors écran) ; à
      vérifier à l'œil dans la fenêtre.
- [ ] **Calque masqué** : ses textes et cotes ne s'affichent plus au canevas
      (changement du 2026-10-04, `drawEntityTexts`) — non testé.
- [ ] **DXF des cotes** (`LINE` + `TEXT`) : jamais ouvert dans AutoCAD, QCAD ou
      LibreCAD ; vérifier lisibilité, position et angle du texte. L'export DXF
      cadastral écrit aussi ses cotes ainsi désormais.
- [ ] **Fermeture de la fenêtre et des onglets** : le correctif du plantage à la
      fermeture (`~MainWindow`) est vérifié par les tests, pas par une
      compilation AddressSanitizer (arrêtée faute de mémoire, à relancer
      seulement sur demande).

### Changements de comportement à confirmer avec le mainteneur

- [ ] Une **cotation linéaire mesure le long de sa direction** : un ancien fichier
      portant une rotation non nulle affiche une autre valeur qu'avant.
- [ ] **Valeurs de cote avec un point** (`10.00`), pas une virgule : à régler
      avec le style de cote (A-06).
- [ ] **Choix ouverts** : `Ctrl+W` (fermer le dessin ou cycle de sélection,
      G-36) ; touche `F` (zoom sur tout, propre à QCAD, V-21) ; zoom et
      panneaux dans le coin inférieur droit (écart voulu de V-00, G-12) ;
      réglage par défaut du clic droit (S-08e).

## Synthèse du benchmark

**Comparés** : AutoCAD 2025 (référence du métier : panneaux Dessin, Modification,
Annotation du ruban et leurs commandes), QCAD (menus Dessin, Modifier, Cotation,
Édition, Sélection, Accrochage, Info, Bloc), LibreCAD (liste complète des
outils), Covadis (outils topographiques sous AutoCAD).

**État de BCAD au 2026-10-03** (vérifié dans le code ; photographie de départ
du benchmark, le bilan courant est dans **Avancement** ci-dessus) :

| | Nombre |
|---|---|
| Outils livrés dans l'interface (Lot 0) | 35 : 29 de l'hôte (dont la cotation « linéaire », partielle) + 6 du module cadastre |
| Écrits dans le moteur mais sans bouton (⚙️) | décalage (ligne, arc, cercle, polyligne), entité texte, entités de cotation, styles de cote, triangulation de Delaunay *(texte et cotations ont leurs outils depuis le 2026-10-04)* |
| Tâches ouvertes, partie 1 (dessin, modification, annotation) | **112** |
| — par priorité | P0 : 15 · P1 : 36 · P2 : 44 · P3 : 17 |
| — par effort | S : 65 · M : 36 · L : 11 |
| Tâches ouvertes, partie 2 (affichage, calques, propriétés) | **49**, dont 3 défauts constatés |
| — par priorité | P0 : 9 · P1 : 18 · P2 : 14 · P3 : 7 (+ 1 renvoi vers M-10) |
| Tâches ouvertes, partie 3 (module cadastre pour l'Afrique) | **61**, dont 1 défaut constaté |
| — par priorité | P0 : 16 · P1 : 31 · P2 : 12 · P3 : 2 |

**Ce qui manque le plus** (constat du benchmark) :

1. La **saisie des commandes au clavier** (`L`, `TR`, `O`…, Entrée répète) :
   l'habitude première d'un utilisateur d'AutoCAD, absente.
2. Les **gestes de modification de base** : décaler, raccord, étirer, poignées.
   Le décalage est déjà écrit dans le moteur.
3. Le **texte et les vraies cotations** : les entités existent et sont
   sérialisées. *(2026-10-04 : outil Texte livré (D-01) ; cotations en vrais
   objets et linéaire horizontale/verticale livrées (A-01, A-02).)*
4. Les **mesures** (distance, surface, coordonnées) : le cœur du métier de
   géomètre, aucun outil.
5. Les **hachures** et l'**import de levé** : prérequis du livrable
   (`ROADMAP_MARKET.md` étapes 2 et 3). *(Import de levé livré le 2026-10-04,
   K-03 / D-03 ; hachures toujours à faire.)*
6. Les **aides au dessin** : 8 modes d'accrochage sur 14, tout ou rien ; pas de
   repérage polaire ni de point « À partir de » ; pas de presse-papiers ; pas
   d'unités dans le document.

**Ce que QCAD et LibreCAD apportent de plus qu'AutoCAD** (Lot 4) : des outils
directs là où AutoCAD passe par des accrochages ou des options — lignes
tangentes, bissectrice, perpendiculaire, rognage automatique, rogner les deux,
découpe au rectangle, calage par points de référence, simplification de
polyligne, polylignes équidistantes, menu Info complet. Plus simples à
apprendre : utile pour l'usage éducation / TP.

## Règles pour chaque outil (définition de « fini »)

1. Une entrée dans `kTools` (`MainWindowTools.cpp`) : le menu, le ruban et la
   barre d'état en sont des lectures. Une icône SVG dans `src/app/icons/`.
2. Une **consigne par étape** dans `ViewportPrompts.cpp` (« Décaler — spécifiez
   la distance »).
3. Saisie clavier **et** clic : tout passe par `placePoint` / `submitTypedPoint`.
4. Une seule étape d'annulation (macro), entités rendues sous leur id.
5. Refus = message en barre d'état (`statusMessage`), jamais de boîte modale.
6. Un cas dans `tests/unit/app/ViewportToolsTest.cpp` (faire, annuler, refaire).
7. Aucun nom de métier dans `src/app` ni `src/core` (`scripts/check_arch.sh`) :
   un outil cadastral vit dans le module, via `IWorkbench`.

---

## Lot 0 — Déjà livré (ne pas refaire)

- [x] Ligne (2 points, relatif `@dx,dy`, polaire `@d<a`) — `ACAD LINE`
- [x] Cercle centre + rayon — `ACAD CIRCLE`
- [x] Arc centre, départ, fin — `ACAD ARC`
- [x] Polyligne (segments droits, Entrée termine, `C` ferme) — `ACAD PLINE`
- [x] Rectangle 2 coins — `ACAD RECTANG`
- [x] Point — `ACAD POINT`
- [x] Sélection fenêtre / capture, tout sélectionner, dernier — `ACAD SELECT`
- [x] Déplacer (sélection ou objet désigné), Copier, Tourner, Échelle, Symétrie
- [x] Rogner / Prolonger (lignes et arcs), Scinder (1 point), Joindre, Exploser, Effacer
- [x] Union / Intersection / Différence / Différence symétrique (polylignes fermées)
- [x] Cotations linéaire, alignée, angulaire, rayon, diamètre (objets cotation, A-01)
- [x] Texte sur une ligne et modification d'un texte (D-01, D-01b, 2026-10-04)
- [x] Cycle de commande AutoCAD : Entrée / Espace / clic droit répètent ou
      terminent, Échap annule, la souris sélectionne au repos (S-09)
- [x] Cadastre : créer, rechercher, scinder, lotir, fusionner, modifier la limite (module)

---

## Lot 1 — P0, le moteur est prêt (gains rapides)

- [ ] **M-01 Décaler** (`OFFSET`) — P0 · S · `ACAD OFFSET`, `QCAD` Offset avec
      distance / par un point · ⚙️ `offsetLine/Arc/Circle/Polyline` existent.
      Fini quand : distance saisie (ou point de passage), côté désigné au clic,
      répétable sans relancer l'outil. Options AutoCAD : `P`ar (point de
      passage), `E`ffacer la source, `C`alque courant ou de la source, `M`ultiple.
- [ ] **C-01 Commandes et alias au clavier** — P0 · M · `ACAD` (alias),
      `LC` ligne de commande, `QCAD` raccourcis à deux lettres · ❌ la ligne de
      commande n'accepte que des points. Taper `L`, `PL`, `C`, `A`, `REC`, `POL`,
      `PO`, `T`, `H`, `M`, `CO`, `RO`, `SC`, `MI`, `S`, `TR`, `EX`, `BR`, `J`, `X`,
      `E`, `O`, `F`, `CHA`, `AR`, `DLI`, `DAL`, `DAN`, `DRA`, `DDI` lance l'outil ;
      **Entrée ou Espace sur ligne vide répète la dernière commande** ; les options
      d'une commande se tapent par leur lettre (ex. `C` Clore, `D` Distance).
      Les alias vivent dans la table `kTools`, pas en dur ailleurs.
- [x] **D-01 Texte** (`TEXT`) — P0 · S · `ACAD TEXT` · ⚙️ `TextEntity` existe,
      sérialisé. Point d'insertion, hauteur, angle, contenu saisi.
      **Fait (2026-10-04)** : outil Texte (ruban Accueil › Annotation et
      Annoter › Texte, menu Dessin) aux étapes de la commande TEXTE d'AutoCAD —
      point de départ, hauteur (tapée ou second clic ; Entrée garde la
      précédente), angle (degrés tapés ou clic), puis lignes tapées dans la
      ligne de commande, chacune posée 1,5 hauteur sous la précédente, ligne
      vide pour terminer. Le canevas dessine maintenant l'angle du texte.
- [x] **D-01b Modifier un texte** (double-clic) — P0 · S · `ACAD TEXTEDIT`,
      `QCAD` Edit Text · ❌. Contenu, hauteur, angle ; annulable.
      **Fait (2026-10-04)** pour le contenu : double-clic sur un texte au
      repos, ou « Modifier le texte » (Édition, Annoter › Texte) sur la
      sélection ; commande annulable (`viewport_tools_test`). Hauteur et angle
      se modifieront par le panneau Propriétés (P-05).
- [x] **A-01 Cotations en vrais objets** — P0 · M · `ACAD DIM*` · ⚙️
      `LinearDimensionEntity`, `AlignedDimensionEntity`, `AngularDimensionEntity`,
      `RadialDimensionEntity` existent et sont sérialisés ; l'outil actuel les
      **éclate** en lignes + texte. Fini quand : une cotation se sélectionne, se
      déplace et s'enregistre comme un seul objet (export DXF `DIMENSION` : voir
      `TODO.md` §3 étape 2).
      **Fait (2026-10-04)** : chaque outil de cote pose un seul objet, dessiné
      (lignes d'attache, ligne de cote, flèches, valeur jamais à l'envers) au
      canevas, à l'aperçu et au PDF par un calcul unique
      (`geom::dimensionGraphics`, aussi pour la sélection et l'emprise). Hauteur
      de texte : propriété `dimension.text_height`, enregistrée et modifiable
      dans Propriétés. Les cotes automatiques du module cadastre s'affichent
      enfin. **Partiel pour le DXF** : la cote s'exporte en `LINE` + `TEXT`
      (lisible partout, non associative) ; l'entité `DIMENSION` avec son bloc
      anonyme et la lecture des `DIMENSION` restent à faire.
      Tests : `dimension_graphics_test`, `dimension_persistence_test`,
      `viewport_tools_test`.
- [x] **A-02 Cotation linéaire horizontale / verticale** — P0 · S · `ACAD
      DIMLINEAR`, `LC/QCAD` Horizontal + Vertical · ⚠️ l'outil « Linéaire » est en
      fait une alignée décalée. Fini quand : H ou V choisi par la position de la
      ligne de cote (comme AutoCAD), plus deux variantes forcées (`QCAD DH/DV`).
      **Fait (2026-10-04)** : la position choisit H ou V ; `H` ou `V` tapé
      avant la position les force (pas de boutons DH/DV séparés). La mesure
      suit la direction de cote, y compris après une symétrie.
- [ ] **I-01 Mesurer distance et angle** — P0 · S · `ACAD MEASUREGEOM`, `LC`
      Info › Distance / Angle, `QCAD` Info · ❌. Résultat dans la ligne de
      commande, sans rien créer.
- [ ] **I-02 Mesurer surface et périmètre** — P0 · S · `ACAD AREA`, `LC/QCAD`
      Info › Area (polygonale, cercle/arc, polyligne) · ❌ (`polygonArea` existe,
      trous compris). Par points ou par objet fermé ; options Ajouter /
      Soustraire. Indispensable au géomètre.
- [ ] **I-06 Coordonnées d'un point** — P0 · S · `ACAD ID`, `QCAD` Position /
      Position relative / polaire · ❌. X, Y affichés dans la ligne de commande.

## Lot 2 — P0, gestes de base attendus d'un utilisateur AutoCAD

- [ ] **M-02 Poignées** (grips) — P0 · L · `ACAD grips` · ❌. Sommets, milieux
      et centres déplaçables à la souris sur la sélection ; saisie clavier
      pendant le glisser.
- [ ] **M-03 Étirer** (`STRETCH`) — P0 · M · `ACAD STRETCH`, `QCAD SS`,
      `LC` Stretch · ❌. Fenêtre de capture, sommets inclus déplacés.
- [ ] **M-04 Raccord** (`FILLET`) — P0 · M · `ACAD FILLET`, `QCAD Round` · ❌.
      Rayon saisi, rayon 0 = coin vif ; lignes, arcs, sommets de polyligne.
      Options : `P`olyligne (tous les sommets), `R`ayon, `A`juster / ne pas
      ajuster les objets, `M`ultiple. LibreCAD et QCAD : même outil.
- [ ] **M-05 Tourner / Échelle par valeur** — P0 · S · `ACAD ROTATE/SCALE` · ⚠️
      seulement par points. Angle (degrés) et facteur saisis dans la ligne de
      commande, option Copie.
- [ ] **D-02 Hachures** (`HATCH`) — P0 · L · `ACAD HATCH`, `QCAD` Hatch from
      selection / from segments · ❌. Contour fermé désigné ou point intérieur,
      trous respectés, motifs plein + ANSI31 au minimum, export DXF `HATCH`.
      (`ROADMAP_MARKET.md` étape 2.) Avec : **modifier une hachure** (motif,
      échelle, angle — `ACAD HATCHEDIT`, `QCAD` Edit Hatch), P1.
- [x] **D-03 Import de points de levé** (fait par K-03, 2026-10-04) — P0 · M · `COV` Points topo · ❌.
      CSV/TXT `nom, X, Y, Z, code` → points + étiquettes. Via `IFileImporter`
      (point d'extension prêt), dans le module concerné si le format est métier.

## Lot 3 — P1, les outils AutoCAD que l'on cherche ensuite

### Dessin
- [ ] **D-04 Arc 3 points** (défaut d'AutoCAD) et départ-centre-fin,
      départ-fin-rayon, départ-fin-angle — P1 · S · `ACAD ARC`, `QCAD` (10 variantes).
- [ ] **D-05 Cercle** : centre-diamètre, 2 points, 3 points, tangent-tangent-rayon
      — P1 · M · `ACAD CIRCLE`, `QCAD` (13 variantes).
- [ ] **D-06 Polygone régulier** inscrit / circonscrit / par côté — P1 · S ·
      `ACAD POLYGON`, `QCAD` 4 variantes.
- [ ] **D-07 Diviser / Mesurer** (points régulièrement espacés sur un objet) —
      P1 · S · `ACAD DIVIDE/MEASURE`, `QCAD` N points on line, Divide.
- [ ] **D-08 Polyligne à segments d'arc** — P1 · L · `ACAD PLINE` option Arc ·
      ⚠️ `PolylineEntity` n'a pas de bulge (prérequis aussi pour Joindre des arcs).
- [ ] **D-09 Blocs** : créer, insérer, éclater (symboles : borne, arbre, regard)
      — P1 · L · `ACAD BLOCK/INSERT`, `QCAD` menu Bloc · ❌. Puis : attributs
      (`ATTDEF`, P2), éditer un bloc et ses références, purger les inutilisés,
      bibliothèque de symboles livrée par les modules (gabarits pays).
- [ ] **D-10 Image raster** : insérer, caler sur 2 points (fond scanné) — P1 · M ·
      `ACAD IMAGEATTACH`, `QCAD` Insert Bitmap · ❌.
- [ ] **D-11 Texte multiligne** (`MTEXT`) — P1 · M · `ACAD MTEXT` · ❌.
- [ ] **D-12 MNT et courbes de niveau** depuis les points de levé — P1 · L ·
      `COV` · ⚙️ triangulation de Delaunay dans `geometry/Triangulation.cpp`
      (`ROADMAP_MARKET.md` étape 5).

### Modification
- [ ] **M-06 Réseau** rectangulaire, polaire, le long d'une trajectoire — P1 · M ·
      `ACAD ARRAY/ARRAYRECT/ARRAYPOLAR/ARRAYPATH` · ❌. Nombre et pas saisis ;
      réseau associatif (modifiable ensuite, `ARRAYEDIT`) en P2.
- [ ] **M-07 Chanfrein** (`CHAMFER`) — P1 · S (après M-04) · `ACAD CHAMFER`,
      `QCAD/LC` Bevel.
- [ ] **M-08 Rogner / Prolonger étendus** aux cercles et polylignes, avec arêtes
      de coupe désignées — P1 · M · `ACAD TRIM/EXTEND` · ⚠️ lignes et arcs, coupe
      « contre tout ». Options : `T`rajet (fence), `C`apture, `E`ffacer, Maj+clic
      pour basculer Rogner ↔ Prolonger (comme AutoCAD).
- [ ] **M-09 Édition de polyligne** : ajouter / supprimer / déplacer un sommet,
      ouvrir / fermer — P1 · M · `ACAD PEDIT`, `QCAD` Insert / Delete / Append
      Node · ⚠️ seulement « déplacer un sommet » de parcelle (module).
- [ ] **M-10 Copier les propriétés** (`MATCHPROP`) — P1 · S · `ACAD MATCHPROP`,
      `LC` Modify › Properties · ❌. Calque, couleur, style de cote.
- [ ] **M-11 Copie multiple** (répéter jusqu'à Entrée) — P2 · S · `ACAD COPY` ·
      ⚠️ une copie par lancement.

### Annotation
- [ ] **A-03 Cotation continue et ligne de base** — P1 · M · `ACAD
      DIMCONTINUE/DIMBASELINE`, `QCAD DC/DB` · ❌ (après A-01).
- [ ] **A-04 Cotation en ordonnée / coordonnées X,Y** — P1 · S · `ACAD
      DIMORDINATE`, `QCAD DO` · ❌. Très utilisée en topographie.
- [ ] **A-05 Ligne de repère** (`MLEADER`) — P1 · M · `ACAD MLEADER`, `LC/QCAD`
      Leader · ❌.
- [ ] **A-06 Styles de cote** : flèches, précision, unités, hauteur — P1 · M ·
      `ACAD DIMSTYLE` · ⚙️ `layout/DimensionStyle.h` existe, aucune interface.
- [ ] **A-07 Styles de texte** — P1 · S · `ACAD STYLE` · ❌ (les `text_styles`
      des gabarits pays attendent ce consommateur, `TODO.md` §7.1).
- [ ] **A-08 Tableau** dans le dessin (nomenclature, surfaces) — P1 · L ·
      `ACAD TABLE` · ⚠️ seulement sur la feuille imprimée du module
      (`ROADMAP_MARKET.md` étape 4).
- [ ] **A-09 Étiquettes de limite** : gisement + distance sur chaque côté,
      coordonnées des sommets — P1 · M · `COV` · ⚠️ cotes automatiques de
      parcelle dans le module seulement.

### Aides au dessin (accrochage, repérage, saisie)

État : l'accrochage couvre extrémité, milieu, centre, intersection, quadrant,
perpendiculaire, proche, grille (`SnapEngine.h`), activé ou non en bloc ;
mode orthogonal F8.

- [ ] **S-01 Accrochages manquants** : tangente, nœud (point), centre
      géométrique (polygone fermé), insertion (texte, bloc), parallèle,
      prolongement — P1 · M · `ACAD OSNAP`, `QCAD` menu Accrochage.
- [ ] **S-02 Choix des accrochages actifs** (cases par mode, comme l'onglet
      Accrochage aux objets d'AutoCAD) — P1 · S · ⚠️ tout ou rien aujourd'hui.
- [ ] **S-03 Repérage polaire** (angles par pas : 15°, 30°, 45°, 90°) — P1 · M ·
      `ACAD POLAR`, `QCAD` Restrict Angle or Length.
- [ ] **S-04 Point « À partir de »** / origine relative déplaçable — P1 · S ·
      `ACAD FROM`, `QCAD` Set / Lock Relative Zero. Les coordonnées `@` partent
      aujourd'hui du dernier point de l'outil.
- [ ] **S-05 Accrochage ponctuel** (forcer un mode pour le prochain point :
      Maj + clic droit, ou `FIN`, `MIL`, `CEN` tapés) — P2 · S · `ACAD`.
      **Maj + clic droit** (ou Ctrl + clic droit) ouvre à tout moment le menu
      d'accrochage d'AutoCAD : Point de repérage temporaire, À partir de
      (S-04), Milieu entre 2 points, puis Extrémité, Milieu, Intersection,
      Centre, Quadrant, Tangente (S-01), Perpendiculaire, Parallèle, Nœud,
      Proche, Aucun, et « Paramètres d'accrochage… » (S-02). Le mode choisi ne
      vaut que pour le prochain point. Aujourd'hui Maj + clic droit fait
      Entrée, comme un clic droit simple.
- [ ] **S-06 Repérage d'accrochage objet** (alignement sur des points déjà
      accrochés) — P2 · L · `ACAD OTRACK`.
- [ ] **S-07 Saisie dynamique** près du curseur (longueur, angle) — P2 · M ·
      `ACAD DYNMODE`.
- [x] **S-09 Cycle de commande AutoCAD et sélection à la souris** — P0 · M ·
      `ACAD` · demandé par le mainteneur (2026-10-04 : « dans AutoCAD, la
      sélection se fait juste depuis la souris »). **Fait** : au repos, la
      souris sélectionne (clic, fenêtre, capture) sans choisir d'outil ; une
      commande se termine seule et revient au repos (cercle, arc, rectangle,
      point, cotations, déplacer, copier, tourner, échelle, symétrie,
      scinder) ; la ligne enchaîne ses segments jusqu'à Entrée, la polyligne
      jusqu'à Entrée ou C, rogner et prolonger se répètent jusqu'à Entrée ;
      Échap termine la commande puis vide la sélection ; Entrée, Espace ou
      ligne de commande vide au repos relancent la dernière commande ; clic
      droit = Entrée ; une modification lancée sans sélection fait d'abord
      désigner ses objets à la souris (clic ajoute, Maj retire, fenêtre),
      Entrée ou clic droit valide — ce qui remplace le « clic sur un objet »
      propre à Déplacer. `viewport_tools_test` couvre le cycle, clics compris.
**S-08 Menus contextuels du clic droit** — `ACAD SHORTCUTMENU` · ⚠️ dans BCAD
le clic droit fait toujours Entrée (valide l'étape, termine la commande ou la
désignation), sans aucun menu. Constat du 2026-10-04, découpé en cinq tâches.
Règles communes :
- les menus **réutilisent les `QAction` existantes** (outils de `kTools`,
  actions de `MainWindowMenus.cpp`, ruban) : une seule façon de faire
  (ADR-016), mêmes libellés, mêmes icônes, mêmes raccourcis affichés ;
- une entrée dont l'outil n'est pas encore livré **n'apparaît pas** (pas
  d'entrée grisée dans un menu contextuel, contrairement au ruban) ;
- le canevas signale la demande (position, mode : repos, sélection ou
  commande) et `MainWindow` compose le menu : `Viewport` ne connaît pas les
  actions ;
- un module peut ajouter ses entrées pour ses propres objets (par exemple
  « Propriétés de la parcelle ») par son workbench, sans nom de métier dans
  `src/app` (`check_arch.sh`) ;
- fini quand : chaque menu est ouvert dans `viewport_tools_test` ou
  `mainwindow_workbench_test` et ses entrées vérifiées, et
  `UI_CONVENTIONS.md` § « Cycle d'une commande » les décrit.

- [ ] **S-08a Menu au repos** (aucune commande, rien de sélectionné) — P1 · S ·
      `ACAD` mode par défaut. Entrées : **Répéter** la dernière commande
      (« Répéter Ligne », existe : Entrée au repos), **Entrées récentes**
      (dernières coordonnées et valeurs tapées), **Annuler / Rétablir**,
      **Coller** et variantes (E-01), **Panoramique** (V-03), **Zoom** :
      étendu (existe), fenêtre (V-01), précédent (V-02), **Isoler / Tout
      afficher** (L-07), **Sélection rapide** (M-16), **Rechercher** (E-03),
      **Tout sélectionner** (existe).
- [ ] **S-08b Menu avec une sélection** (mode édition) — P1 · M · `ACAD` mode
      édition. Entrées : Répéter, Entrées récentes, **Couper / Copier / Copier
      avec point de base** (E-01), **Effacer, Déplacer, Copier, Échelle,
      Rotation, Symétrie** (existent), **Ordre de tracé** (M-14), **Ajouter la
      sélection** (lance l'outil qui crée ce type d'objet), **Sélectionner
      similaire** (même type et même calque), **Désélectionner tout**
      (SEL-01, existe en Échap), **Isoler** (L-07), **Propriétés** (ouvre le
      panneau, existe), **Propriétés rapides** (P-10). Plus des entrées
      **propres au type** de la sélection : texte → Modifier le texte (existe,
      D-01b) ; cotation → position du texte, précision, inverser la flèche
      (A-15, A-06) ; polyligne → Éditer la polyligne (M-09) ; objet d'un
      module → ses entrées.
- [ ] **S-08c Menu pendant une commande** (mode commande) — P1 · M · `ACAD`
      mode commande. Entrées : **Entrée**, **Annuler** (la commande, = Échap),
      **Entrées récentes**, puis les **options de la commande en cours**,
      celles qu'affiche la consigne entre crochets (ex. cotation linéaire
      `[H horizontale / V verticale]`, polyligne `Clore`, ligne `Annuler le
      dernier segment`) — choisir une option revient à la taper dans la ligne
      de commande ; puis le sous-menu **Remplacements d'accrochage** (S-05),
      **Panoramique** et **Zoom**. Prérequis : que chaque consigne déclare ses
      options sous une forme lisible (liste) plutôt que dans son texte.
- [ ] **S-08d Menu des poignées** (clic droit sur une poignée active) — P2 · S
      (après M-02) · `ACAD` grips. Entrées : Étirer, Déplacer, Rotation,
      Échelle, Miroir, Point de base, Copier, Référence, Annuler, Propriétés.
- [ ] **S-08e Réglage du clic droit** — P2 · S · `ACAD` Options › Préférences
      utilisateur › Personnaliser le clic droit. Pour chaque mode (repos,
      sélection, commande) : menu ou Entrée ; plus le **clic droit temporisé**
      (clic court = Entrée, clic maintenu = menu, délai réglable). Défaut
      proposé : menu au repos et avec une sélection, **Entrée pendant une
      commande** (le geste actuel de BCAD, déjà appris). Réglage gardé par
      utilisateur, hors du document.
      **Double clic droit** : AutoCAD n'a pas de geste propre ; avec le clic
      droit réglé sur Entrée, deux clics droits **terminent la commande puis
      relancent la dernière** (Entrée au repos répète) — geste très utilisé.
      BCAD ne le permet pas : au repos, `pressEnter(fromRightClick = true)`
      (`src/app/ViewportInput.cpp`) ne relance volontairement pas. À changer
      ici : au repos, en mode Entrée, le clic droit relance comme Entrée.
      *Où, comme AutoCAD* : Options › onglet **Préférences utilisateur** ›
      cadre « Comportements standard Windows » › bouton **« Personnaliser le
      clic droit… »**, qui ouvre une boîte à trois cadres (Mode par défaut :
      Répéter la dernière commande / Menu contextuel ; Mode édition ; Mode
      commande : Entrée / Menu si la commande a des options / Menu) et la case
      « Activer le clic droit temporisé ». Dans BCAD : la même page de la
      boîte Options (S-15).

**Pointeur, accrochage et double-clics** — constat du 2026-10-04 : dans BCAD le
pointeur du canevas est la **flèche système** (aucun `setCursor` dans
`src/app/Viewport*.cpp`) ; on vise avec sa pointe. Les marqueurs d'accrochage
existent (`drawSnapMarker`, `src/app/ViewportOverlay.cpp`) et le point cliqué
est bien le point accroché (`snappedWorld`), mais sans nom affiché ni pointeur
qui saute sur le point. Les libellés AutoCAD ci-dessous sont ceux de la version
française, **à vérifier sur AutoCAD 2025 FR** avant de les reprendre.

- [ ] **S-11 Réticule et cible de sélection** (le pointeur d'AutoCAD) — P1 · S ·
      `ACAD CURSORSIZE`, `PICKBOX`. Le pointeur devient **deux traits en
      croix** ; le point visé est leur croisement. Trois états, comme
      AutoCAD : **au repos** croix + petit **carré** au centre (la cible de
      sélection) ; **saisie d'un point** croix seule ; **« désignez les
      objets »** (`pickingObjects_`) carré seul. Longueur des traits par
      défaut 5 % du canevas (réglable jusqu'à 100 % = plein écran) ; carré
      par défaut 3 px de demi-côté.
      *Où, comme AutoCAD* : Options › onglet **Affichage** › curseur
      **« Taille du réticule »** (en bas à droite de l'onglet) ; couleur par
      Options › Affichage › **Couleurs…** › Espace objet 2D › Réticule ;
      taille du carré : Options › onglet **Sélection** › curseur **« Taille de
      la cible de sélection »** (en haut à gauche).
      *Où dans BCAD* : `setCursor(Qt::BlankCursor)` sur le canevas
      (`Viewport.cpp`, constructeur), curseur système rendu en sortie du
      canevas ; nouvelle `drawCursor` dans `ViewportOverlay.cpp`, appelée en
      dernier par `Viewport::paintGL` (après `drawRubberBand`) ; la taille du
      carré **est** la tolérance de pointage `kPickToleranceScreenPx`
      (`ViewportTolerances.h`, 6 px aujourd'hui), qui devient le réglage.
      Fini quand : les trois états se voient, le carré et la tolérance de
      pointage restent la même valeur (un test le vérifie).
- [ ] **S-12 Info-bulle d'accrochage et aimant** — P1 · S · `ACAD AUTOSNAP`.
      **Info-bulle** : le nom du mode (« Extrémité », « Milieu », « Centre »,
      « Intersection », « Perpendiculaire »…) affiché à côté du marqueur.
      **Aimant** : le réticule saute sur le point d'accrochage dès qu'il entre
      dans la zone de visée, et y reste tant qu'il y est.
      *Où, comme AutoCAD* : Options › onglet **Dessin** › cadre
      **« Paramètres d'accrochage automatique »** (en haut à gauche) : cases
      **Marqueur**, **Aimant**, **Afficher l'info-bulle d'accrochage
      automatique**, Afficher la zone de visée (S-14), bouton Couleurs….
      *Où dans BCAD* : l'info-bulle dans `drawSnapMarker`
      (`ViewportOverlay.cpp`), en bas à droite du marqueur, un libellé par
      `SnapType` ; l'aimant dans `drawCursor` (S-11) : le réticule est dessiné
      au point accroché (`activeSnap_`) plutôt qu'à la souris. Les trois cases
      dans la page Dessin de S-15.
- [ ] **S-13 Marqueurs d'accrochage conformes à AutoCAD** — P2 · S · `ACAD`.
      Formes : Extrémité **carré** ✅, Milieu **triangle** ✅, Centre
      **cercle** ✅, Perpendiculaire **angle droit** ✅, Intersection **X**
      (⚠️ BCAD dessine un losange), Quadrant **losange**, Tangente **cercle et
      trait**, Nœud **cercle barré d'un X**, Proche **sablier**, Parallèle
      **deux traits**, Extension **petites croix** — les cinq derniers avec
      leurs modes (S-01, S-06). Taille et couleur réglables.
      *Où, comme AutoCAD* : Options › onglet **Dessin** › curseur **« Taille
      du marqueur d'accrochage automatique »** (en bas à gauche) et bouton
      **Couleurs…** du cadre « Paramètres d'accrochage automatique ».
      *Où dans BCAD* : `drawSnapMarker` (`ViewportOverlay.cpp`, demi-taille
      `kHalf`) ; l'intersection passe du losange au X.
- [ ] **S-14 Zone de visée de l'accrochage** (ouverture) — P2 · S ·
      `ACAD APERTURE`. Rayon autour du réticule dans lequel un point
      d'accrochage est cherché ; AutoCAD : 10 px par défaut, carré
      **masqué** par défaut (case pour l'afficher).
      *Où, comme AutoCAD* : Options › onglet **Dessin** › curseur **« Taille
      de la zone de visée »** (en bas à droite) ; case « Afficher la zone de
      visée de l'accrochage automatique » dans le cadre de S-12.
      *Où dans BCAD* : `kSnapToleranceScreenPx` (`Viewport.cpp`, 10 px,
      déjà la valeur d'AutoCAD) devient le réglage ; le carré, s'il est
      demandé, dessiné par `drawCursor` (S-11).
- [ ] **S-15 Boîte de dialogue Options** — P1 · M · `ACAD OPTIONS` (les dix
      onglets d'AutoCAD rapportés à BCAD : G-40). Porte les
      réglages de S-08e, S-10, S-11 à S-14, S-16 et la taille des poignées
      (M-02). Onglets repris dans l'**ordre d'AutoCAD**, seulement ceux qui
      ont des réglages : **Affichage** (réticule, couleurs), **Préférences
      utilisateur** (clic droit, double-clic), **Dessin** (accrochage
      automatique, zone de visée), **Sélection** (cible, aperçu, poignées).
      Réglages par utilisateur (`QSettings`, comme les états de calques de
      `LayerPanel.cpp`), **jamais dans le document** ; boutons OK / Annuler /
      Appliquer ; effet immédiat sur le canevas.
      *Où, comme AutoCAD* : bouton **« Options »** en **bas du menu de
      l'application** (le bouton en haut à gauche, à côté de la barre d'accès
      rapide) ; aussi en fin du menu contextuel au repos (« Options… »,
      S-08a) et par la commande `OPTIONS` tapée.
      *Où dans BCAD* : le menu de l'application est `fileMenu`
      (`ribbon_->setApplicationMenu(fileMenu)`, `MainWindowMenus.cpp`) —
      « Options… » en dernière entrée, après un séparateur ; aussi menu
      **Outils** › Options… (`toolsMenu`, même fichier) ; boîte dans une
      nouvelle unité `src/app/OptionsDialog.cpp` ; le `Viewport` reçoit les
      valeurs par des accesseurs, sans lire `QSettings` lui-même.
- [ ] **S-16 Double-clic pour modifier tout objet** — P1 · S · `ACAD
      DBLCLKEDIT`. Double-clic gauche sur : un **texte** → édition ✅ (D-01b) ;
      une **cotation** → édition de son texte (avec A-06) ; une
      **polyligne** → édition de polyligne (M-09) ; un **bloc** → éditeur de
      blocs (D-09) ; **tout autre objet** → le panneau Propriétés sur cet
      objet (AutoCAD ouvre ses Propriétés rapides, P-10) ; un objet de module
      → l'action que déclare le module (ex. parcelle → ses propriétés). Rappel
      : **double-clic molette** = zoom étendu, tâche V-06.
      *Où, comme AutoCAD* : Options › onglet **Préférences utilisateur** ›
      cadre « Comportements standard Windows » › case **« Double-clic pour
      modifier »** (activée par défaut).
      *Où dans BCAD* : `Viewport::mouseDoubleClickEvent`
      (`ViewportInput.cpp`, ne traite aujourd'hui que le texte) émet une
      demande d'édition de l'objet ; `MainWindow` choisit l'action selon le
      type ; la case dans la page Préférences utilisateur de S-15.

### Sélection

État : au repos, clic sur un objet, fenêtre glissée (gauche → droite), capture
(droite → gauche), Maj/Ctrl bascule un objet, Échap vide, tout, dernier (S-09).
Les gestes diffèrent encore d'AutoCAD sur plusieurs points : voir S-10.

- [ ] **S-10 Sélection à la souris fidèle à AutoCAD** — P1 · M · `ACAD`
      PICKADD, PICKAUTO, PICKDRAG, SELECTIONPREVIEW, lasso · ⚠️ constaté le
      2026-10-04 en relisant `src/app/ViewportInput.cpp` (`selectAt`,
      `mouseReleaseEvent`) contre le comportement par défaut d'AutoCAD.
      Écarts à combler :

      | Geste | AutoCAD (réglages par défaut) | BCAD aujourd'hui |
      |---|---|---|
      | Clic sur un 2ᵉ objet | **s'ajoute** à la sélection (PICKADD = 2) | **remplace** la sélection |
      | Clic dans le vide | commence une fenêtre, **garde** la sélection | **vide** la sélection |
      | Fenêtre | clic, puis 2ᵉ clic ailleurs, bouton relâché (PICKDRAG = 0) | seulement en glissant |
      | Appui-glissé depuis le vide | **lasso** (forme libre ; fenêtre ou capture selon le sens) | rectangle |
      | Survol d'un objet | **surbrillance** avant le clic (SELECTIONPREVIEW) | rien |
      | Maj + clic | **retire** de la sélection | bascule (ajoute ou retire) |
      | Capture (droite → gauche) | objets dont le **tracé** touche la fenêtre | objets dont le **rectangle englobant** touche la fenêtre (trop large : un arc ou une polyligne en L peuvent être pris sans être touchés) |
      | Échap, fenêtre gauche → droite | vide ; objets entièrement dedans | ✅ identique |

      Fini quand : chaque ligne du tableau est couverte par un cas de
      `tests/unit/app/ViewportToolsTest.cpp` (clic, ajout, Maj, clic dans le
      vide, fenêtre en deux clics, lasso dans les deux sens, capture sur le
      tracé) ; la surbrillance au survol se voit au canevas ; la désignation
      des objets d'une commande (`pickingObjects_`) garde les mêmes gestes ;
      `UI_CONVENTIONS.md` § « Cycle d'une commande » décrit le résultat.
      Liens : M-02 Poignées (bleues sur la sélection) est une tâche séparée ;
      SEL-02 (polygone, trajet) et SEL-06 (cyclage Maj+Espace) la complètent.

- [ ] **SEL-01 Inverser la sélection, tout désélectionner** — P1 · S · `LC`,
      `QCAD` Invert Selection.
- [ ] **SEL-02 Sélection par polygone et par trajet** (objets coupés par une
      ligne brisée) — P2 · M · `ACAD WP/CP/F`, `QCAD/LC` Select Intersected.
- [ ] **SEL-03 Sélection d'un contour** (chaîne d'objets connectés) — P2 · S ·
      `QCAD/LC` (De-)Select Contour.
- [ ] **SEL-04 Sélection par calque** — P2 · S · `QCAD/LC` (De-)Select Layer.
- [ ] **SEL-05 Désélection par fenêtre** — P2 · S · `LC` Deselect Window.
- [ ] **SEL-06 Cyclage des objets superposés** — P2 · S · `ACAD` (Maj+Espace).

### Édition

- [ ] **E-01 Presse-papiers** : copier, couper, coller (Ctrl+C / X / V), coller
      avec point de base, entre deux dessins — P1 · M · `ACAD COPYCLIP /
      COPYBASE / PASTECLIP`, `QCAD` Cut / Copy with Reference · ❌.
- [ ] **E-02 Déplacement fin au clavier** (flèches : pas réglable, rotation
      ±90°) — P2 · S · `QCAD` Quick Modify.
- [ ] **E-03 Rechercher / remplacer** dans les textes — P2 · S · `ACAD FIND`,
      `QCAD` Find/Replace.
- [ ] **E-04 Dupliquer sur place / coller le long d'un objet** — P3 · S ·
      `QCAD` Duplicate, Paste along Entity.

### Unités

- [ ] **U-01 Unités du dessin** (m, mm, cm), précision d'affichage, unité
      d'angle (degrés, grades — usuels en topographie) et conversion d'un
      dessin — P1 · M · `ACAD UNITS`, `QCAD` Convert Drawing Unit · ❌ aucune
      notion d'unité dans le document (le cadastre suppose des mètres).
      Boîte d'AutoCAD à reprendre et emplacement : voir le complément à U-01
      dans la partie 4 (§ 4.11).

## Lot 4 — Les bons outils de QCAD et LibreCAD absents d'AutoCAD

Ce qu'AutoCAD obtient par accrochages ou options, QCAD et LibreCAD en font des
outils directs — plus rapides à apprendre, ce qui compte pour l'usage
éducation / TP (`ROADMAP_MARKET.md` étape 8).

### Dessin
- [ ] **Q-01 Parallèle par un point** (en plus de la distance) — P1 · S ·
      `QCAD` Parallel through Point · s'appuie sur M-01.
- [ ] **Q-02 Ligne tangente** point → cercle, et à deux cercles — P1 · M ·
      `QCAD` Tangent (Point, Circle), Tangent (Two Circles).
- [ ] **Q-03 Ligne perpendiculaire / à angle relatif** à une entité — P1 · S ·
      `QCAD` Orthogonal, Relative Angle · `LC` Angle line.
- [ ] **Q-04 Bissectrice** de deux lignes — P2 · S · `QCAD` Angle Bisector.
- [ ] **Q-05 Lignes horizontale / verticale directes** — P2 · S · `LC lh/lv`,
      `QCAD` (l'ortho F8 couvre le cas, l'outil le rend évident).
- [ ] **Q-06 Grille de points M×N** — P2 · S · `QCAD` MxN Points.
- [ ] **Q-07 Polylignes équidistantes** (plusieurs parallèles d'un coup) — P2 · S
      · `LC pe`, `QCAD` Offset de polyligne.
- [ ] **Q-08 Rectangle par dimensions / 3 points** — P2 · S · `QCAD` Rectangle
      with Size, Rectangle (3 Points).
- [ ] **Q-09 Étoile** — P3 · S · `QCAD` Star.

### Modification
- [ ] **Q-10 Rognage automatique** (clic sur le morceau à retirer, entre deux
      intersections) — P1 · M · `QCAD` Auto Trim. Plus rapide que TRIM pour
      nettoyer un levé.
- [ ] **Q-11 Rogner les deux** (coin entre deux lignes) — P1 · S · `QCAD` Trim Both.
- [ ] **Q-12 Retirer un segment entre deux intersections / avec un écart** —
      P2 · M · `QCAD` Break out Segment, Break out Gap.
- [ ] **Q-13 Découper au rectangle** (clip) — P2 · M · `QCAD` Clip to Rectangle.
      Utile pour extraire une zone d'un plan importé.
- [ ] **Q-14 Déplacer et tourner** en une opération, **Tourner deux fois** —
      P2 · S · `QCAD` Move and Rotate, Rotate Two.
- [ ] **Q-15 Aligner sur points de référence** (calage par 2 paires de points) —
      P1 · M · `QCAD` Align Reference Points, `ACAD ALIGN`. Calage d'un levé ou
      d'un plan importé.
- [ ] **Q-16 Retournement horizontal / vertical** — P3 · S · `QCAD` Flip H/V.
- [ ] **Q-17 Inverser le sens** d'une ligne / polyligne — P2 · S · `QCAD`
      Reverse, `LC` Revert direction.
- [ ] **Q-18 Simplifier une polyligne** (sommets alignés, doublons) — P2 · S ·
      `QCAD` Simplify. Nettoyage de levés et d'imports.
- [ ] **Q-19 Déplacer le point de départ** d'une polyligne fermée — P3 · S ·
      `QCAD` Relocate Start Point.
- [ ] **Q-20 Sommets aux auto-intersections** — P3 · S · `QCAD` Insert Nodes at
      Self-Intersections (pré-traitement des booléens).
- [ ] **Q-23 Diviser une entité** en N parties ou en un point (sans les
      séparer visuellement) — P2 · S · `QCAD` Divide / Split Entities, `LC` Divide.
- [ ] **Q-24 Supprimer les nœuds entre deux nœuds** / ajouter un nœud en fin —
      P2 · S · `LC` Delete between two nodes, `QCAD` Append Node (complète M-09).
- [ ] **Q-25 Détecter les entités de longueur nulle** — P2 · S · `QCAD`
      Detection (avec M-15).

### Annotation
- [ ] **Q-21 Réinitialiser la position du texte de cote** — P2 · S · `QCAD`
      Reset Label Position (après A-01).
- [ ] **Q-22 Repère de référence** (datum) — P3 · S · `QCAD` Datum.

### Information (QCAD / LibreCAD : menu Info)
- [ ] **I-03 Longueur totale** de la sélection — P1 · S · `LC/QCAD` Info.
- [ ] **I-04 Distance point–entité** — P2 · S · `LC/QCAD` Info.
- [ ] **I-05 Liste des propriétés** d'une entité (coordonnées, longueur, rayon)
      — P2 · S · `ACAD LIST`, `QCAD` Info.
- [ ] **I-07 Distance entité–entité** — P2 · S · `QCAD` Info.

## Lot 5 — P2 / P3, à faire quand un besoin réel le demande

### Dessin
- [ ] **D-13 Ellipse / arc elliptique** — P2 · M · `ACAD ELLIPSE`, `QCAD/LC`
      (variantes LibreCAD : foyers, 4 points, centre + 3 points, inscrite).
- [ ] **D-19 Arc tangent en continuation** du dernier segment — P2 · S · `ACAD
      ARC` Continue, `LC` Arc Tangential, `QCAD` Tangentially Connected.
- [ ] **D-20 Dégradé** (`GRADIENT`) — P3 · M · `ACAD` (après D-02).
- [ ] **D-21 Champ** (texte calculé : date, nom de fichier, surface d'un
      objet) — P3 · M · `ACAD FIELD`. Le cartouche a déjà ses champs résolus
      (ADR-017) ; utile hors cartouche seulement.
- [ ] **D-22 Vectoriser une image** — P3 · L · `QCAD` Trace Bitmap.
- [ ] **D-14 Spline** (points de contrôle / d'ajustement) — P2 · L · `ACAD SPLINE`,
      `QCAD` (+ insérer / retirer un point).
- [ ] **D-15 Ligne de construction / demi-droite** — P2 · M · `ACAD XLINE/RAY`.
- [ ] **D-16 Contour / région** depuis un point intérieur — P2 · M · `ACAD
      BOUNDARY/REGION` (partagé avec D-02).
- [ ] **D-17 Masque** (wipeout) — P2 · S · `ACAD WIPEOUT`, `QCAD`.
- [ ] **D-18 Anneau, nuage de révision, main levée** — P3 · S · `ACAD
      DONUT/REVCLOUD/SKETCH`, `QCAD` Ring, Freehand Line.

### Modification
- [ ] **M-12 Allonger** (`LENGTHEN`) — P2 · S · `ACAD`, `QCAD` Lengthen / Shorten.
- [ ] **M-13 Scinder entre deux points**, et polylignes fermées — P2 · M · `ACAD
      BREAK` · ⚠️ un point, polylignes ouvertes.
- [ ] **M-14 Ordre de tracé** (premier / arrière-plan) — P2 · S · `ACAD
      DRAWORDER`, `QCAD`.
- [ ] **M-15 Supprimer les doublons** (`OVERKILL`) — P2 · M · `ACAD OVERKILL`.
      Utile après un import DXF.
- [ ] **M-16 Sélection rapide** par type / calque / propriété — P2 · M · `ACAD
      QSELECT`.
- [ ] **M-17 Symétrie avec effacement de la source** (option) — P3 · S · `ACAD MIRROR`.
- [ ] **M-18 Raccord de courbes** (`BLEND`) — P3 · L · `ACAD BLEND`.
- [ ] **M-19 Aligner des textes** — P3 · S · `ACAD TEXTALIGN`.
- [ ] **M-20 Éclater un texte en lettres** — P3 · S · `LC`.
- [ ] **M-21 Projection isométrique** d'une sélection — P3 · M · `QCAD`
      Projection.

### Annotation
- [ ] **A-10 Longueur d'arc** — P2 · S · `ACAD DIMARC`, `QCAD DG`.
- [ ] **A-11 Cotation rapide** (`QDIM`) — P2 · M · `ACAD QDIM`.
- [x] **A-12 Cotations rayon/diamètre par désignation du cercle** — P2 · S · ⚠️
      aujourd'hui centre + point. **Fait (2026-10-04)** avec A-01 : un clic sur
      un cercle ou un arc en prend centre et rayon (seuls cercles et arcs se
      désignent) ; centre + point reste possible.
- [ ] **A-13 Échelle d'annotation** — P2 · L · `ACAD ANNOSCALE`.
- [ ] **A-14 Tolérance géométrique, marque de centre, ligne d'axe** — P3 · S ·
      `ACAD TOLERANCE/CENTERMARK/CENTERLINE`, `QCAD`.
- [ ] **A-15 Interrompre une cote / espacer des cotes** — P2 · S · `ACAD
      DIMBREAK/DIMSPACE`.
- [ ] **A-16 Retirer les surcharges de style** d'une cote — P3 · S · `QCAD`
      Remove Style Overrides.

---

## Ordre d'implémentation conseillé

1. **Lot 1** en entier (≈ 1,5 semaine) : le moteur est déjà écrit, chaque outil
   est surtout une entrée `kTools`, une consigne et un test. **C-01 Commandes
   au clavier** d'abord (chaque outil suivant en profite), puis M-01 Décaler et
   I-02 Surface : les plus demandés par un géomètre.
2. **Lot 2** : Poignées et Hachures sont les deux gros morceaux (L) ; Étirer,
   Raccord et Import de levé suivent.
3. **Lot 3** dans l'ordre : S-02/S-04/S-08 et SEL-01 (aides rapides, S), U-01
   Unités, E-01 Presse-papiers, D-04/D-05/D-06/D-07 (variantes de tracé, S),
   A-03/A-04/A-05 (cotations), M-06/M-07/M-09, puis D-08 Polyligne à arcs
   (prérequis de plusieurs autres) et D-09 Blocs.
4. **Lot 4** au fil de l'eau : Q-10 Rognage automatique et Q-15 Aligner sur
   points de référence en premier (nettoyage et calage de levés).
5. **Lot 5** seulement sur besoin exprimé.

*Écart à cet ordre (2026-10-04)* : à la demande du mainteneur, l'annotation est
passée devant — ruban réorganisé, texte (D-01, D-01b) et cotations (A-01, A-02,
A-12) livrés avant le Lot 1. Le Lot 1 reste la suite naturelle des P0.

## Partie 2 — Affichage, Calques, Propriétés (onglet Accueil)

Même méthode que la partie 1 : AutoCAD (panneaux **Calques** et **Propriétés**
de l'onglet Accueil, palette Propriétés, gestionnaire des propriétés des
calques, onglet **Affichage** et barre de navigation), QCAD (menu Affichage,
liste des calques, éditeur de propriétés, menu Calque), LibreCAD. État BCAD
vérifié dans `LayerPanel.cpp`, `PropertiesPanel.cpp`, `Layer.h`,
`LayerManager.h`, `GlRenderer.cpp`, `ViewportInput.cpp`, `ViewportOverlay.cpp`.

Préfixes : `V-` affichage · `L-` calques · `P-` propriétés · `B-` défaut constaté.

### Déjà livré (ne pas refaire)

- [x] Zoom molette centré sur le curseur, panoramique au bouton du milieu
- [x] Zoom étendu (`F`), grille adaptative au zoom (F7)
- [x] Bascules d'accrochage (F3), accrochage grille (F9), ortho (F8) en barre d'état
- [x] Liste des calques : visible, verrouillé, couleur, épaisseur, type de ligne
- [x] Ajouter, renommer (double-clic), supprimer un calque ; calque courant
- [x] Isoler un calque, tout afficher ; états de calques enregistrer / restaurer
- [x] Recherche et filtre dans la liste des calques
- [x] Calques déclarés par les modules (`IStyleProvider`, gabarits pays)
- [x] Panneau Propriétés : calque et couleur (par calque / personnalisée) de la
      sélection simple ou multiple, valeurs mixtes signalées, annulable
- [x] Propriétés déclarées par les modules (texte, liste, réel, entier, booléen)
- [x] Ruban d'AutoCAD récent en français (V-00) : Accueil, Insertion, Annoter ;
      zoom sur tout, bascules des panneaux et aides au dessin dans le coin
      inférieur droit de la barre d'état (2026-10-04)
- [x] Plusieurs dessins ouverts, un onglet chacun, bouton « + » (V-09)
- [x] Défauts B-01 à B-04 corrigés (calque « (Mixte) », suppression d'un calque
      occupé, libellés anglais, extension à l'enregistrement)
- [x] Champ réel du panneau Propriétés : plus de troncature à 99,99 (2026-10-04)

### P0 — défauts et manques bloquants

- [x] **V-00 Ruban organisé comme AutoCAD récent** — demandé par le
      mainteneur (2026-10-04). **Fait** : `Accueil` (Dessin, Modification,
      Annotation, Calques, Bloc, Propriétés, Groupes, Utilitaires,
      Presse-papiers), `Insertion` (Bloc, Définition de bloc, Référence,
      Importer, Données, Liaison et extraction, Localisation), `Annoter`
      (Texte, Cotation, Lignes d'axe, Lignes de repère, Tableaux) ;
      l'affichage passe dans le coin inférieur droit de la barre d'état ; les
      outils pas encore réalisés sont grisés avec leur tâche en infobulle.
      Vérifié sur la vraie fenêtre par `mainwindow_workbench_test`. Prochaines
      livraisons demandées : enrichir les blocs **Texte** et **Cotation**.
- [x] **B-01 Choisir « (Mixte) » change le calque des objets en « (Mixte) »** —
      P0 · S · défaut. `PropertiesPanel.cpp` ajoute l'entrée `tr("(Mixte)")`
      mais ignore `tr("(Mixed)")` : le garde ne protège rien. Avec test.
      **Fait (2026-10-03)** : le calque cible est porté par la donnée de
      l'entrée de liste, plus par son texte — l'espace réservé n'en a pas, et un
      calque réellement nommé « (Mixte) » reste choisissable.
      `properties_panel_layer_test` (vrai widget, hors écran) échoue sur
      l'ancien code et passe sur le nouveau.
- [x] **B-02 Supprimer un calque laisse ses objets sur un calque inexistant** —
      P0 · S · défaut. `LayerManager::removeLayer` retire le calque sans toucher
      aux entités. AutoCAD refuse de supprimer un calque non vide (`LAYDEL` est
      la commande explicite) : refuser, ou proposer de déplacer les objets sur
      `0` — dans une seule étape d'annulation.
      **Fait (2026-10-03)** : comme AutoCAD, le panneau Calques refuse de
      supprimer un calque occupé et dit combien d'objets il porte (message non
      bloquant en barre d'état) ; le calque 0 est refusé avec explication ; un
      calque vide part sans question. `layer_panel_remove_test` (vrai widget)
      échoue sur l'ancien code. La suppression avec ses objets reste L-09.
- [x] **B-03 Libellés anglais** dans les calques et les propriétés (« Set as
      current layer », « Isolate layer », « Change Layer », « Entity Color »,
      « Edit property ») — P0 · S.
      **Fait (2026-10-04)** : panneau Calques (menu contextuel, propriétés du
      calque, états, nouveau calque, types de ligne « Continu, Tirets,
      Pointillés, Tiret-point » — libellés seulement, la valeur se lit à
      l'index), panneau Propriétés (libellés d'annulation, boîte de couleur),
      et les informations géométriques du noyau affichées dans ce panneau
      (« Ligne / Longueur », « Cercle / Rayon », « Polyligne (fermée) /
      Surface », cotations).
- [x] **B-04 « Enregistrer sous » sans extension** — P0 · S · défaut constaté
      en test manuel (fichiers `1` et `fichier1` créés sans `.bcad`, donc
      invisibles dans la boîte d'ouverture filtrée). **Fait (2026-10-04)** :
      l'extension est ajoutée si elle manque — projet `.bcad`, export DXF et
      exports des modules (extension déclarée par l'exporteur) — et un fichier
      existant sous le nom complété est confirmé avant d'être remplacé
      (`SaveDialog.h`, `save_dialog_test`).
- [ ] **L-01 Rendu de l'épaisseur et du type de ligne des calques** — P0 · M ·
      `ACAD LWDISPLAY`, `QCAD` · ⚠️ stockés dans `Layer` mais dessinés en trait
      continu de 1 px (`glLineWidth(1.0f)`). Sans eux, une limite cadastrale
      en tiret-point ou un trait fort ne se voient ni à l'écran ni à
      l'impression (`TODO.md` §7.1, `plot_styles` des gabarits).
- [ ] **L-02 Liste des calques dans le ruban** (onglet Accueil, panneau
      Calques) : calque courant, et bascules visible / verrouillé en un clic
      sans ouvrir le panneau — P0 · S · `ACAD` panneau Calques · ❌ le calque
      courant ne se change que par le menu contextuel du panneau.
- [ ] **P-01 Propriétés géométriques éditables** : coordonnées de début et de
      fin, centre, rayon, angles, longueur, aire ; sommets d'une polyligne —
      P0 · M · `ACAD` palette Propriétés, `QCAD` éditeur de propriétés · ❌
      seuls calque, couleur et propriétés de module s'affichent. Saisie d'une
      valeur = commande annulable.
- [ ] **V-01 Zoom fenêtre** — P0 · S · `ACAD ZOOM F`, `QCAD/LC` Window Zoom · ❌.
- [ ] **V-02 Zoom précédent / suivant** — P0 · S · `ACAD ZOOM P`, `QCAD`
      Previous View · ❌.
- [ ] **V-03 Outil Panoramique** (main, au bouton gauche) — P0 · S · `ACAD PAN`,
      barre de navigation · ❌ le panoramique exige un bouton du milieu : rien
      sur un pavé tactile, matériel courant sur le poste de référence (ADR-016).

### P1 — attendu par un utilisateur d'AutoCAD

Affichage :
- [ ] **V-04 Zoom avant / arrière par pas** (boutons, `+` / `-`, molette sur un
      pavé) — P1 · S · `ACAD ZOOM`, `QCAD` Zoom In / Out.
- [ ] **V-05 Zoom sur la sélection** — P1 · S · `ACAD ZOOM O`, `QCAD` Zoom to
      Selection.
- [ ] **V-06 Double-clic molette = zoom étendu** — P1 · S · `ACAD`.
- [ ] **V-07 Barre de navigation** au bord du canevas (panoramique, étendu,
      fenêtre, précédent) — P1 · S · `ACAD` Navigation Bar.
- [ ] **V-08 Réglage de la grille et de l'accrochage grille** (pas, sous-
      division, origine) — P1 · S · `ACAD DSETTINGS` · ⚠️ grille adaptative,
      pas non réglable.
- [x] **V-09 Plusieurs dessins ouverts** (onglets de documents) — P1 · L ·
      `ACAD` onglets de fichiers · ❌ un seul document par fenêtre.
      **Fait (2026-10-03)**, demandé par le mainteneur : onglets entre le ruban
      et le canevas (nom du fichier, `*` si modifié, chemin en infobulle, ×
      pour fermer, `+` pour un nouveau dessin, déplaçables) ; chaque dessin a
      son historique (`QUndoGroup`), son cadrage et son état. Ouvrir accepte
      plusieurs fichiers ; un fichier déjà ouvert ramène à son onglet ; un
      dessin vierge intact est remplacé ; Import DXF ouvre un onglet au lieu
      d'écraser ; fermer demande pour chaque dessin modifié ; sauvegarde
      automatique de tous les dessins ; Ctrl+Tab / Ctrl+Maj+Tab, Ctrl+W.
      Tests : `document_sessions_test`, `mainwindow_tabs_test` (vraie
      fenêtre hors écran, module cadastre chargé).

Calques :
- [ ] **L-03 Calque imprimable / non imprimable** — P1 · S · `ACAD` colonne
      Tracer, `QCAD` Plottable · ❌. Calques de construction non imprimés.
- [ ] **L-04 Annuler les changements de calques** — P1 · M · `ACAD LAYERP` · ❌
      visibilité, verrou, couleur, renommage ne passent pas par la pile
      d'annulation.
- [ ] **L-05 Rendre courant le calque d'un objet** — P1 · S · `ACAD LAYMCUR`,
      `QCAD` Activate Layer of Entity.
- [ ] **L-06 Mettre la sélection sur le calque courant / sur le calque d'un
      objet cible** — P1 · S · `ACAD LAYCUR / LAYMATCH`, `QCAD` Move Selection
      to Current Layer.
- [ ] **L-07 Isoler / désisoler depuis la sélection** — P1 · S · `ACAD
      LAYISO / LAYUNISO` · ⚠️ isoler par nom de calque existe.
- [ ] **L-08 Fusionner des calques** — P1 · S · `ACAD LAYMRG`. Nettoyage après
      import DXF.
- [ ] **L-09 Supprimer un calque et ses objets / purger les calques vides** —
      P1 · S · `ACAD LAYDEL / PURGE`, `QCAD` Purge Unused Layers (après B-02).
- [ ] **L-10 Types de ligne chargés** (bibliothèque `.lin`, tiret-point,
      limite, clôture) et **échelle de type de ligne** — P1 · M · `ACAD
      LINETYPE / LTSCALE` · ⚠️ quatre types fixes. Prérequis des limites
      cadastrales réglementaires.

Propriétés :
- [ ] **P-02 Épaisseur et type de ligne par objet** (« Par calque » ou valeur)
      — P1 · M · `ACAD` panneau Propriétés · ❌ l'entité ne porte qu'une couleur
      de substitution.
- [ ] **P-03 Listes couleur / épaisseur / type de ligne dans le ruban**
      (panneau Propriétés de l'onglet Accueil) — P1 · S · `ACAD`.
- [ ] **P-04 Couleurs AutoCAD indexées** (ACI 1–255 : rouge, jaune, vert…) en
      plus du sélecteur RVB — P1 · S · `ACAD` Sélectionner une couleur ·
      ⚠️ sélecteur RVB seul ; l'échange DXF repose sur l'ACI.
- [ ] **P-05 Propriétés des textes et cotations** (contenu, hauteur, style,
      précision) — P1 · S · après D-01 et A-01.
- [ ] **P-06 Copier les propriétés** — voir M-10 (`MATCHPROP`).

### P2 — confort

- [ ] **V-10 Vues nommées** (enregistrer / restaurer un cadrage) — P2 · M ·
      `ACAD VIEW`, `QCAD` Stored Views.
- [ ] **V-11 Mode brouillon** (performance sur gros plans) — P2 · S · `QCAD`
      Draft Mode. Poste modeste (ADR-016).
- [ ] **V-12 Écran épuré / plein écran** — P2 · S · `ACAD CLEANSCREEN` (Ctrl+0).
- [ ] **V-13 Repère des axes et origine** affichés dans le canevas — P2 · S ·
      `ACAD` icône SCU.
- [ ] **V-14 Bascule des panneaux** (Ctrl+1 Propriétés, calques) — P2 · S ·
      `ACAD`.
- [ ] **L-11 Geler / libérer** (exclu de l'affichage, de l'accrochage et de la
      sélection) — P2 · S · `ACAD LAYFRZ`, `QCAD` Frozen.
- [ ] **L-12 Désactiver / verrouiller le calque d'un objet désigné** — P2 · S ·
      `ACAD LAYOFF / LAYLCK / LAYULK`.
- [ ] **L-13 Transparence par calque** — P2 · M · `ACAD` colonne Transparence.
- [ ] **L-14 Filtres de calques** par propriété et groupes — P2 · M · `ACAD` ·
      ⚠️ recherche texte seulement.
- [ ] **L-15 Copier des objets vers un autre calque** — P2 · S · `ACAD COPYTOLAYER`.
- [ ] **L-16 Créer un calque depuis la sélection** — P2 · S · `QCAD` Create
      Layer from Selection.
- [ ] **P-07 Filtre par type dans une sélection mixte** (« Ligne (2) »,
      « Cercle (1) ») — P2 · S · `ACAD` palette Propriétés.
- [ ] **P-08 Transparence par objet** — P2 · S (après L-13).
- [ ] **P-09 Groupes d'objets** (grouper, dégrouper, sélectionner le groupe) —
      P2 · M · `ACAD GROUP` (panneau Groupes de l'onglet Accueil).

### P3 — marginal

- [ ] **V-15 Fenêtres multiples dans le canevas** — P3 · L · `ACAD VPORTS`.
- [ ] **V-16 Grille isométrique** — P3 · S · `QCAD` Isometric Grid.
- [ ] **V-17 Superpositions** (sens, ordre, point de départ des objets) — P3 · S
      · `QCAD` Overlay.
- [ ] **L-17 Parcourir les calques** — P3 · S · `ACAD LAYWALK`.
- [ ] **L-18 Sous-calques** (hiérarchie) — P3 · M · `QCAD` Add Sublayer.
- [ ] **L-19 Calques non accrochables** — P3 · S · `QCAD` Snappable.
- [ ] **P-10 Propriétés rapides** au survol — P3 · S · `ACAD QPMODE`.

### Barre d'état et coins inférieurs, comme AutoCAD

Constat du 2026-10-04. Disposition d'AutoCAD 2025 (version française, de
mémoire : **libellés et ordre à vérifier** sur une installation avant de les
reprendre) :

| Emplacement | AutoCAD | BCAD aujourd'hui |
|---|---|---|
| Coin inférieur gauche, barre d'état | onglets **Objet**, **Présentation1**, **Présentation2**…, bouton **+**, flèche de liste | coordonnées X, Y et messages (`coordLabel_`, `MainWindow.cpp`) |
| Coin inférieur gauche, zone de dessin | **icône du SCU** (flèches X, Y) | rien (V-13) |
| Au-dessus de la barre d'état | ligne de commande, ancrée ou flottante | ligne de commande ancrée ✅ |
| Bord droit de la zone de dessin | **barre de navigation** (panoramique, zoom, roue) ; ViewCube en haut à droite (3D) | rien (V-07) |
| Coin inférieur droit, barre d'état | les outils ci-dessous, puis le menu **Personnalisation (≡)** tout à droite | nom de l'outil en cours (`toolLabel_`), zoom sur tout, bascules Calques / Propriétés / Vérifications, F3, F7, F9, F8 |

Outils du coin inférieur droit d'AutoCAD, de gauche à droite (ceux marqués
*masqué* ne s'affichent qu'après les avoir cochés dans ≡) :

| Bouton AutoCAD | Touche | BCAD |
|---|---|---|
| Coordonnées (*masqué*) | — | ✅ mais à gauche (`coordLabel_`) |
| Objet / Papier | — | ❌ (avec V-22) |
| Grille | F7 | ✅ |
| Mode Accrochage (grille) | F9 | ✅ |
| Saisie dynamique (*masqué*) | F12 | ❌ S-07 |
| Ortho | F8 | ✅ |
| Repérage polaire | F10 | ❌ S-03 |
| Dessin isométrique | F5 (plan) | ❌ V-16 |
| Repérage d'accrochage aux objets | F11 | ❌ S-06 |
| Accrochage aux objets (flèche : modes et paramètres) | F3 | ✅ sans flèche (V-19, S-02) |
| Épaisseur de ligne | — | ❌ L-01 |
| Transparence | — | ❌ L-13 / P-08 |
| Cycle de sélection | Ctrl+W | ❌ SEL-06 |
| Échelle d'annotation « 1:1 » | — | ❌ A-13 |
| Espace de travail (roue dentée) | — | ❌ hors périmètre |
| Unités | — | ❌ U-01 |
| Propriétés rapides | — | ❌ P-10 |
| Verrouiller l'interface | — | ❌ hors périmètre |
| Isoler les objets | — | ❌ L-07 |
| Accélération matérielle | — | ❌ hors périmètre (V-11 mode brouillon) |
| Écran épuré | Ctrl+0 | ❌ V-12 |
| Personnalisation (≡) | — | ❌ V-20 |

**Écart voulu** : zoom sur tout et bascules des panneaux sont dans ce coin à la
demande du mainteneur (V-00, « l'onglet Affichage dans le coin inférieur
droit ») ; AutoCAD ne les y met pas (zoom : barre de navigation, molette ;
panneaux : ruban et Ctrl+1). À garder sauf décision contraire du mainteneur.

- [ ] **V-18 Barre d'état ordonnée comme AutoCAD** — P1 · S · `ACAD`. Dans le
      coin inférieur droit, l'**ordre du tableau** ci-dessus ; les
      **coordonnées en tête** de ce groupe (place qu'AutoCAD leur donne quand
      on les affiche ; un géomètre les veut visibles, donc affichées par
      défaut) ; le **nom de l'outil en cours** retiré (AutoCAD l'écrit dans
      la ligne de commande, que BCAD renseigne déjà par la consigne) ; puis,
      séparés par un trait, zoom et panneaux (écart voulu). Chaque bouton
      d'une tâche livrée prend sa place à sa livraison ; aucun bouton grisé
      (contrairement au ruban).
      *Où dans BCAD* : `coordLabel_` et `toolLabel_` (`MainWindow.cpp`,
      constructeur, `statusBar()->addWidget / addPermanentWidget`) ;
      `addStatusButton` et la table `toggles` (`MainWindowMenus.cpp`).
- [ ] **V-19 Flèche d'options sur les boutons de la barre d'état** — P1 · S ·
      `ACAD`. **Accrochage aux objets** : liste des modes à cocher
      (Extrémité, Milieu, Centre, Intersection, Perpendiculaire…) et
      « Paramètres d'accrochage aux objets… » (S-02) ; **Grille** : « Paramètres
      de la grille… » (V-08) ; **Repérage polaire** : angles 90, 45, 30,
      22,5, 15… (S-03). Le **clic droit** sur le bouton ouvre le même menu.
      *Où dans BCAD* : `addStatusButton` (`MainWindowMenus.cpp`) —
      `QToolButton::MenuButtonPopup` et un `QMenu` par bouton.
- [ ] **V-20 Menu de personnalisation de la barre d'état (≡)** — P2 · S ·
      `ACAD` bouton Personnalisation. Tout à droite : liste à cocher de
      chaque élément de la barre d'état (coordonnées, chaque bascule, zoom,
      panneaux) ; choix gardé par utilisateur (`QSettings`), jamais dans le
      document.
      *Où dans BCAD* : dernier widget permanent de la barre d'état
      (`MainWindowMenus.cpp`).
- [ ] **V-21 Touches de fonction d'AutoCAD** — P1 · S · `ACAD`. **F1** aide ;
      **F2** historique de la ligne de commande agrandi ; **F3** ✅ ; **F7** ✅ ;
      **F8** ✅ ; **F9** ✅ ; **F10** repérage polaire (S-03) ; **F11** repérage
      d'objet (S-06) ; **F12** saisie dynamique (S-07) ; **Ctrl+0** écran
      épuré (V-12) ; **Ctrl+1** Propriétés (V-14) ; **Ctrl+W** cycle de
      sélection (SEL-06). Chaque touche affichée dans l'infobulle de son
      bouton. Écart à trancher : `F` = zoom sur tout dans BCAD (raccourci
      QCAD), sans équivalent AutoCAD (zoom étendu = double-clic molette, V-06,
      ou `Z` Entrée `E` Entrée avec C-01).
      *Où dans BCAD* : raccourcis des `QAction` (`MainWindowMenus.cpp`,
      `MainWindowTools.cpp`) ; F2 bascule la hauteur de la ligne de commande
      (`buildCommandLine`, `MainWindow.cpp`).
- [ ] **V-22 Onglets Objet / Présentations** dans le coin inférieur gauche —
      P2 · L · `ACAD` Model / Layout tabs. « Objet » est le canevas ; une
      présentation est une **feuille d'impression** (cartouche, fenêtres de
      vue à l'échelle) ; « + » en ajoute une. Rejoint la mise en page suivie
      dans `TODO.md` (feuilles PDF, ADR-017) : à faire quand les feuilles
      deviennent éditables à l'écran, pas avant.
      *Où dans BCAD* : en tête de la barre d'état, à gauche
      (`statusBar()->addWidget`, `MainWindow.cpp`), à la place des
      coordonnées qui passent à droite (V-18).

Placement des deux éléments du canevas déjà recensés : **V-13** icône du SCU
dans le **coin inférieur gauche du canevas** (dessinée par
`ViewportOverlay.cpp`, au-dessus de la grille) ; **V-07** barre de navigation
**le long du bord droit** du canevas, en haut.

### Ordre conseillé pour la partie 2

1. Les défauts **B-01, B-02, B-03** (une demi-journée, avec tests).
2. **V-01 à V-03** (navigation : un géomètre sur portable n'a souvent pas de
   bouton du milieu), puis **L-02** liste des calques dans le ruban.
3. **L-01** rendu épaisseur / type de ligne, puis **L-10** types de ligne
   chargés : sans eux, le plan imprimé n'a pas ses conventions graphiques.
4. **P-01** propriétés géométriques, puis le reste des P1.

## Partie 3 — Module cadastre adapté à l'Afrique

Méthode : lecture du module (`src/plugins/cadastre/`, 4 486 lignes : entités,
commandes, validateurs, mise en page, exports, gabarits) et de ses documents
(`CADASTRE_SPEC.md`, `CADASTRE_PLUGIN_STATUS.md`) ; recherche sur les
procédures foncières de l'Afrique de l'Ouest (Togo, Bénin, Côte d'Ivoire,
Sénégal, Burkina Faso), leurs systèmes de coordonnées, la norme ISO 19152
(LADM) et les outils fonciers utilisés sur le continent (STDM / ONU-Habitat,
SOLA et Open Tenure / FAO, Covadis). Préfixe `K-`. Les codes `C4`, `D3`, `E2`,
`F1`… renvoient à `CADASTRE_SPEC.md`.

### Ce que demandent les procédures (constats de la recherche)

| Pays | Titre / acte | Ce que le géomètre doit livrer |
|---|---|---|
| Togo | Titre foncier (OTR / DCCFE) | **Plan parcellaire géoréférencé** (obligatoire depuis mai 2024), bornage **contradictoire** en présence des riverains, procès-verbal de bornage |
| Bénin | Certificat de propriété foncière (ANDF) | Levé topographique par géomètre agréé ANDF, bornage avec **convocations 15 jours avant**, plan en 2 exemplaires |
| Côte d'Ivoire | Arrêté de concession définitive (ACD) | Dossier technique : plan, **tableau des calculs de coordonnées**, **tableau des calculs de surface**, PV de bornage, **plan de situation**, rapport du géomètre, calculs de géoréférencement, copies A3 ; lot vérifié contre le **plan de lotissement approuvé** (îlot / lot) |
| Sénégal | NICAD (DGID) | Identifiant de **16 caractères** `RR D AAA C SSS PPPPP` (région, département, arrondissement, commune, section, parcelle), plan de bornage signé |
| Burkina Faso | APFR (foncier rural) | Possession reconnue par les **voisins et autorités coutumières**, levé parcellaire, délivrée par le maire |

Systèmes de coordonnées : Togo `Lomé / UTM 31N` (EPSG:25231, Clarke 1880) ou
`WGS 84 / UTM 31N` (EPSG:32631) ; Bénin UTM 31N ; Côte d'Ivoire
`Abidjan 1987 / UTM 30N` (EPSG:2041) ou UTM 30N ; Sénégal UTM 28N ; Burkina,
Mali, Niger entre UTM 29N et 32N. Les datums anciens (Clarke 1880, Point 58,
Adindan) demandent une transformation vers WGS 84.

### État du module (vérifié dans le code)

- Entités : **parcelle** (section, numéro, contenance *en texte libre*,
  commune, propriétaire *en texte libre*, nature parmi 4), **limite**,
  **borne** (borne / repère / PI / station, précision), **servitude**.
- Commandes : nouvelle parcelle, rechercher, scinder (médiane verticale
  seulement), lotir (bandes égales verticales), fusionner deux parcelles,
  modifier un sommet par boîte de dialogue, plan cadastral PDF, profil.
- Contrôles : topologie, recouvrement, identification (motifs du gabarit),
  validité de la feuille. `SurveyToleranceValidator` écrit mais non branché.
- Livrables : PDF (cartouche, nomenclature, cotations automatiques, bornes
  numérotées, flèche nord, échelle), DXF cadastral, GeoJSON, GeoPackage (non
  conforme à la spécification), export CSV de coordonnées écrit mais **non
  enregistré** (et contour extérieur seulement), ArcGIS : bouchon.
- Gabarit : **Togo seulement** (motifs, échelles ; `survey_tolerance` non lu ;
  `plot_styles` et `text_styles` non consommés).
- **Aucun système de coordonnées**, aucune unité, aucun riverain, aucune pièce
  jointe, aucun historique.

*Depuis ce constat (2026-10-04)* : la parcelle se dessine par ses sommets
(K-01) ou se convertit d'une polyligne (K-02) ; la contenance est lue et
comparée à la surface calculée avec la tolérance `survey_tolerance` du gabarit
(K-09) ; les exports CSV des coordonnées (K-04, contour extérieur seulement)
et du calcul de surface (K-05, trous déduits) sont enregistrés ; un carnet de
levé s'importe en bornes (K-03).
Les cotes automatiques du plan sont de vrais objets cotation (A-01). Restent
vrais : Togo seul, aucun système de coordonnées, aucun riverain.

### Déjà livré (ne pas refaire)

- [x] Parcelle avec trous (persistés, surface nette, contrôles, DXF, GeoJSON)
- [x] Scinder, lotir, fusionner, modifier une limite — annulables et rétablissables
- [x] Recherche par section et numéro
- [x] Trois contrôles de topologie et d'identification, profil désigné par l'opérateur
- [x] Feuille PDF : cartouche déclaratif (ADR-017), nomenclature, cotations
      des côtés et des angles, bornes numérotées, flèche nord, échelle du profil
- [x] Export DXF cadastral (calques CADASTRE / COTATION / CARTOUCHE), GeoJSON
- [x] Calques du module créés à l'ouverture (`IStyleProvider`)
- [x] Ruban avec icônes, panneaux Parcelles / Édition / Livrables / Contrôle
- [x] Nouvelle parcelle par ses sommets, conversion de polylignes (K-01, K-02)
- [x] Contenance calculée contre déclarée (K-09)
- [x] Tableau des coordonnées des bornes et calcul de surface, PDF et CSV
      (K-04, K-05) ; import de levé en bornes (K-03)

### P0 — sans quoi un géomètre ne peut pas produire un dossier

- [x] **K-B01 « Nouvelle parcelle » crée un rectangle fixe de 10 × 5 m nommé
      « A 001 » à l'origine** — P0 · S · défaut. `makeCreateParcel` sans
      argument rend `ParcelEntity::createDefault()`. Remplacé par K-01 / K-02.
      **Fait (2026-10-04)** : sans contour, la commande ne crée plus rien.
- [x] **K-01 Saisir une parcelle par ses sommets** (clics, coordonnées
      tapées, accrochage aux bornes) — P0 · M · spec F1, `COV`, `STDM`. Demande
      une stratégie générique de l'hôte « saisir un polygone »
      (`WorkbenchParams`), sans nom de métier.
      **Fait (2026-10-04)** : stratégie générique `WorkbenchParams::PickPolygon`
      (`PLUGIN_API_VERSION` 14) — l'hôte fait dessiner un contour fermé avec les
      gestes de la polyligne (clics accrochés, `x,y`, `@dx,dy`, Entrée / C /
      clic droit pour fermer, Échap pour renoncer, côté de fermeture montré) et
      passe les sommets à la commande, sans savoir ce qu'ils délimitent.
      « Nouvelle parcelle » s'en sert ; section et numéro se saisissent ensuite
      dans le panneau Propriétés ; rétablir rend la parcelle sous son id.
      Le test de bout en bout a révélé un plantage à la fermeture de la
      fenêtre (historiques encore connectés pendant la destruction des
      dessins, mémoire libérée parcourue), corrigé à la fermeture de la
      fenêtre et d'un onglet. Tests : `viewport_tools_test`,
      `cadastre_parcel_commands_test`, `mainwindow_workbench_test` (vraie
      fenêtre, module chargé).
- [x] **K-02 Convertir des polylignes fermées sélectionnées en parcelles** —
      P0 · S · spec D3. Indispensable après un import DXF d'un plan existant.
      Possible dans le module seul (`SelectionIds`).
      **Fait (2026-10-04)** : bouton « Convertir en parcelles » (panneau
      Parcelles, icône dédiée) ; contour, trous, calque et couleur gardés,
      champs cadastraux prêts à saisir ; polylignes ouvertes et parcelles
      existantes laissées de côté ; annulable, ids stables au rétablissement
      (`cadastre_parcel_commands_test`).
- [x] **K-03 Import de levé** : fichier de points `matricule, X, Y, Z, code`
      (CSV / TXT) → bornes numérotées ; rapport des lignes rejetées — P0 · M ·
      `COV` · via `IFileImporter` du module (point d'extension prêt).
      **Fait (2026-10-04)** : `Importer → Points de levé (CSV, TXT)` — un
      point = une borne (matricule en référence, altitude et code terrain
      gardés ; BORNE / REP / PI / STATION donnent la nature) ; lecture
      tolérante (`;` `,` tabulation ou espaces, virgule ou point décimal,
      en-tête, commentaires, Z facultatif) ; lignes écartées rapportées avec
      leur numéro, sans bloquer les autres. Côté hôte, générique : un import
      de module est **annulable** d'un seul Ctrl+Z, et ses remarques
      s'affichent en barre d'état (`cadastre_survey_import_test`,
      `viewport_tools_test`). Couvre aussi D-03 (partie 1).
- [x] **K-04 Tableau des coordonnées des bornes** : n°, X, Y, gisement et
      distance vers la borne suivante — sur le plan PDF et en export CSV —
      P0 · M · exigé en Côte d'Ivoire, plan géoréférencé au Togo · ⚠️ l'export
      CSV existe mais n'est pas enregistré et ignore les trous.
      **Fait (2026-10-04)** : une ligne par côté de parcelle — borne, X, Y,
      borne suivante, gisement en **grades** (depuis le nord, sens horaire,
      convention topographique française ; autre unité avec U-01), distance —
      dans la colonne de droite du plan cadastral PDF et dans
      `Fichier → Exporter → Tableau des coordonnées (CSV)` (point-virgule,
      virgule décimale, UTF-8 avec BOM). Une seule numérotation « B1, B2… »
      pour le plan et le tableau (`cadastre_coordinates_test`). Les trous ne
      sont pas encore listés. L'ancien `CsvCoordinateExporter`, jamais
      enregistré, reste dans le code tant que son test (`CadastreIoTest`) n'est
      pas retiré avec l'accord du mainteneur.
- [x] **K-05 Tableau de calcul de surface** (méthode des coordonnées, détail
      par sommet) — P0 · S · exigé en Côte d'Ivoire · `polygonArea` existe.
      **Fait (2026-10-04)** : `Fichier → Exporter → Calcul de surface (CSV)` —
      par parcelle, une ligne par sommet (borne, X, Y, Y(i+1) − Y(i−1),
      produit), puis 2S, S, trous déduits et surface nette ; mêmes numéros de
      borne que le plan (`cadastre_surface_test`). La version PDF ira dans le
      dossier technique (K-12). **Sécurité** : les exports CSV neutralisent
      l'injection de formule (un texte commençant par = + - @ est préfixé
      d'une apostrophe), signalée par la revue de sécurité sur K-04.
- [ ] **K-06 Système de coordonnées du dossier** : code EPSG fixé par le pays
      choisi (voir « Pays du dossier » ci-dessous, K-45 à K-50), affiché au
      cartouche, écrit dans GeoJSON / GeoPackage / DXF — P0 · M · Togo (plan
      géoréférencé), Côte d'Ivoire (calculs de géoréférencement). Prérequis
      hôte : U-01 Unités.
- [ ] **K-07 Procès-verbal de bornage contradictoire** (PDF : date, géomètre
      et son agrément, propriétaire, riverains présents ou absents par limite,
      bornes posées / reconnues / manquantes, observations, emplacements de
      signature) — P0 · L · Togo (art. 230), Bénin, Côte d'Ivoire.
- [ ] **K-08 Riverains par limite** : parcelles voisines détectées par les
      limites communes, nom du riverain et sa présence au bornage — P0 · M ·
      prérequis de K-07 ; APFR (reconnaissance par les voisins).
- [x] **K-09 Contenance calculée contre déclarée** : surface calculée en m² et
      ha a ca, écart avec la contenance de l'acte, tolérance du profil — P0 · S
      · spec C4 · ⚠️ la contenance est une chaîne libre, `survey_tolerance`
      n'est pas lue.
      **Fait (2026-10-04)** : règle `cadastre.contenance` (panneau
      Vérifications) — surface nette (trous déduits) contre contenance de
      l'acte lue quelle que soit son écriture (« 1 250,50 m² », « 2,35 ha »,
      « 2 ha 3 a 50 ca »…) ; écart admis = périmètre (contour et trous) ×
      `survey_tolerance.default_m`, désormais lue dans le gabarit et suivie au
      changement de profil (annulable). **Hypothèse à faire valider par un
      géomètre de chaque pays.** `cadastre_contenance_test`. L'affichage en
      ha a ca au choix du profil reste K-17.
- [ ] **K-10 Gabarits pays** : Bénin, Côte d'Ivoire, Sénégal, Burkina Faso,
      Niger, Mali — identifiant, système de coordonnées, échelles, tolérances,
      calques, styles, cartouche, pièces du dossier — P0 · S par pays (données
      JSON) · `ROADMAP_MARKET.md` étape 3. Bénin et Côte d'Ivoire d'abord.
- [ ] **K-11 Identifiant de parcelle défini par le gabarit** : champs
      composés et motif par pays — Togo section / numéro, Sénégal NICAD
      (16 caractères structurés), Côte d'Ivoire îlot / lot / n° de
      lotissement, numéro de titre foncier — P0 · M · ⚠️ deux champs fixes
      (section, numéro) avec motif.

### Pays du dossier : configuration automatique et contour du territoire

Demande du mainteneur (2026-10-03) : l'opérateur **choisit son pays** et tout
se règle seul ; le **contour du pays** est affiché en fond et le dessin se fait
dedans ; **plusieurs pays** peuvent être choisis ensemble pour les travaux
transfrontaliers ou internationaux.

- [ ] **K-45 Choix du pays du dossier** (un ou plusieurs), à la création du
      dossier et modifiable ensuite — P0 · M. Le choix applique **sans autre
      saisie** : système de coordonnées (K-06), schéma d'identifiant (K-11),
      échelles et tolérances, cartouche et autorité (K-14), calques et styles,
      unités (U-01). Le pays devient le profil du dossier : il remplace la
      saisie libre « Profil du dossier... » par une liste. Annulable, rangé
      dans les attributs du dossier (`cadastre.dossier.pays`), relu à
      l'ouverture.
      Table de départ (à faire valider par un géomètre de chaque pays) :

      | Pays | Système proposé | Zones UTM couvertes |
      |---|---|---|
      | Togo | Lomé / UTM 31N (EPSG:25231) ou WGS 84 / UTM 31N (EPSG:32631) | 31N |
      | Bénin | WGS 84 / UTM 31N (EPSG:32631) | 31N |
      | Côte d'Ivoire | Abidjan 1987 / UTM 30N (EPSG:2041) ou WGS 84 / UTM 30N (EPSG:32630) | 29N, 30N |
      | Sénégal | WGS 84 / UTM 28N (EPSG:32628) | 28N |
      | Burkina Faso | WGS 84 / UTM 30N (EPSG:32630) | 30N, 31N |
      | Mali | WGS 84 / UTM 30N (EPSG:32630) | 29N, 30N, 31N |
      | Niger | WGS 84 / UTM 32N (EPSG:32632) | 31N, 32N |

- [ ] **K-46 Contour du pays en fond** : calque de référence verrouillé, non
      imprimable, non accrochable, créé à l'ouverture du dossier — P0 · M.
      Données **embarquées avec le module** (hors-ligne, ADR-016), un fichier
      GeoJSON par pays dans `share/bcad/plugins/cadastre/countries/`,
      simplifié pour rester léger. Source à licence libre : **Natural Earth**
      (domaine public) ou **geoBoundaries** (CC BY 4.0) — pas GADM, dont la
      licence interdit l'usage commercial. Les contours, en longitude /
      latitude, sont projetés dans le système du dossier (projection de
      Mercator transverse écrite dans le module, sans PROJ ; changement de
      datum par K-21).
- [ ] **K-47 Dessiner dans son pays** : un point posé hors du contour donne un
      **avertissement immédiat** (barre d'état, marqueur), et la règle
      « hors du territoire » du panneau Vérifications liste les parcelles et
      bornes concernées — P0 · S. **Pas de blocage dur** : eaux
      territoriales, frontières contestées ou mal tracées dans les données,
      erreurs de levé près d'une frontière doivent rester saisissables et
      signalées. Une zone tampon (ex. 50 m) évite les fausses alertes dues à
      la simplification du contour.
- [ ] **K-48 Plusieurs pays en même temps** (travaux transfrontaliers,
      corridors, projets régionaux) — P1 · L. Les contours choisis sont
      affichés ensemble ; **chaque parcelle est rattachée au pays qui la
      contient** et suit ses règles (identifiant, contrôles, cartouche de son
      dossier technique) ; une parcelle à cheval sur une frontière est
      signalée. Le dossier garde **un seul système de coordonnées** : zone
      UTM commune quand elle existe, sinon choix explicite de l'opérateur (les
      calculs de surface et de distance doivent rester dans un même repère).
- [ ] **K-49 Zone UTM automatique** quand le pays en couvre plusieurs (Burkina
      Faso, Mali, Niger, Côte d'Ivoire) : proposée d'après la position du
      premier point saisi ou importé, modifiable, figée ensuite avec
      avertissement si un point tombe loin de la zone — P1 · S.
- [ ] **K-50 Changer de pays ou de système en cours de dossier** : reprojeter
      tout le dessin, avec aperçu et une seule étape d'annulation — P2 · M.

Architecture : pays, contours, systèmes et règles sont des **données du
module** (gabarits JSON et fichiers de contours) ; l'hôte n'en connaît aucun.
Il ne lui faut que des prérequis génériques : unités du document (U-01), un
calque de référence verrouillé et non imprimable (L-03), et un moyen pour un
module de signaler un point « hors zone » pendant la saisie (avertissement
non bloquant, via la consigne ou la barre d'état).

### Découpage administratif, feuille remplie seule, paquets pays

Demande du mainteneur (2026-10-03) : afficher en fond les **régions,
départements / préfectures, communes, cantons, villages, quartiers**,
activables ; que la **feuille d'impression reprenne seule** ce qui est connu
de l'emplacement ; des **gabarits et cartouches par pays**.

Ce que disent les données (vérifié) : les jeux libres **geoBoundaries**
(CC BY 4.0, usage commercial permis avec attribution) et **OCHA COD-AB**
couvrent les niveaux 0 à 2 ou 3 selon les pays — Togo : régions,
préfectures ; Bénin : jusqu'au niveau 3 ; Sénégal : régions, départements,
arrondissements. **Sous la commune** (canton, chefferie, village, quartier),
les polygones libres sont rares : OpenStreetMap donne surtout des **points**
de villages et de quartiers, sous licence ODbL (attribution, et partage à
l'identique de la base dérivée). Il faut donc prévoir l'apport de données
locales.

- [ ] **K-51 Paquet pays** : un dossier par pays,
      `share/bcad/plugins/cadastre/countries/<code ISO>/`, qui réunit profil
      (système de coordonnées, identifiant, échelles, tolérances), cartouche,
      contour (K-46), découpage administratif (K-52), légende, codes terrain,
      symboles, liste des pièces du dossier — P0 · S. **Installable
      séparément** : un géomètre ne télécharge que son pays (poste modeste,
      connexion rare, ADR-016). C'est la forme concrète de K-10.
- [ ] **K-52 Couches administratives en fond, activables par niveau** :
      région, département / préfecture, commune, arrondissement, canton /
      chefferie, village, quartier — noms affichés, calques verrouillés, non
      imprimables par défaut, chacun allumé ou éteint — P1 · M. Le paquet pays
      dit quels niveaux existent et comment ils s'appellent localement
      (préfecture au Togo, département au Bénin et au Sénégal…).
- [ ] **K-53 Localisation automatique de la parcelle** : son emplacement dans
      le découpage remplit région, département, commune, canton, village,
      quartier — P1 · M. Valeurs **déduites et signalées comme telles**
      (« déduit du découpage, version du 2026-xx »), toujours modifiables :
      la saisie de l'opérateur l'emporte. Une parcelle à cheval sur deux
      communes est signalée, **jamais devinée** (même règle que l'unanimité
      des parcelles de l'ADR-017).
- [ ] **K-54 Feuille et cartouche remplis automatiquement** : localisation
      (K-53), système de coordonnées, échelle, date, numéro de dossier,
      surface calculée, référence de la parcelle, cabinet (K-55) — P1 · S.
      L'architecture le permet déjà : les champs du cartouche sont résolus
      par `resolveField` (attributs du dossier, puis parcelles unanimes) ;
      on ajoute une troisième source, le découpage administratif. Un champ
      vide reste vide et le diagnostic le dit : rien n'est inventé.
- [ ] **K-55 Profil du géomètre et du cabinet**, saisi une fois : nom, numéro
      d'agrément, ordre professionnel, adresse, téléphone, logo du cabinet,
      cachet et signature en image — repris dans tous les cartouches et PV —
      P1 · S.
- [ ] **K-56 Identifiant composé depuis le découpage** : le préfixe NICAD du
      Sénégal (région, département, arrondissement, commune) se remplit d'après
      la localisation, l'opérateur n'ajoute que section et parcelle — P1 · S
      (après K-11 et K-53).
- [ ] **K-57 Données de découpage versionnées et datées** : les découpages
      changent avec les réformes de décentralisation ; la version et sa date
      s'impriment avec la mention de source, et le paquet pays se met à jour
      par simple copie de fichier, hors-ligne — P1 · S.
- [ ] **K-58 Apport de découpages locaux** : quartiers, villages, lotissements
      approuvés fournis par la mairie (Shapefile, GeoJSON, DXF) ou dessinés
      par l'opérateur, quand les données libres s'arrêtent à la commune —
      P1 · M.
- [ ] **K-59 Mentions de licence imprimées** sur la feuille quand une couche
      de fond est utilisée (« © geoBoundaries, CC BY 4.0 », « © contributeurs
      OpenStreetMap, ODbL ») — P1 · S · obligation des licences.
- [ ] **K-60 Toponymie de fond** : routes, cours d'eau, lieux-dits, pour se
      repérer et pour le plan de situation (K-13) — P2 · M · OpenStreetMap
      (ODbL, livré comme fichier de données distinct, sous sa licence).

Cartouches par pays (K-14, K-51) — précautions :
- textes et mentions de l'autorité par défaut ; **pas d'armoiries ni de logo
  officiel** sans l'accord de l'administration concernée, l'opérateur reste
  libre d'ajouter le logo de son cabinet ;
- chaque cartouche pays est validé par un géomètre agréé du pays avant d'être
  livré (les champs exigés varient : numéro de réquisition, de titre foncier,
  NICAD, lot et îlot, autorité de visa).

### P1 — attendu d'un logiciel de cadastre sérieux

Dossier et livrables :
- [ ] **K-12 Dossier technique en un clic** : plan de bornage, plan de
      situation, tableau des coordonnées, tableau des surfaces, PV, rapport,
      en PDF multipage, A3 ou A4 selon le profil — P1 · M · Côte d'Ivoire.
- [ ] **K-13 Plan de situation** (encart à petite échelle), composé seul
      depuis les couches de fond : commune ou quartier (K-52), voies (K-60),
      parcelle en surbrillance — P1 · M.
- [ ] **K-14 Cartouche par pays** : autorité (OTR / DCCFE, ANDF, DGID, MCLU),
      mentions légales, numéro d'agrément du géomètre — P1 · S par pays
      (gabarit de cartouche, ADR-017).
- [ ] **K-15 Fiche d'implantation des bornes** (coordonnées, gisements et
      distances depuis des stations connues) — P1 · M.
- [ ] **K-16 Étiquettes de parcelle et symboles de bornes dans le canevas**,
      pas seulement sur la feuille imprimée — P1 · M · spec F3 / H3 / H4.
- [ ] **K-17 Contenance en unités agraires** (ha a ca) au choix du profil —
      P1 · S.

Levé et calculs topographiques :
- [ ] **K-18 Import de station totale** (Leica GSI, formats texte courants) et
      de GNSS (CSV) — P1 · M.
- [ ] **K-19 Codes terrain → dessin automatique** (borne, clôture, mur, bâti,
      piste, puits), bibliothèque de codes par gabarit pays — P1 · M · `COV`.
- [ ] **K-20 Calcul de cheminement** : fermeture planimétrique, tolérance,
      compensation ; points rayonnés depuis les stations — P1 · L. Pourrait
      former un module « topographie » distinct, réutilisable hors cadastre.
- [ ] **K-21 Transformation de coordonnées** local ↔ WGS 84 (Helmert à
      7 paramètres pour Lomé / Clarke 1880, Abidjan 1987, Point 58, Adindan),
      paramètres dans le gabarit, sans dépendance à PROJ — P1 · M.

Topologie et contrôles :
- [ ] **K-22 Limites partagées** : déplacer une borne déplace les deux
      parcelles qui la partagent ; accrochage aux bornes existantes — P1 · L ·
      spec F5.
- [ ] **K-23 Vides et micro-chevauchements** entre parcelles voisines, à la
      tolérance du profil — P1 · M.
- [ ] **K-24 Tolérance de levé branchée** (`SurveyToleranceValidator` +
      `survey_tolerance.default_m`) une fois K-09 et K-27 en place — P1 · S ·
      `TODO.md` §7.5.
- [ ] **K-25 Couche de référence** : importer le cadastre existant de
      l'administration (Shapefile, GeoJSON, DXF) en calque verrouillé et
      contrôler le recouvrement avec les titres déjà délivrés — P1 · M · la
      Côte d'Ivoire vérifie la disponibilité de la parcelle dans sa base.

Lotissement et morcellement :
- [ ] **K-26 Lotissement réel** : îlots, lots numérotés, voirie avec emprise,
      réserves administratives et espaces verts, découpe par façade,
      profondeur ou surface minimale, tableau des lots et bilan des surfaces
      (% voirie, % réserves) — P1 · L · Côte d'Ivoire (approbation du plan de
      lotissement), toute l'Afrique de l'Ouest urbaine · ⚠️ aujourd'hui N
      bandes égales verticales.
- [ ] **K-27 Morcellement** par une ligne dessinée quelconque, ou par une
      surface cible à détacher — P1 · M · ⚠️ coupe par la médiane verticale
      seulement.

Droits et personnes (ISO 19152 / LADM) :
- [ ] **K-28 Parties et droits** : personne physique, morale, famille,
      collectivité coutumière ; quotes-parts d'indivision ; nature du droit
      (titre foncier, ACD, certificat de propriété foncière, APFR, certificat
      foncier rural, possession coutumière, bail) — P1 · L · `LADM`, `STDM`,
      `SOLA`, spec E2 · ⚠️ un propriétaire en texte libre.
- [ ] **K-29 Pièces jointes** (photos des bornes, actes scannés, PV signé)
      liées à la parcelle et emportées dans le fichier — P1 · M · `STDM`.

Échanges :
- [ ] **K-30 Export Shapefile** — P1 · M · `STDM`, usage courant des
      administrations · ⚠️ `ArcGisAdapter` est un bouchon.
- [ ] **K-31 Export KML / KMZ** (contrôle sur Google Earth par le client et
      l'administration) — P1 · S.
- [ ] **K-32 GeoPackage conforme** (index spatial, WKB complet, système de
      coordonnées) ou renommé « export SQLite » — P1 · M · `TODO.md` §7.7.

### P2 — confort et usages ruraux

- [ ] **K-33 Recensement systématique « adapté à l'usage »** : saisie rapide
      sur image satellite (D-10) ou relevé GPS de poche, précision assumée par
      catégorie, **liste et plan d'affichage public** — P2 · L · `SOLA`
      Systematic Registration, `Open Tenure` ; APFR, certificat foncier rural.
- [ ] **K-34 Historique des mutations** (parcelle mère → filles, date, acte) —
      P2 · M · `LADM` (source).
- [ ] **K-35 Restrictions** : zones non aedificandi, emprises de voie, reculs
      — P2 · M · `LADM` · ⚠️ servitude simple seulement.
- [ ] **K-36 Bâti dans la parcelle** (emprise, nature « Bâtie » déduite) —
      P2 · M.
- [ ] **K-37 Couleur par nature d'occupation** — P2 · S · spec E3.
- [ ] **K-38 QR code et empreinte sur le plan livré** (vérification
      d'authenticité contre la double vente d'un même terrain) — P2 · M.
- [ ] **K-39 Collecte terrain sur mobile** (formulaires ODK / XLSForm, aller-
      retour) — P2 · L · `STDM` (GeoODK).
- [ ] **K-40 Carroyage et légende configurables par le gabarit** — P2 · M ·
      spec G3 (le carroyage national a été retiré de l'hôte : il revient comme
      donnée du profil).
- [ ] **K-41 Export image PNG haute résolution** — P2 · S · spec I4.
- [ ] **K-42 Libellés du module traduisibles** (pays anglophones : Ghana,
      Nigeria) — P2 · M.

### P3 — sur besoin

- [ ] **K-43 Export ArcGIS GDB** — P3 · L · après une décision sur GDAL.
- [ ] **K-44 Courbes de niveau et profil de la parcelle** (terrains ruraux en
      pente) — P3 · M · voir D-12.

### Ordre conseillé pour la partie 3

1. ~~**Saisir vraiment une parcelle** : K-B01, K-02 (conversion, S), puis K-01.~~
   **Fait** (2026-10-04).
2. ~~**Les deux tableaux du dossier** et la contenance : K-04, K-05, K-09 ; puis
   K-03 import de levé.~~ **Fait** (2026-10-04).
3. **Pays et géoréférencement** : U-01 (hôte), K-45 choix du pays, K-46
   contour en fond, K-47 dessin dans le pays, K-06, K-11, puis K-10 Bénin et
   Côte d'Ivoire ; K-48 plusieurs pays et K-49 zone UTM ensuite.
   Dans la foulée, **K-51 paquet pays** (forme des gabarits), puis K-55
   cabinet, K-52 / K-53 découpage et localisation, K-54 feuille remplie seule.
4. **Le bornage contradictoire** : K-08 riverains → K-07 PV → K-12 dossier
   technique.
5. Les P1 dans cet ordre : K-22 / K-23 topologie, K-27 morcellement, K-31 KML,
   K-30 Shapefile, K-26 lotissement, K-28 parties et droits.

Règle d'architecture : tout ce qui nomme un pays, un acte ou une procédure vit
dans le module ou dans ses gabarits JSON. L'hôte ne reçoit que des
prérequis génériques : unités (U-01), saisie d'un polygone (stratégie de
`WorkbenchParams`), image raster (D-10), export multipage.

## Partie 4 — Interface générale : fenêtre, ligne de commande, palettes, fichiers, impression

Constat du 2026-10-04, à la demande du mainteneur (« je veux tout, plus détaillés
ceux qui servent au dessin »). Ce qui n'avait été comparé dans aucune partie :
haut de la fenêtre, ruban contextuel, ligne de commande, canevas, onglets des
dessins, palettes, fichiers, impression, raccourcis, aide. Préfixe `G-`.
✏️ = sert directement au dessin (détaillé davantage). Libellés AutoCAD de la
version française, **de mémoire, à vérifier** sur AutoCAD 2025 FR avant de les
reprendre. État BCAD **vérifié dans le code** (fichier et fonction cités).

### 4.1 Ligne de commande ✏️

État BCAD : un seul champ (`commandLine_`, `QLineEdit`, `MainWindow::buildCommandLine`,
`MainWindow.cpp`) ; la consigne de l'outil est son texte d'invite
(`setPlaceholderText`) ; le texte tapé part à `Viewport::submitTypedPoint`
(coordonnées, valeurs, options). Pas d'historique : les messages passent 6 s
dans la barre d'état puis disparaissent. Pas de nom de commande tapé (C-01).

- [ ] **G-01 Saisie semi-automatique des commandes** ✏️ — P1 · M (après C-01) ·
      `ACAD` AutoComplete (`INPUTSEARCHOPTIONS`). Pendant la frappe, une **liste
      s'ouvre au-dessus de la ligne de commande** : commandes et alias qui
      commencent par (ou contiennent) le texte, avec leur icône ; ↑ / ↓ pour
      choisir, Entrée ou clic pour lancer. Options d'AutoCAD à reprendre :
      recherche au milieu du mot (« LIG » trouve « POLYLIGNE »), ordre par
      fréquence d'usage, délai d'apparition, **correction automatique** des
      fautes déjà rencontrées, **synonymes** (« CERCLE » ↔ « ROND »).
      *Où, comme AutoCAD* : la liste ancrée au-dessus de la ligne de
      commande ; réglages par **clic droit sur la ligne de commande** ›
      « Options de recherche d'entrée… ».
      *Où dans BCAD* : un `QCompleter` sur `commandLine_` ; ses noms viennent
      de la **même table que les menus** (`kTools`, `MainWindowTools.cpp`, plus
      les actions des modules déclarées par leurs workbenches), jamais d'une
      liste recopiée ; fréquence d'usage gardée en `QSettings`.
      Fini quand : taper « CER » propose Cercle, Entrée le lance ; un module
      chargé ajoute ses commandes à la liste sans toucher `src/app`.
- [ ] **G-02 Options cliquables dans la consigne** ✏️ — P1 · S · `ACAD`. Les
      options entre crochets s'affichent **en bleu et se cliquent** (ex.
      « Position de la ligne de cote ou [Texte/Angle/**H**orizontale/**V**erticale] »),
      la lettre à taper en capitale. Un clic = taper l'option.
      *Où, comme AutoCAD* : dans la ligne de commande, à gauche du curseur de
      saisie.
      *Où dans BCAD* : la consigne quitte l'invite du champ pour un libellé à
      gauche de `commandLine_` (`buildCommandLine`) ; chaque outil **déclare
      ses options sous forme de liste** (`ViewportPrompts.cpp`) — prérequis
      partagé avec S-08c (même liste au menu du clic droit). Texte toujours
      en texte brut (pas de HTML venu d'un fichier).
- [ ] **G-03 Historique de la ligne de commande** ✏️ — P1 · S · `ACAD`. Trois
      lignes d'historique au-dessus de la saisie : consignes validées, valeurs
      tapées, **résultats** (une mesure I-01 / I-02 doit rester lisible, pas
      disparaître en 6 s) ; **F2** ouvre l'historique complet dans une fenêtre
      agrandie, à copier ; **Ctrl+9** masque / affiche la ligne de commande.
      *Où, comme AutoCAD* : au-dessus de la ligne de commande (lignes semi-
      transparentes) ; F2 ouvre la fenêtre de texte.
      *Où dans BCAD* : un `QPlainTextEdit` en lecture seule au-dessus de
      `commandLine_` (`buildCommandLine`), alimenté par `promptChanged`,
      `onCommandLineSubmitted` et `statusMessage` (`MainWindow.cpp`) ; F2 et
      Ctrl+9 avec V-21.
- [ ] **G-04 Rappel des entrées précédentes** ✏️ — P1 · S · `ACAD`. ↑ / ↓ dans
      la ligne de commande parcourent les commandes et valeurs déjà tapées
      (dernière distance, dernier point `@10<45`…) ; les mêmes sous « Entrées
      récentes » du menu du clic droit (S-08a, S-08c).
      *Où dans BCAD* : `commandLine_` (`keyPressEvent` d'un petit
      `QLineEdit` dérivé) ; liste par session, 20 entrées.
- [ ] **G-05 Expressions dans les valeurs tapées** ✏️ — P2 · S · `ACAD` `'CAL`,
      calculatrice dans la saisie. Taper `12,5/3`, `2*7,25` ou `@10<45+90` au
      lieu d'un nombre : la valeur est calculée. Utile pour reporter une cote
      d'un croquis de terrain.
      *Où dans BCAD* : `typedNumber` (`ViewportTextTools.cpp`) et
      `parseCoordinateInput` (`CoordinateInput.cpp`) — un seul évaluateur
      d'expressions (+ − × ÷, parenthèses), partagé.
- [ ] **G-06 Ligne de commande flottante ou ancrée** — P3 · S · `ACAD`. La
      ligne se détache, se déplace sur le canevas, transparence réglable.

### 4.2 Ruban ✏️

État BCAD : `RibbonBar` (`RibbonBar.cpp`) — onglets fixes, panneaux
`addPanel(tab, titre, actions, nbGrands)`, outils à venir grisés ; un onglet par
module.

- [ ] **G-07 Onglets contextuels** ✏️ — P1 · M (après D-11, D-02) · `ACAD`.
      Un onglet **apparaît en couleur à la fin du ruban** quand on sélectionne
      un objet qui a des réglages propres, et disparaît à la désélection :
      **texte multiligne** → « Éditeur de texte » (style, hauteur, gras /
      italique, justification, paragraphe, insérer un symbole ° ± Ø, champ,
      rechercher / remplacer, fermer l'éditeur) ; **hachure** → « Éditeur de
      hachures » (motif, échelle, angle, origine, contour, associative) — le
      même onglet pendant la commande Hachures (« Création de hachures ») ;
      **bloc** → « Éditeur de blocs » ; **tableau** → « Cellule de tableau ».
      Un objet de module peut avoir le sien (parcelle → « Parcelle » : scinder,
      fusionner, modifier la limite, propriétés), déclaré par son workbench.
      *Où, comme AutoCAD* : dernier onglet du ruban, onglet coloré, actif à
      l'apparition.
      *Où dans BCAD* : `RibbonBar` gagne un onglet contextuel montré /
      masqué sur `selectionChanged` selon le `TypeId` de la sélection ; côté
      module, un champ « type d'objet » dans `IWorkbench` / `WorkbenchPanel`
      → changement d'ABI, **`PLUGIN_API_VERSION` à incrémenter** et à
      documenter (`PLUGIN_ARCHITECTURE.md` §14, `API_ABI_POLICY.md` §5.2).
- [ ] **G-08 Panneaux déroulants et épinglables** ✏️ — P1 · S · `ACAD`. Le titre
      d'un panneau porte une flèche ▼ qui **déroule les outils moins
      fréquents** (Dessin ▼ : ellipse, spline, hachure… ; Modification ▼ :
      étirer, décaler, réseau…), et une punaise pour le garder ouvert. Le
      panneau Modification de BCAD, devenu très large, se range ainsi.
      *Où, comme AutoCAD* : barre de titre en bas de chaque panneau.
      *Où dans BCAD* : `RibbonBar::addPanel` — un indice « au-delà, dans la
      partie déroulante » ; `MainWindowMenus.cpp` répartit les outils.
- [ ] **G-09 Infobulles enrichies des outils** ✏️ — P2 · S · `ACAD`. Au survol
      d'un bouton : nom, phrase d'explication, **nom de commande à taper**
      (et son alias, C-01), « F1 pour l'aide » ; au survol prolongé, une
      illustration du geste.
      *Où dans BCAD* : une description et un nom de commande par entrée de
      `kTools` (`MainWindowTools.cpp`) ; les outils à venir gardent leur
      infobulle « à venir ».
- [ ] **G-10 Réduire le ruban** — P2 · S · `ACAD`. Bouton à droite des onglets :
      ruban complet → titres de panneaux → onglets seuls ; double-clic sur un
      onglet bascule. Gagne de la place sur un petit écran (ADR-016).
      *Où dans BCAD* : `RibbonBar` (bouton à droite de la barre d'onglets).
- [ ] **G-11 Onglet Sortie** — P2 · S · `ACAD` onglet Sortie. Tracer (G-33),
      aperçu, exporter PDF / DXF, exports des modules (CSV, GeoJSON…), qui
      sont aujourd'hui dans le menu Fichier seulement.
      *Où dans BCAD* : `MainWindowMenus.cpp`, après Annoter ; mêmes `QAction`
      que le menu Fichier.
- [ ] **G-12 Onglet Vue** — P2 · S · `ACAD` onglet Vue. Panneaux Palettes
      (Propriétés, Calques, Vérifications, palettes d'outils G-16), Interface
      (barre d'état, ligne de commande, onglets des dessins, ruban réduit),
      Fenêtres (V-15). Remplace, si le mainteneur le veut, les bascules de
      panneaux du coin inférieur droit (écart voulu de V-00).
- [ ] **G-13 Onglets Paramétrique, Gérer, Express Tools** — P3 · L · `ACAD`.
      Contraintes géométriques et cotes pilotantes (Paramétrique),
      personnalisation et normes (Gérer), outils express. Hors besoin actuel
      d'un géomètre ; recensé pour mémoire.

### 4.3 Canevas ✏️

- [ ] **G-14 Infobulle au survol d'un objet** ✏️ — P2 · S · `ACAD`
      `ROLLOVERTIPS`. Au repos, le survol d'un objet affiche, près du curseur,
      son **type, calque, couleur, type de ligne** ; un objet de module montre
      ses propriétés clés (parcelle : section, numéro, contenance) — très
      utile pour lire un plan cadastral sans rien sélectionner. Va avec la
      surbrillance au survol (S-10).
      *Où, comme AutoCAD* : près du curseur ; contenu choisi par type d'objet
      (personnalisation de l'interface, « Info-bulles de survol ») ; activé
      dans Options › Sélection (aperçu de la sélection).
      *Où dans BCAD* : `Viewport::mouseMoveEvent` au repos (`ViewportInput.cpp`),
      `pickEntity` après un court délai ; libellés tirés du `PropertyMap`
      (les propriétés déclarées par les modules, sans nom de métier dans
      `src/app`).
- [ ] **G-15 Contrôles de la fenêtre** — P3 · S · `ACAD`. En **haut à gauche du
      canevas** : `[-]` (barre de navigation V-07, icône SCU V-13), `[Haut]`
      (vues), `[Filaire 2D]` (styles visuels). En 2D, seul `[-]` sert.
      *Où dans BCAD* : dessiné par `ViewportOverlay.cpp`, coin supérieur gauche.
- [ ] **G-16 Calculatrice rapide** ✏️ — P2 · M · `ACAD QUICKCALC` (Ctrl+8).
      Palette de calcul ; pendant une commande, le résultat se colle dans la
      saisie ; fonctions géométriques : distance entre deux points désignés,
      angle d'une ligne, milieu, intersection. Complète G-05.
      *Où dans BCAD* : dock à droite, onglet à côté de Propriétés.

### 4.4 Palettes

- [ ] **G-17 Palettes d'outils** ✏️ — P2 · M (après D-09, D-02) · `ACAD
      TOOLPALETTES` (Ctrl+3). Palette à onglets de **symboles, motifs de
      hachure et commandes préréglées**, glissés sur le dessin. Pour un
      géomètre : bornes, arbres, regards, poteaux, clôtures ; hachures bâti,
      eau, végétation, voirie — **fournis par le gabarit pays** du module.
      *Où, comme AutoCAD* : palette flottante, à droite par défaut.
      *Où dans BCAD* : dock à droite, en onglet avec Propriétés ; contenu
      lu des gabarits des modules (`resolveDataFile`, `IStyleProvider`).
- [ ] **G-18 DesignCenter** — P3 · M (après D-09) · `ACAD ADCENTER` (Ctrl+2).
      Parcourir un autre dessin et en glisser calques, blocs, styles dans le
      dessin courant.
- [ ] **G-19 Gestionnaire des propriétés des calques en palette** — P2 · M ·
      `ACAD LAYER`. Toutes les colonnes d'AutoCAD (État, Nom, Activé, Geler,
      Verrouiller, Tracer, Couleur, Type de ligne, Épaisseur, Transparence,
      Description), arbre des filtres à gauche. BCAD : panneau Calques
      (`LayerPanel.cpp`) avec une partie des colonnes ; colonnes manquantes
      suivies par L-03, L-11, L-13. Cette tâche ne porte que la mise en forme
      en palette.
- [ ] **G-20 Palettes Références externes et Jeu de feuilles** — P3 · L ·
      `ACAD XREF`, `SHEETSET`. Après D-10 (références) et V-22 (présentations).

### 4.5 Haut de la fenêtre

État BCAD : le menu de l'application est le menu Fichier
(`ribbon_->setApplicationMenu(fileMenu)`, `MainWindowMenus.cpp`) : Nouveau,
Ouvrir, Enregistrer, Enregistrer sous, Fermer, Importer / Exporter, Aperçu
avant impression, Quitter. Barre d'accès rapide (`quickAccessToolbar`) :
Nouveau, Ouvrir, Enregistrer, Enregistrer sous, Aperçu, Annuler, Rétablir,
Zoom sur tout. Titre : « <dessin> — BCAD » (`MainWindowDocument.cpp`).

- [ ] **G-21 Menu de l'application complet** — P1 · S · `ACAD`. Colonne de
      gauche : Nouveau, Ouvrir, Enregistrer, Enregistrer sous ▸ (formats),
      Importer, Exporter ▸, Imprimer ▸ (Tracer, Aperçu, Mise en page),
      **Utilitaires ▸** (Contrôler G-30, Récupérer G-30, Purger G-31,
      Propriétés du dessin G-32, Unités U-01), Fermer. Colonne de droite :
      **Documents récents** (triés par date, épinglables) et **Documents
      ouverts**. En haut, un champ **rechercher une commande** ; en bas,
      **Options** (S-15) et **Quitter**.
      *Où, comme AutoCAD* : gros bouton en haut à gauche de la fenêtre.
      *Où dans BCAD* : `fileMenu` (`MainWindowMenus.cpp`) ; documents récents
      en `QSettings` (10 derniers), partagés avec G-25.
- [ ] **G-22 Barre d'accès rapide complète** ✏️ — P2 · S · `ACAD`. Par défaut :
      Nouveau, Ouvrir, Enregistrer, Enregistrer sous, Tracer, **Annuler ▾ et
      Rétablir ▾** (la flèche liste les actions : on annule plusieurs étapes
      d'un clic), Espace de travail ; puis la **flèche de personnalisation**
      (cocher les boutons, afficher sous le ruban, afficher la barre de
      menus). Le Zoom sur tout de BCAD n'y est pas dans AutoCAD (il est dans
      la barre de navigation et au double-clic molette, V-06).
      *Où dans BCAD* : `quickAccessToolbar` (`MainWindowMenus.cpp`) ; la liste
      d'annulation est un `QUndoView` sur le `QUndoGroup` (`undoGroup_`) dans
      un menu.
- [ ] **G-23 Barre de titre** — P3 · S · `ACAD`. Nom du dessin actif, `*` s'il
      est modifié ; à droite, recherche dans l'aide (G-38).
      *Où dans BCAD* : `setWindowTitle` (`MainWindowDocument.cpp`) avec
      `setWindowModified`.

### 4.6 Onglets des dessins

État BCAD : un onglet par dessin, fermable, bouton « + »
(`documentTabs_`, `MainWindowSessions.cpp`), V-09.

- [ ] **G-24 Clic droit sur un onglet de dessin** — P2 · S · `ACAD`. Nouveau,
      Ouvrir, Enregistrer, **Enregistrer tout**, Fermer, **Fermer tous les
      autres**, **Copier le chemin complet**, **Ouvrir l'emplacement du
      fichier**.
      *Où dans BCAD* : menu contextuel de `documentTabs_`
      (`MainWindowSessions.cpp`).
- [ ] **G-25 Onglet « Début »** (page d'accueil) — P2 · M · `ACAD`. Premier
      onglet, à gauche des dessins : **Nouveau** (avec choix du gabarit,
      G-26), **Ouvrir**, **Fichiers récents** en vignettes ou en liste,
      Apprendre — chez BCAD des guides **locaux** (hors ligne, ADR-016), et
      pour un géomètre « Nouveau dossier » avec choix du pays (K-45).
      *Où dans BCAD* : premier onglet de `documentTabs_`, une page Qt à la
      place du canevas.
- [ ] **G-26 Aperçu miniature au survol d'un onglet** — P3 · M · `ACAD`. Vignette
      de l'espace objet (et des présentations, V-22).
      *Où dans BCAD* : image du canevas saisie en quittant l'onglet
      (`grabFramebuffer`), gardée dans `DocumentSession`.

### 4.7 Fichiers et sécurité du travail

État BCAD : **enregistrement automatique existant** — toutes les 2 min
(`autosaveTimer_`, `MainWindow.cpp`), seulement pour un dessin **déjà
enregistré** et modifié, dans `<fichier>.autosave` à côté
(`autosavePathFor`, `MainWindow.h`) ; à la réouverture de ce fichier, BCAD
propose la version automatique si elle est plus récente. Pas de copie de
sauvegarde, pas de gabarit d'hôte.

- [ ] **G-27 Gabarits de dessin** ✏️ — P1 · M · `ACAD QNEW`, fichiers `.dwt`.
      Un nouveau dessin part d'un **gabarit** : calques, styles de texte et
      de cote, unités, cartouche, présentations prêts — on dessine aussitôt
      sur les bons calques avec les bonnes tailles. « Nouveau » (Ctrl+N) prend
      le gabarit par défaut ; « Nouveau… » ouvre la boîte **Sélectionner un
      gabarit** (vignette, description).
      *Où, comme AutoCAD* : Ctrl+N / menu de l'application › Nouveau ; le
      gabarit par défaut dans Options › onglet **Fichiers** › Paramètres de
      gabarit › « Nom du fichier gabarit par défaut pour QNOUV ».
      *Où dans BCAD* : un gabarit est un `.bcad` ordinaire (pas de nouveau
      format), copié à la création (`onNew`, `MainWindowDocument.cpp`) ; les
      modules en fournissent par leurs données (`resolveDataFile`), d'où les
      gabarits pays du cadastre (K-10, K-51) ; le choix par défaut dans S-15.
      Fini quand : Nouveau sur le gabarit d'un pays ouvre un dessin avec ses
      calques, sa hauteur de cote et son cartouche.
- [ ] **G-28 Enregistrement automatique complété** — P1 · S · `ACAD SAVETIME`.
      Écarts : un **dessin jamais enregistré n'est pas protégé** (perdu à une
      coupure de courant) ; intervalle non réglable (AutoCAD : 10 min par
      défaut, case et minutes réglables) ; emplacement imposé à côté du
      fichier ; **pas de copie de sauvegarde** : « Enregistrer » écrase la
      version précédente (AutoCAD la garde en `.bak`, `ISAVEBAK`).
      *Où, comme AutoCAD* : Options › onglet **Ouvrir et enregistrer** ›
      « Enregistrement automatique », « Minutes entre les enregistrements » et
      « Créer une copie de sauvegarde à chaque enregistrement » ;
      dossier dans Options › **Fichiers** › « Emplacement du fichier
      d'enregistrement automatique ».
      *Où dans BCAD* : `onAutosaveTimeout` (`MainWindowDocument.cpp`) — les
      dessins sans nom écrits dans un dossier de récupération (G-29) ; la copie
      `<fichier>.bak` faite par `onSave` avant `io::Database::save` ; réglages
      dans S-15.
- [ ] **G-29 Récupération après plantage** — P1 · M · `ACAD`
      Gestionnaire de récupération de dessins. Au **démarrage** suivant un
      arrêt brutal, une palette liste les dessins récupérables (fichier,
      version automatique, copie de sauvegarde, date) ; on choisit lequel
      ouvrir. BCAD ne le propose qu'en rouvrant le même fichier, et rien pour
      un dessin sans nom.
      *Où dans BCAD* : au démarrage (`main.cpp` / constructeur de
      `MainWindow`), lecture du dossier de récupération ; palette à droite.
- [ ] **G-30 Contrôler et récupérer un dessin** — P2 · M · `ACAD AUDIT`,
      `RECOVER`. Contrôler : liste et corrige les erreurs (calque inexistant,
      identifiant en double, objet dégénéré). Récupérer : ouvre un fichier
      abîmé en gardant ce qui se lit. BCAD écarte déjà à la lecture les
      valeurs non finies (`finiteStod`, `NativeSerializers.cpp`) et
      `Database::load` rapporte ce qu'il n'a pas pu lire.
      *Où, comme AutoCAD* : menu de l'application › Utilitaires.
- [ ] **G-31 Purger** — P2 · S · `ACAD PURGE`. Calques (L-09), styles, blocs
      inutilisés, avec la liste avant suppression. Menu de l'application ›
      Utilitaires.
- [ ] **G-32 Propriétés du dessin** — P2 · S · `ACAD DWGPROPS`. Titre, sujet,
      auteur, mots-clés, commentaires, propriétés personnalisées — que le
      cartouche lit (ADR-017, `resolveField`) et les champs (D-21). BCAD
      enregistre déjà un `PropertyMap` de document (`Document::properties`,
      `Database.cpp`) : il manque la boîte. Menu de l'application ›
      Utilitaires.

### 4.8 Impression

État BCAD : « Aperçu avant impression » (Ctrl+P, `onPrintPreview`,
`MainWindowDocument.cpp`) imprime l'étendue du dessin, ajustée, sur A3 paysage,
par la même composition que le PDF ; le plan cadastral passe par le module.

- [ ] **G-33 Boîte Tracer comme AutoCAD** — P1 · M · `ACAD PLOT` (Ctrl+P). Mise
      en page (nommée, réutilisable), imprimante ou PDF, **format de papier**,
      **zone de tracé** (étendue, **fenêtre désignée à la souris**, limites,
      présentation), décalage (centrer), **échelle** (ajustée ou 1:200,
      1:500, 1:1000…), table de styles de tracé (G-34), orientation, aperçu.
      *Où dans BCAD* : remplace la boîte d'aperçu fixe de `onPrintPreview` ;
      réutilise `layout::PdfExportOptions` et `applyFittingScale` ; les
      échelles proposées viennent du gabarit (profil du module).
- [ ] **G-34 Styles de tracé** — P2 · M · `ACAD` tables `.ctb`. Monochrome
      (tout en noir), épaisseur par couleur ou par calque à l'impression.
      Lié à L-01 (épaisseurs).
- [ ] **G-35 Publier plusieurs feuilles** — P3 · M · `ACAD PUBLISH`. Après V-22.

### 4.9 Raccourcis clavier généraux ✏️

| Raccourci | AutoCAD | BCAD |
|---|---|---|
| Ctrl+N / O / S / Maj+S | nouveau, ouvrir, enregistrer, enregistrer sous | ✅ |
| Ctrl+P | tracer | ✅ aperçu (G-33) |
| Ctrl+Z / Ctrl+Y | annuler / rétablir | ✅ |
| Ctrl+A, Suppr | tout sélectionner, effacer | ✅ |
| Ctrl+Tab | dessin suivant | ✅ |
| Ctrl+F4 | fermer le dessin | ⚠️ BCAD ferme par **Ctrl+W** |
| Ctrl+W | cycle de sélection (SEL-06) | ⚠️ pris par « Fermer le dessin » |
| Ctrl+Q | quitter | ✅ |
| Ctrl+C / X / V | copier, couper, coller | ❌ E-01 |
| Ctrl+Maj+C / Ctrl+Maj+V | copier avec point de base, coller comme bloc | ❌ E-01 |
| Ctrl+1 / 2 / 3 / 8 / 9 / 0 | Propriétés, DesignCenter, palettes d'outils, calculatrice, ligne de commande, écran épuré | ❌ V-14, G-18, G-17, G-16, G-03, V-12 |
| Ctrl+L / G / F / B / U | ortho, grille, accrochage objet, accrochage grille, polaire | ❌ (F8, F7, F3, F9 existent) |

- [ ] **G-36 Raccourcis clavier généraux d'AutoCAD** ✏️ — P1 · S · `ACAD`. Tout
      le tableau ci-dessus, chacun à la livraison de sa tâche ; **lever le
      conflit Ctrl+W** : « Fermer le dessin » passe à **Ctrl+F4** (AutoCAD),
      Ctrl+W revient au cycle de sélection. Les doubles de touches de
      fonction (Ctrl+L, G, F, B, U) tout de suite : ce sont les mêmes
      `QAction` que F8, F7, F3, F9.
      *Où dans BCAD* : raccourcis des `QAction` (`MainWindowMenus.cpp`,
      `QKeySequence::Close` à remplacer) ; un test vérifie qu'aucun raccourci
      n'est attribué deux fois.
- [ ] **G-37 Personnaliser les raccourcis** — P3 · M · `ACAD CUI`. Page de la
      boîte Options (S-15) ; réglage par utilisateur.

### 4.10 Aide

- [ ] **G-38 Aide hors ligne et contextuelle (F1)** — P2 · M · `ACAD` F1. F1
      ouvre l'aide de la **commande en cours** (ou de l'outil survolé) ; aide
      **locale**, livrée avec BCAD (poste hors ligne, ADR-016), consultable
      par une recherche (G-23).
      *Où dans BCAD* : pages HTML des guides dans les ressources, affichées
      dans un `QTextBrowser` ; une page par entrée de `kTools`.

### 4.11 Réglages : Options, Paramètres de dessin, profils, styles

Dans AutoCAD, **les réglages ne sont jamais dans le ruban** : ils vivent dans la
boîte Options et dans des boîtes spécialisées, qu'on atteint aussi par le clic
droit sur un bouton de la barre d'état. BCAD n'a aujourd'hui aucune de ces
boîtes (seuls les états de calques et quelques choix passent par `QSettings`,
`LayerPanel.cpp`).

- [ ] **G-40 Boîte Options : les dix onglets d'AutoCAD rapportés à BCAD** —
      P2 · M (étend S-15) · `ACAD OPTIONS`. S-15 pose la boîte et ses quatre
      premiers onglets ; celle-ci la complète, onglet par onglet, dans
      l'ordre d'AutoCAD. En haut de la boîte, comme AutoCAD : profil courant
      (G-42) et dessin courant.

      | Onglet AutoCAD | Contenu AutoCAD | Repris dans BCAD |
      |---|---|---|
      | **Fichiers** | chemins de support, polices, **gabarit par défaut**, enregistrement automatique, palettes d'outils, impression, fichiers temporaires | gabarit par défaut (G-27), dossier d'enregistrement automatique et de récupération (G-28, G-29), dossiers de modules (`$BCAD_PLUGIN_PATH`) et de leurs données, en lecture seule |
      | **Affichage** | thème, info-bulles, **Couleurs…**, **Polices…**, finesse des arcs, performances, présentations, **taille du réticule** | thème sombre / clair (`theme/dark.qss`), info-bulles (G-09, G-14), couleurs du fond et du réticule (S-11), finesse des arcs (tolérance de tessellation), taille du réticule (S-11) |
      | **Ouvrir et enregistrer** | format par défaut, vignette, **enregistrement automatique**, **copie .bak**, contrôle d'intégrité, **nombre de fichiers récents** | enregistrement automatique et minutes, copie de sauvegarde (G-28), nombre de fichiers récents (G-21) |
      | **Tracer et publier** | traceur par défaut, décalage, tampon, **style de tracé par défaut** | format de papier et orientation par défaut, style de tracé par défaut (G-34) |
      | **Système** | performances graphiques, périphérique de pointage, boîtes masquées, sécurité | mode brouillon (V-11), réafficher les boîtes masquées, modules chargés (liste, en lecture seule) |
      | **Préférences utilisateur** | **double-clic**, **clic droit**, unités d'insertion, champs, **priorité des coordonnées tapées**, **cotes associatives**, **épaisseurs de ligne…**, **liste des échelles** | double-clic (S-16), clic droit (S-08e), priorité saisie clavier sur accrochage, épaisseurs (L-01), liste des échelles (A-13, profils du module) |
      | **Dessin** | **accrochage automatique** (marqueur, aimant, info-bulle, zone de visée, couleurs, tailles), repérage, saisie dynamique | S-12, S-13, S-14, S-03, S-06, S-07 |
      | **Modélisation 3D** | réticule 3D, ViewCube, navigation 3D | **hors périmètre** (BCAD est 2D, ADR-016) |
      | **Sélection** | **cible**, modes (Maj pour ajouter, fenêtrage implicite, **lasso**), **aperçu de la sélection**, **poignées** (taille, couleurs, limite), onglets contextuels | S-11 (cible), S-10 (modes, lasso, aperçu), M-02 (poignées), G-07 |
      | **Profils** | jeux de réglages nommés | G-42 |

      *Où dans BCAD* : `src/app/OptionsDialog.cpp` (S-15), une page par
      onglet ; chaque réglage lu et écrit en `QSettings`, appliqué par
      accesseurs (`Viewport`, `MainWindow`), jamais dans le document.
- [ ] **G-41 Paramètres de dessin** ✏️ — P1 · M · `ACAD DSETTINGS`. La boîte des
      **aides au dessin**, distincte d'Options, à six onglets : **Accrochage
      et grille** (pas X / Y, type de grille, grille adaptative — V-08),
      **Repérage polaire** (angle d'incrément, angles supplémentaires,
      repérage orthogonal ou polaire — S-03), **Accrochage aux objets** (une
      case par mode, Tout sélectionner / Tout effacer — S-02), **Saisie
      dynamique** (S-07), **Propriétés rapides** (P-10), **Cycle de
      sélection** (SEL-06). Toutes les aides se règlent au même endroit.
      *Où, comme AutoCAD* : **clic droit sur un bouton d'aide de la barre
      d'état** (accrochage, grille, polaire) › « Paramètres… », ou la flèche
      de ce bouton (V-19) › « Paramètres d'accrochage aux objets… » ; commande
      `DSETTINGS` (alias `DS`, `OS`) ; menu Outils › Paramètres de dessin.
      *Où dans BCAD* : `src/app/DrawingSettingsDialog.cpp` ; ouverte depuis
      `addStatusButton` (`MainWindowMenus.cpp`) et le menu Outils
      (`toolsMenu`) ; valeurs appliquées au `SnapEngine` (`SnapEngine.h`) et
      au `Viewport` ; gardées par utilisateur, sauf le pas de grille, qui est
      une propriété du dessin (comme dans AutoCAD).
- [ ] **G-42 Profils de réglages** — P3 · S · `ACAD` onglet Profils. Enregistrer,
      copier, renommer, exporter, importer un jeu complet de réglages (un par
      utilisateur d'un poste partagé, ou « Formation » / « Production »). Un
      profil exporté en fichier se recopie sur un autre poste hors ligne.
      *Où dans BCAD* : dernier onglet de la boîte Options ; un groupe
      `QSettings` par profil.
- [ ] **G-43 Accès aux styles comme AutoCAD** ✏️ — P2 · S (avec A-06, A-07) ·
      `ACAD`. Le **style courant** se choisit dans une **liste déroulante du
      ruban** : Accueil › Annotation ▼ (styles de texte, de cote, de repère,
      de tableau) et en tête des panneaux Annoter › Texte et Annoter ›
      Cotations ; le **gestionnaire** s'ouvre par la **petite flèche ↘ du titre
      du panneau** (Texte → Style de texte ; Cotations → Gestionnaire des
      styles de cote) ou par la commande (`STYLE`, `DIMSTYLE`).
      *Où dans BCAD* : `RibbonBar::addPanel` gagne la flèche de titre (un
      `QAction` par panneau) et une liste dans un panneau ; les styles livrés
      par les gabarits pays (`text_styles`, A-07) y apparaissent.
- [ ] **G-44 Espaces de travail** — P3 · S · `ACAD WORKSPACE` (roue dentée de la
      barre d'état). Un espace de travail = quels onglets, panneaux et
      palettes sont montrés. Dans BCAD : « Dessin 2D » (hôte seul) et un par
      module (« Cadastre » : onglet du module en premier, palettes du dossier
      ouvertes). Choix gardé par utilisateur.

Complément à **U-01 Unités du dessin** (partie 1) — la boîte d'AutoCAD à
reprendre : **Longueur** (type : décimal, ingénierie, architectural… ;
précision), **Angle** (type : degrés décimaux, degrés / minutes / secondes,
**grades**, radians, **unités topographiques** — gisement « N 45d E » ;
précision ; case **Sens horaire**), **Échelle d'insertion** (unité des blocs
et des dessins insérés : mètres…), **Exemple de sortie**, bouton
**Direction…** (angle 0 : Est, Nord, Ouest, Sud ou désigné). *Où, comme
AutoCAD* : menu de l'application › Utilitaires › Unités, commande `UNITS`
(alias `UN`), bouton Unités de la barre d'état. *Où dans BCAD* : réglage **du
dessin** (`Document::properties`, enregistré), lu partout où un nombre est
affiché : coordonnées (`coordLabel_`), cotes (`formatDimensionText`), mesures
(I-01, I-02), tableaux du cadastre (gisements en grades aujourd'hui fixés).

### 4.12 Organisation du ruban et regroupement des outils ✏️

Principes d'AutoCAD, que BCAD suit déjà en partie (V-00) :

1. **Par intention, du plus fréquent au moins fréquent** : l'onglet Début /
   Accueil réunit ce qui sert en continu (dessiner, modifier, annoter,
   organiser, mesurer) ; les autres onglets servent ponctuellement. ✅
2. **Gros boutons** pour les outils les plus utilisés, petits pour les autres. ✅
3. **Boutons à variantes** : un bouton, une flèche ▾, les variantes d'un même
   outil ; le bouton garde **la dernière variante utilisée**. ❌ G-39.
4. **Panneaux déroulants ▼** pour les outils rares. ❌ G-08.
5. **Onglets contextuels** à la sélection ou pendant une commande. ❌ G-07.
6. **Une seule action, plusieurs accès** : ruban, menu, clic droit, ligne de
   commande, raccourci. ✅ (mêmes `QAction`) — la ligne de commande attend C-01.
7. **Réglages hors du ruban** (Options, boîtes spécialisées). ✅ par absence ;
   G-40, G-41.

- [ ] **G-39 Boutons à variantes** ✏️ — P1 · M · `ACAD`. Un bouton partagé :
      clic sur la partie principale = la **dernière variante utilisée** ;
      clic sur ▾ = la liste des variantes, avec icône et nom. Groupes
      d'AutoCAD à reprendre, chacun à la livraison des variantes :

      | Bouton | Variantes | État BCAD |
      |---|---|---|
      | **Cercle ▾** | centre-rayon, centre-diamètre, 2 points, 3 points, tan-tan-rayon, tan-tan-tan | centre-rayon seul (D-05) |
      | **Arc ▾** | 3 points, départ-centre-fin, départ-centre-angle, départ-centre-longueur, départ-fin-angle, départ-fin-direction, départ-fin-rayon, centre-départ-fin…, continuer | centre-départ-fin seul (D-04) |
      | **Rectangle ▾** | rectangle, polygone | rectangle (D-06) |
      | **Hachures ▾** | hachures, dégradé, contour | ❌ (D-02) |
      | **Ellipse ▾** | centre, axe-fin, arc elliptique | ❌ |
      | **Ajuster ▾** | ajuster (rogner), prolonger | **les deux existent : à regrouper tout de suite** |
      | **Raccord ▾** | raccord, chanfrein, raccord de courbes | ❌ (M-04, M-07, M-18) |
      | **Réseau ▾** | rectangulaire, polaire, trajectoire | ❌ (M-06) |
      | **Cotation ▾** (Accueil › Annotation) | linéaire, alignée, angulaire, longueur d'arc, rayon, diamètre, ordonnée, raccourcie | **cinq existent : à regrouper tout de suite** |
      | **Texte ▾** | multiligne, une ligne | une ligne (D-11) |
      | **Mesurer ▾** (Utilitaires) | rapide, distance, rayon, angle, aire, volume | ❌ (I-01, I-02) |
      | **Booléens ▾** | union, soustraction, intersection (régions) | les quatre existent (dont la différence symétrique) : à regrouper |

      *Où dans BCAD* : `RibbonBar::makeButton` (`RibbonBar.cpp`) met
      aujourd'hui toute action à menu en `InstantPopup` ; un bouton partagé
      demande `MenuButtonPopup` et une action par défaut qui suit la
      dernière variante (`QToolButton::setDefaultAction` au déclenchement) ;
      les groupes se déclarent dans `MainWindowMenus.cpp` (table, pas de
      cas particuliers) ; la dernière variante gardée par utilisateur
      (`QSettings`). Les menus classiques (Dessin, Modifier, Cotation)
      gardent toutes les variantes à plat, comme AutoCAD.
      Fini quand : les trois regroupements possibles aujourd'hui (Ajuster,
      Cotation, Booléens) sont faits et testés dans
      `mainwindow_workbench_test` (le bouton suit la dernière variante).
- [ ] **G-45 Ruban rangé panneau par panneau comme AutoCAD** ✏️ — P1 · S ·
      `ACAD`. Comparaison du ruban actuel (`MainWindowMenus.cpp`) à celui
      d'AutoCAD, onglet Début / Accueil, et corrections :

      | Panneau | AutoCAD (gros boutons en gras) | BCAD aujourd'hui | À faire |
      |---|---|---|---|
      | Dessin | **Ligne, Polyligne, Cercle ▾, Arc ▾** ; Rectangle ▾, Hachures ▾, Ellipse ▾ ; ▼ point multiple, spline, droite, demi-droite, région, nuage de révision, diviser, mesurer | **Ligne, Polyligne, Cercle, Arc** ; Rectangle, Point | variantes (G-39) ; Point et Texte au bon endroit (Texte est dans Annotation, ✅) ; ▼ (G-08) |
      | Modification | **Déplacer, Rotation, Ajuster ▾, Effacer** ; Copier, Miroir, Raccord ▾, Réseau ▾ ; Étirer, Échelle, Décaler ; ▼ coupure, joindre, décomposer, allonger, éditer la polyligne, ordre de tracé | tout en petits boutons, booléens et outils à venir à plat | quatre gros boutons comme AutoCAD ; Ajuster ▾ et Booléens ▾ (G-39) ; ▼ pour Coupure, Joindre, Exploser, booléens |
      | Annotation | **Texte ▾**, **Cotation ▾** ; Repère multiple ▾, Tableau ; ▼ styles courants (G-43) | **Linéaire**, Texte, repère et tableau à venir | Cotation ▾ en gros bouton (G-39), Texte en gros bouton |
      | Calques | **Propriétés des calques** ; liste déroulante des calques ; Rendre courant, Isoler, Geler, Verrouiller, Faire correspondre… | bascule du panneau Calques ; à venir : Rendre courant, Isoler, Fusionner | liste déroulante (L-02) ; boutons d'état (L-05, L-07, L-11) |
      | Bloc | **Insérer ▾** ; Créer, Modifier, Définir les attributs | à venir (D-09) | — |
      | Propriétés | **Copier les propriétés** ; listes Couleur, Épaisseur, Type de ligne, Transparence ; Liste | bascule du panneau Propriétés, copier les propriétés à venir | listes (P-03) ; Copier les propriétés en gros bouton (M-10) |
      | Groupes | **Grouper** ; dégrouper, modifier | à venir (P-09) | — |
      | Utilitaires | **Mesurer ▾** ; sélection rapide, tout sélectionner, calculatrice, ID point | tout sélectionner, sélectionner le dernier (propre à BCAD), à venir | Mesurer ▾ en gros bouton (I-01, I-02) ; ID point (I-06) ; Calculatrice (G-16) |
      | Presse-papiers | **Coller ▾** ; couper, copier, copier avec point de base | à venir (E-01) | — |
      | Vue | SCU, ViewCube, barre de navigation, fenêtres, palettes | **absent** : zoom et panneaux en barre d'état (écart voulu de V-00) | à trancher avec le mainteneur (G-12) |

      Mêmes tableaux pour **Insertion** et **Annoter** une fois leurs outils
      livrés ; leurs panneaux suivent déjà AutoCAD (V-00), sauf Nuage de
      points et Contenu (Insertion), Annotation (échelles, A-13) et Marquage
      (Annoter), à ajouter à leur livraison.
      *Où dans BCAD* : `MainWindowMenus.cpp` (`ribbon_->addPanel`, nombre de
      gros boutons) ; `mainwindow_workbench_test` vérifie l'ordre des
      panneaux et les gros boutons.

### Ordre conseillé pour la partie 4

1. **Les plus utiles au dessin, sans prérequis** : G-36 raccourcis (et le
   conflit Ctrl+W), G-03 historique, G-04 rappel, G-02 options cliquables.
2. **Sécurité du travail** sur poste modeste : G-28 dessins sans nom
   protégés, G-29 récupération au démarrage, puis la copie de sauvegarde à
   chaque enregistrement (dans G-28).
3. **Gabarits** G-27 (avec K-10 / K-51 côté module), puis G-21 menu de
   l'application et documents récents.
4. **Organisation du ruban** : G-39 boutons à variantes (Ajuster, Cotation,
   Booléens tout de suite), G-45 ruban rangé comme AutoCAD, G-08 panneaux
   déroulants ; puis G-41 Paramètres de dessin avec S-02 et V-08.
5. Après C-01 : **G-01** saisie semi-automatique ; après D-11 / D-02 : **G-07**
   onglets contextuels ; S-15 puis G-40 boîte Options.
6. Le reste au fil des besoins.

## Limites de ce recensement

Fait en deux passes le 2026-10-03. Ce qui n'est **pas** garanti exhaustif :

- **AutoCAD** : recensé par les panneaux du ruban et les listes de commandes,
  pas page par page dans la référence complète (les pages d'aide officielles
  ne se chargent pas entièrement hors navigateur). Les options des commandes
  rares peuvent manquer.
- **Covadis** : seuls les points topographiques, le MNT et l'étiquetage
  gisement / distance sont repris ; le reste (projets routiers, cubatures,
  profils en long et en travers) n'est pas recensé ici.
- **Partie 2** (affichage, calques, propriétés) : AutoCAD recensé par les
  panneaux de l'onglet Accueil, le gestionnaire des propriétés des calques et
  la navigation ; les onglets Présentation / espace papier sont hors périmètre.
- **Partie 3** (cadastre Afrique) : procédures recensées pour le Togo, le
  Bénin, la Côte d'Ivoire, le Sénégal et le Burkina Faso, sur sources
  publiques et articles ; **Mali et Niger ne sont pas documentés** ici, ni les
  textes de loi eux-mêmes. À faire valider par un géomètre agréé de chaque
  pays avant d'écrire un gabarit.
- **Partie 4** (interface générale, 2026-10-04) : libellés AutoCAD de mémoire,
  à vérifier sur une installation ; impression traitée côté interface
  (boîte Tracer, styles de tracé), la composition des feuilles reste suivie
  dans `TODO.md`.
- **Hors périmètre de ce fichier** : composition des feuilles et des
  présentations — suivie dans `TODO.md`.
- **Première passe manquante, corrigée en seconde** : saisie des commandes au
  clavier, édition de texte, coordonnées d'un point, aides au dessin,
  sélection avancée, presse-papiers, unités, options détaillées des commandes,
  blocs et hachures détaillés.

## Sources

AutoCAD :
- Aide AutoCAD 2025 — https://help.autodesk.com/view/ACD/2025/ENU/?guid=GUID-45C1A271-9650-4927-858F-B3BDB19B3E6C
- Aide AutoCAD LT 2023 — https://help.autodesk.com/view/ACDLT/2023/ENU/?guid=GUID-1D87D5C3-21BC-499E-A560-79592348D47E
- Onglet Accueil (Home tab) — https://help.autodesk.com/cloudhelp/2016/ENU/AutoCAD-Architecture/files/GUID-B060BED0-B506-4148-9EAE-5CCE0CC2F633.htm
- Accrochages aux objets — https://help.autodesk.com/cloudhelp/ENU/AutoCAD-Web-Help/files/Drafting-and-Creating/AutoCAD_Web_Help_Drafting_and_Creating_Osnap_html.html
- Onglet Accrochage aux objets — https://help.autodesk.com/cloudhelp/2023/ENU/AutoCAD-Core/files/GUID-50383F73-4F23-4F70-B4FC-52D5748D80AF.htm
- Raccourcis clavier AutoCAD — https://www.autodesk.com/shortcuts/autocad
- DIMBREAK — https://help.autodesk.com/view/ACD/2023/ENU/?guid=GUID-926915C5-C398-46C6-A5D8-16F0D5791760
- TEXTALIGN — https://www.cadforum.cz/en/command.asp?cmd=TEXTALIGN
- Lignes d'axe et marques de centre — https://novedge.com/blogs/design-news/autocad-tip-autocad-centerlines-and-center-marks-best-practices
- Ligne de construction — https://www.autodesk.com/blogs/autocad/construction-line-in-autocad-tuesday-tips-with-frank/
- Ellipse et polygone — https://www.nobledesktop.com/learn/autocad/getting-started-with-the-ellipse-and-polygon-tools-in-autocad
- Panneau Modification — https://www.cadtraininginstitute.com/essential-modify-panel-commands-in-autocad/
- Outils de modification (TOI-Pedia) — http://wiki.bk.tudelft.nl/toi-pedia/AutoCAD_Modify_Tools
- Cotations (CADTutor) — https://www.cadtutor.net/tutorials/autocad/dimensioning.php
- Cotations continue et ligne de base — https://www.tpointtech.com/autocad-continue-and-baseline-dimension
- Liste des commandes (Tutorial45) — https://tutorial45.com/autocad-command-list/

AutoCAD — affichage, calques, propriétés :
- LAYISO — https://help.autodesk.com/view/ACD/2025/ENU/?guid=GUID-E24B9866-9538-43BF-A3DF-AA7E2341C624
- Commandes de calques complémentaires — https://www.g-wlearning.com/cad/7352/ch05/data/supmat01.pdf
- Gestionnaire des propriétés des calques — https://help.autodesk.com/view/ACD/2024/ENU/?guid=GUID-B297EBD9-D68C-47E1-87CE-1B3798496599
- États de calques — https://novedge.com/blogs/design-news/autocad-tip-layer-states-for-rapid-layer-visibility-plotting-and-viewport-overrides
- Propriétés des objets et calques — https://help.autodesk.com/cloudhelp/2026/ENU/AutoCAD-OnBoarding/files/ACD_FOUNDATIONS_MAIN8.html
- Palette Propriétés — https://help.autodesk.com/view/ACD/2025/ENU/?guid=GUID-0621F211-3587-4932-BB3D-4D92ECB58042
- Propriétés des objets (CADTutor) — https://www.cadtutor.net/tutorials/autocad/object-properties.php
- Zoom et panoramique — https://help.autodesk.com/view/ACD/2020/ENU/?contextId=HYT_Blog_August_2019_Zoom_Pan
- Navigation — https://www.autodesk.com/blogs/autocad/explore-your-navigational-options-tuesday-tips-with-frank/
- Zoom, panoramique, navigation (Noble Desktop) — https://blog.nobledesktop.com/learn/autocad/zooming-panning-navigation-i

Cadastre en Afrique :
- Togo, procédures du titre foncier (OTR) — https://www.gnadoemedia.com/wp-content/uploads/2023/09/Togo-OTR-Procedures-detablissement-et-de-mutations-du-Titre-Foncier.pdf
- Togo, bornage contradictoire — https://www.lqdd.org/bornage-contradictoire-litige-parcelle-togo/
- Togo, titre foncier : procédure et documents — https://www.lqdd.org/titre-foncier-au-togo-procedure-couts-documents/
- Bénin, confirmation des droits fonciers (ANDF) — https://andf.bj/confirmation-des-droits-fonciers/
- Bénin, pièces du titre foncier — https://skyforcebenin.com/pieces-a-fournir-pour-lobtention-du-titre-foncier-au-benin/
- Côte d'Ivoire, ACD (Service public) — https://servicepublic.gouv.ci/accueil/detaildemarcheparticulier/1/245/27
- Côte d'Ivoire, procédure ACD — https://www.capital-foncier.com/blog/procedure-acd-cote-divoire-etapes-documents
- Côte d'Ivoire, approbation d'un lotissement — https://www.capital-foncier.com/blog/procedure-lotissement-47-etapes-cote-divoire
- Sénégal, NICAD (DGID) — https://www.dgid.sn/wp-content/uploads/2022/03/plaquette_nicad_verso-3.pdf
- Sénégal, NICAD — https://keurcity.com/nicad-senegal/
- Burkina Faso, APFR — https://faolex.fao.org/docs/pdf/bkf95496.pdf
- Lomé / UTM zone 31N — https://epsg.io/25231
- WGS 84 / UTM zone 31N — https://epsg.io/32631
- Abidjan 1987 / UTM zone 30N — https://epsg.io/2041
- STDM (ONU-Habitat / GLTN) — https://github.com/gltn/stdm
- SOLA et Open Tenure (FAO) — https://www.fao.org/tenure/sola-suite/about/en/
- Natural Earth, contours de pays (domaine public) — https://www.naturalearthdata.com/
- geoBoundaries Togo (ADM0–2) — https://data.humdata.org/dataset/geoboundaries-admin-boundaries-for-togo
- geoBoundaries Bénin (ADM0–3) — https://data.humdata.org/dataset/geoboundaries-admin-boundaries-for-benin
- OCHA COD-AB Togo — https://data.humdata.org/dataset/cod-ab-tgo
- OCHA COD-AB Sénégal — https://data.humdata.org/dataset/cod-ab-sen
- OCHA COD-AB Côte d'Ivoire — https://data.humdata.org/dataset/cod-ab-civ
- Licence OpenStreetMap (ODbL), FAQ — https://osmfoundation.org/wiki/Licence/Licence_and_Legal_FAQ
- geoBoundaries, limites administratives (CC BY 4.0) — https://www.geoboundaries.org/
- LADM, ISO 19152 (FIG publication 84) — https://gdmc.nl/3dcadastres/Figpub84.pdf
- Cheminement planimétrique (AFT) — https://www.aftopo.org/categories-lexique/10-5-cheminement-planimetrique/

QCAD :
- Manuel de référence — https://qcad.org/doc/qcad/latest/reference/en/qcad_reference_manual_en.html
- Cotations — https://qcad.org/doc/qcad/2.2/reference/en/chapter24.html

LibreCAD :
- Outils de dessin — https://docs.librecad.org/en/latest/ref/tools.html
- Référence des commandes — https://edwinsaul.com/docs/LibreCAD/LibreCAD_commandFull.pdf

Covadis :
- Points topographiques (Ajenyur) — https://ajenyur.com/covadis-points-topographiques/
- Covadis 2D — https://pdfcoffee.com/covadis-2d-pdf-free.html
- Évolutions Covadis (Sogelink) — https://www.sogelink.com/wp-content/uploads/2024/10/Evolutions-Covadis-AutoPiste-Georail-SoConnect.pdf
