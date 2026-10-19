# Design system BCAD

Référence de l'interface : à quoi elle ressemble, comment elle se comporte, et
comment y ajouter quelque chose sans réinventer ce qui existe. Le *où* (quel
outil dans quel onglet, quel menu) est dans [`UI_CONVENTIONS.md`](UI_CONVENTIONS.md) ;
les normes de code dans [`CONTRIBUTOR_GUIDE.md`](CONTRIBUTOR_GUIDE.md).

Modèle : **AutoCAD 2024 en français**, thème sombre. Quand une question n'est pas
tranchée ici, on fait comme AutoCAD.

## 1. Principe : une valeur, un endroit

Ce document décrit des **rôles** ; les **valeurs** sont dans le code, à un seul
endroit. Ne recopier une valeur ni ici, ni dans un autre fichier.

| Quoi | Seule source | Lu par |
|---|---|---|
| Couleurs, typographie, espacements, rayons, tailles d'icônes | `src/app/theme/Theme.h` (constantes) et `Theme.cpp` (couleurs) | C++ : `theme::color(theme::Token::…)`, `theme::k…` ; feuille de style : jetons `@nom@` |
| Mise en forme des widgets | `src/app/theme/dark.qss` — un gabarit qui ne contient que des jetons | `theme::styleSheet()` |
| Outils : libellé, icône | table `kTools` (`MainWindowTools.cpp`) | menus, ruban, barre d'état |
| Alias clavier | table `kToolAliases` (`ToolAliases.h`) | ligne de commande, canevas |
| Position dans le ruban | `MainWindowMenus.cpp` (déclaratif, par panneau) | `RibbonBar` |
| Icônes | `src/app/icons/*.svg` | `setActionIcon` |

`scripts/check_arch.sh` (règles 17 et 18) refuse : une couleur en dur dans
`dark.qss` ou dans `src/app`, une couleur d'icône hors palette, une taille
d'icône en nombre, un calcul recopié au lieu d'une aide commune.

## 2. Fondations

### Couleurs (jetons de `Theme.cpp`)

| Famille | Jetons | Usage |
|---|---|---|
| Surfaces (de la plus enfoncée à la plus haute) | `surface-sunken`, `field`, `canvas`, `window`, `scroll-track`, `toolbar`, `caption`, `panel`, `panel-alt`, `ribbon` | fonds ; `canvas` = espace objet ; `field` = champs de saisie |
| Contrôles, bordures | `control`, `control-hover`, `control-raised`, `control-raised-hover`, `border`, `border-hover`, `border-panel` | boutons, survols, cadres |
| Texte | `text-primary`, `text-secondary`, `text-caption`, `text-dock`, `text-disabled`, `text-strong` | du plus lisible au plus discret ; `text-strong` sur `accent` |
| Accent | `accent`, `accent-soft` | sélection, focus, état coché (fond `accent-soft`, bord `accent`) |
| Canevas | `canvas-text`, `preview`, `preview-text`, `snap`, `grid-dot`, `grid-axis`, `window-select`, `crossing-select` | aperçu d'outil, marqueur d'accrochage, grille, fenêtre (bleue) / capture (verte) de sélection |
| Calques | `layer-on`, `layer-off`, `lock-on`, `lock-off`, `bulb-base` | ampoule et cadenas de la liste des calques |
| Gravité | `severity-error`, `severity-warning`, `severity-info` | panneau Vérifications |
| Icônes | `icon-geometry`, `icon-action`, `icon-point`, `icon-danger`, `icon-success`, `icon-contrast-light`, `icon-contrast-dark` | seules couleurs permises dans les SVG |

Une couleur manque ? On ajoute un **jeton nommé par son rôle** (pas par sa
teinte : `preview`, pas `jaune`) dans l'enum `Token`, la table de `Theme.cpp` et
le tableau ci-dessus. Jamais de couleur locale « juste pour ce widget ».

### Typographie

Police d'interface d'AutoCAD (Segoe UI), puis la plus proche installée
(`kFontUi`). Deux tailles : **corps** (`kFontBodyPt` : menus, palettes, barre
d'état, ligne de commande) et **légende** (`kFontCaptionPt` : titres de panneau,
libellés du ruban). `kFontGlyphPt` pour un glyphe isolé, `kFontMono` pour la
ligne de commande. Pas d'autre taille : une hiérarchie se fait par la couleur
(`text-primary` / `text-secondary` / `text-caption`) ou le gras.

