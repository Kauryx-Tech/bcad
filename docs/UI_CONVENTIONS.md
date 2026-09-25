# Conventions d'interface BCAD

## Positionnement

BCAD est une application de CAO 2D classique. L'interface doit être
immédiatement compréhensible par un utilisateur de LibreCAD, QCAD ou
FreeCAD en mode Drafting.

BCAD ne copie pas les icônes, logos, couleurs, textes ou mises en page
spécifiques d'un logiciel tiers. Il reprend uniquement les conventions
fonctionnelles communes aux logiciels de CAO libres et professionnels.

## Structure principale

La fenêtre principale suit cette organisation :

```text
┌ Menu : File Edit View Draw Modify Dimension Layer Tools Help ┐
├ Barres d'outils dockables / barre d'accès rapide             ┤
├ Docks gauche/haut optionnels                                 ┤
│                                                             │
│                       Canevas 2D                            │
│                                                             │
├ Ligne de commande / historique                               ┤
└ Coordonnées | calque | unités | grille | snaps | mode        ┘
```

- Le canevas reste la surface principale.
- `Layers` et `Properties` sont des panneaux dockables, tabulables et
  masquables.
- La ligne de commande reste disponible en permanence, mais ne doit pas
  empêcher l'utilisation par menus et barres d'outils.
- Le ruban actuel peut rester un accès rapide, mais il ne remplace pas les
  menus et barres d'outils classiques.

## Menus obligatoires

- `Fichier` : nouveau, ouvrir, enregistrer, importer, exporter, imprimer.
  Les exports GeoJSON et CSV des coordonnées sont disponibles directement
  depuis ce menu.
- `Édition` : annuler, rétablir, couper, copier, coller, supprimer, sélectionner.
- `Affichage` : zoom, affichage, grille, snaps, panneaux.
- `Dessin` : ligne, polyligne, cercle, arc, point.
- `Modifier` : déplacer, copier, tourner, miroir, rogner, prolonger, joindre,
  séparer, fusionner, modifier une limite.
- `Cotation` : dimensions et annotations.
- `Calque` : création, courant, visibilité, verrouillage et propriétés.
- `Outils` : préférences, validateurs, génération de plan, plugins.
- `Cadastre` (onglet déclaré par le module) : création, scission et fusion de
  parcelles, modification de limites, recherche par référence cadastrale et
  génération du plan cadastral.
- `Aide` : documentation et informations sur BCAD.

Un module ne glisse pas ses commandes dans les menus de l'hôte : il déclare un
workbench (`IWorkbench`) dont les panneaux deviennent un menu et un onglet de
ruban à son nom (`WORKBENCH.md`). L'utilisateur n'a donc pas à connaître le nom
interne d'une commande, mais il voit quel module la fournit.

La fenêtre principale implémente actuellement les menus classiques français
`Fichier`, `Édition`, `Affichage`, `Dessin`, `Modifier`, `Cotation`, `Calque`,
`Outils` et `Aide`, auxquels s'ajoute un menu par module chargé. Le menu `Cotation` contient une cotation linéaire interactive : deux clics
définissent les bornes et un troisième positionne la ligne de cote. Le cycle
aperçu → validation → undo est branché au canevas. Il propose aussi la
cotation alignée (deux clics, directement sur le segment), la cotation
angulaire (sommet puis deux rayons), la cotation de rayon et la cotation de
diamètre (centre puis point sur le cercle). Les géométries de cotation sont
créées sur le calque `Dimensions` et les distances nulles sont ignorées.

Le ruban contient les onglets `Accueil`, `Modifier`, `Affichage`, `Annoter`,
auxquels s'ajoute un onglet par module déclaré, avec des panneaux fonctionnels.
Les actions courantes sont aussi
disponibles dans une barre d'accès rapide. Chaque action reçoit explicitement
son icône : l'icône du thème Qt/Linux est utilisée quand elle existe, avec un
repli Qt standard pour garantir un affichage sous WSLg.