### Espacements, rayons

Espacements multiples de 2 px (`kSpaceXs` … `kSpaceL`). Rayons : `kRadius`
(boutons, champs, panneaux), `kRadiusSmall` (cases à cocher), `kRadiusRound`
(poignées de défilement).

### Tailles d'icônes

Ruban : grandes `kIconRibbonLarge`, petites `kIconRibbonSmall` ; barre d'état
`kIconStatusBar` ; accès rapide `kIconQuickAccess` ; bouton d'application
`kIconAppButton` ; chevron `kIconChevron` — les tailles d'AutoCAD à 100 %.

### Iconographie

- SVG propres à BCAD (aucune ressource d'AutoCAD, LibreCAD ou QCAD), embarqués
  dans `resources.qrc` : identiques hors ligne.
- Grille `viewBox="0 0 24 24"`, trait 1,6, extrémités et jonctions arrondies,
  `fill="none"` par défaut.
- Rôles de couleur : la **géométrie** existante en `icon-geometry`, ce que
  l'outil **crée ou change** en `icon-action`, les **points** désignés en
  `icon-point` ; `icon-danger` (effacer) et `icon-success` (vérifier) avec
  parcimonie. Pas de texte dans une icône.
- Nom : `verbe-objet` ou `famille-objet` en anglais, comme le thème freedesktop
  (`draw-polygon`, `modify-offset`, `measure-area`, `zoom-select`).
- Une icône par outil ; les variantes d'un même outil (Cercle 2 points, 3 points)
  partagent l'icône de l'outil.

## 3. Composants

| Composant | Construit par | Règles |
|---|---|---|
| Onglet, panneau du ruban | `RibbonBar::addPanel(onglet, titre, actions, nbGrands, débordement)` | ordre et noms d'AutoCAD ; un panneau sans outil réalisé n'existe pas |
| Grand bouton | `nbGrands` premières actions | outils majeurs du panneau (AutoCAD) |
| Petit bouton | actions suivantes, empilées par colonnes de 3 | l'ordre de la liste reproduit les rangées d'AutoCAD |
| Bouton icône seule | `RibbonBar::iconOnly(action)` | colonne d'icônes d'AutoCAD (Effacer, Décomposer, Décaler) ; infobulle obligatoire |
| Bouton à variantes ▾ | `RibbonBar::splitAction(parent, variantes)` | clic = variante affichée, ▾ = liste ; garde la dernière choisie ; libellé commun par `setIconText` (« Cercle ») |
| Panneau étendu ▼ | argument `débordement` de `addPanel` | outils secondaires du panneau, comme la flèche d'AutoCAD |
| Widget dans un panneau | `QWidgetAction` passé à `addPanel` | ex. liste des calques (`LayerCombo`) |
| Barre d'accès rapide | `MainWindow::installQuickAccess` | contenu d'AutoCAD (fichier, tracer, historique) ; ▾ : afficher la barre des menus, masquée par défaut comme AutoCAD |
| Bascule de barre d'état | action cochable dans `toggles[]` (`MainWindowMenus.cpp`) | icône seule, état lu dans le canevas |
| Palette | `QDockWidget` (Calques, Propriétés, Vérifications) | jamais modale ; ouverte aussi par Vue › Palettes |
| Ligne de commande | `CommandLine` | consigne en texte d'invite ; reçoit alias, noms complets AutoCAD, options, coordonnées ; ↑ ↓ rappel, F2 historique, suggestions pendant la frappe |
| Saisie dynamique (F12) | `Viewport::drawDynamicInput` | bulle près du curseur pendant une commande : étape courte, puis X , Y ou longueur < angle ; le texte tapé s'y affiche ; jetons `panel`, `field`, `accent` |
| Repérage polaire (F10) | `Viewport::drawTracking`, `detail::polarTrack` | rayon pointillé `snap` et bulle « Polaire : longueur < angle » ; après l'accrochage aux objets, exclusif d'Ortho |
| Repérage d'objet (F11) | `Viewport::drawTracking`, `detail::trackAlign` | croix `snap` sur les points acquis (7 au plus) et lignes d'alignement pointillées `snap` ; après l'accrochage aux objets, avant Ortho / polaire |
| Aperçu d'outil | `drawToolPreview` / `drawModifyPreview` | stylo `preview` en tirets ; fantôme par `Viewport::drawGhost` |
| Message | signal `Viewport::statusMessage` | barre d'état, jamais de boîte modale pour un refus |

## 4. Motifs d'interaction (outils)

Un outil se comporte comme la commande AutoCAD du même nom. En particulier :

- **Consigne** : `Outil — étape [options]` ; l'option se tape par sa lettre
  AutoCAD (`R` rayon, `C` copie, `2P`…). Le dernier résultat d'un outil de
  mesure précède la consigne.
- **Message** : `Outil : cause` (« Raccord : rayon 3 impossible entre ces objets »).
- **Entrée / Espace** terminent ; au repos, Entrée relance la dernière
  commande. **Clic droit** : Entrée pendant une commande, menu contextuel
  d'AutoCAD au repos (Répéter, Entrée récente, Annuler, Zoom… ou, avec une
  sélection, Effacer, Déplacer, Propriétés…). **Échap** annule l'étape puis la
  sélection.
- **Rester actif** : un outil qui s'enchaîne dans AutoCAD (Ligne, Décaler,
  Hachures, mesures, Panoramique) reste actif jusqu'à Entrée ; les autres
  reviennent au repos après leur opération.
- **Désigner un objet** : point brut (sans accrochage) ; **désigner un point** :
  point accroché.
- **Annulation** : une opération = un pas, libellé du nom de la commande
  (`EditBatch`).
- **Calques** : un objet dessiné va sur le calque courant ; une copie, un
  décalage, un raccord gardent le calque de l'objet d'origine (sauf option).
- **Sélection préalable** : facultative quand AutoCAD la rend facultative
  (Étirer, Raccord, Décaler désignent leurs objets eux-mêmes).

## 5. Contenu

- Français, vocabulaire d'AutoCAD 2024 en français : Ajuster (pas Rogner),
  Rotation, Miroir, Décomposer, Coupure, Effacer, Raccord, Chanfrein, Décaler,
  Étirer, Réseau, Aire, ID point, Propriétés des calques.
- Libellé de bouton court (un ou deux mots) ; détail dans l'infobulle (colonne
  `detail` de `kTools`).
- Pas de jargon interne (« ToolMode », « entité ») dans un texte visible.
- Toute chaîne visible passe par `tr()`.

## 6. Accessibilité et cible matérielle

- Poste de référence : portable 1366×768, pavé tactile (ADR-016). L'onglet
  Début doit tenir sans défilement à cette largeur ; toute fonction de
  navigation doit marcher sans bouton du milieu.
- Contraste : texte `text-primary` sur surfaces ; `text-disabled` réservé aux
  éléments réellement inactifs.
- Chaque bouton a une infobulle ; chaque outil a un alias clavier et une entrée
  de menu.
- **Échelle de l'interface** : celle de l'affichage, comme AutoCAD — sous WSL,
  l'échelle de Windows (125 %, 150 %…) ; réglable dans Affichage › Échelle de
  l'interface (`UiScale.cpp`). Toutes les tailles du design system sont en pixels
  *logiques* ; le canevas GL convertit en pixels physiques
  (`GlRenderer::render(camera, devicePixelRatio)`).

## 7. Recettes

**Ajouter un outil**
1. `ToolMode` (`Viewport.h`) ; une ligne dans `kTools` (libellé AutoCAD, détail,
   icône) ; l'alias AutoCAD dans `kToolAliases`.
2. Le gestionnaire dans le fichier `Viewport*Tools.cpp` de sa famille ; calcul
   pur dans un `*Detail.h` ; modifications par `EditBatch`.
3. Consigne (`ViewportPrompts.cpp`), aperçu, options tapées.
4. Bouton à sa place AutoCAD (`MainWindowMenus.cpp`) et entrée de menu.
5. Icône SVG si l'outil n'en a pas (§ Iconographie).
6. Test, `check_arch.sh`, état dans `TODO_OUTILS.md`.

**Ajouter une couleur, une taille** : un jeton (§ 2), jamais une valeur locale.

**Ajouter une icône** : SVG 24×24 aux couleurs `icon-*`, ajouté à
`resources.qrc` ; `check_arch.sh` vérifie la palette.