Le parcours dessin validé est : création des entités dans le canevas,
enregistrement/rechargement du fichier `.bcad`, puis aperçu et export
d'impression PDF. Les lignes ouvertes restent des lignes à l'impression ;
seules les géométries fermées sont imprimées comme polygones.

Les opérations cadastrales branchées au canevas réutilisent la sélection
courante : la scission attend une parcelle et utilise une ligne médiane
verticale, la fusion attend exactement deux parcelles, la modification de limite
permet de déplacer un sommet après saisie de ses coordonnées, et la création ne
nécessite pas de sélection. Une fois la référence saisie (`A 007`, `A-7`, `A7`),
la recherche remplace la sélection par les parcelles qui matchent.

## Ce qu'une action laisse dans l'historique

Une action de module n'entre dans le `QUndoStack` **que si elle change le
dessin** (`WorkbenchAction::modifiesDocument`). Déplacer la sélection, ou
produire un livrable extérieur comme le plan PDF, ne doit ni rendre le document
« modifié » — l'hôte proposerait de l'enregistrer pour rien — ni se trouver sous
`Ctrl+Z`, qui déferait le dernier tracé au lieu de la recherche. C'est le module
qui remplit le champ, l'hôte ne sachant pas ce que la commande touche.

## Cycle d'une commande

Toute commande de dessin ou de modification doit :

1. être disponible par menu, barre d'outils et nom de commande ;
2. afficher une invite claire dans la ligne de commande ;
3. accepter les coordonnées absolues et relatives quand cela a un sens ;
4. permettre `Esc` pour annuler l'étape courante ;
5. rester annulable/rétablissable par `Undo/Redo` ;
6. réutiliser la sélection et les accrochages standards.

## Icônes

Les boutons du ruban affichent des icônes standard Qt au-dessus de leur
libellé français. Elles sont volontairement neutres et réutilisent le thème
du système ; elles ne copient pas les ressources graphiques de LibreCAD ou
QCAD. Les menus restent utilisables au clavier même si un thème système ne
fournit pas un pictogramme particulier.

## Accrochage et état de dessin

Les modes de snap visibles sont : extrémité, milieu, centre, intersection,
proche, grille et orthogonal. La barre d'état indique le mode actif.

Les touches usuelles sont conservées :

| Action | Raccourci |
|---|---|
| Nouveau / ouvrir / enregistrer | `Ctrl+N` / `Ctrl+O` / `Ctrl+S` |
| Annuler / rétablir | `Ctrl+Z` / `Ctrl+Y` |
| Supprimer | `Delete` |
| Annuler la commande active | `Esc` |
| Zoom ajusté | `F` |
| Grille / grille magnétique | `F7` / `F9` |
| Accrochage objet | `F3` |
| Ortho | `F8` |

## Propriétés et calques

La sélection d'une entité met à jour le panneau `Properties`. Les propriétés
métier cadastrales sont fournies par le plugin ; le panneau du Core reste
générique.

Le panneau `Layers` doit afficher au minimum le calque courant, la visibilité,
le verrouillage et les attributs graphiques disponibles.

## Impression et plans cadastraux

L'aperçu (`Fichier → Aperçu avant impression`, `Ctrl+P`) compose la feuille avec
le même peintre que l'export : A3 paysage, zone à imprimer = étendue du document,
échelle standard automatique déduite de la place réellement laissée par le
cartouche, flèche Nord, barre d'échelle et cadre. Il n'expose aucun réglage :
format, orientation, échelle et centrage se déduisent du document, et le
cartouche reste vide côté hôte faute de métadonnées projet — c'est le module
métier qui le meuble.

`GeneratePlanSheetCommand` (plugin cadastre) passe par les mêmes
`PdfExportOptions` et `drawSheet`, et ajoute les labels, bornes et tableau des
parcelles. Exposer ces réglages dans l'aperçu, et une action « Exporter PDF »
dans l'hôte, reste à faire ; les deux doivent rester des commandes génériques
sans littéral métier.

## Règle d'architecture

Ces conventions décrivent l'UX, pas le Core métier. Le Core fournit les
mécanismes génériques ; les commandes et panneaux cadastraux sont ajoutés
par `bcad-cadastre-plugin`.
