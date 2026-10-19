Pour ne pas perdre les utilisateurs, ne cherche pas à copier AutoCAD pixel par pixel : reprends sa **logique de navigation**, puis crée un espace de travail spécialisé « Cadastre ». AutoCAD organise justement son interface autour de rubans, palettes, ligne de commande, barre d’état et espaces de travail orientés tâche ; ces espaces peuvent afficher ou masquer les éléments utiles au travail en cours. [help.autodesk](https://help.autodesk.com/view/ACD/2025/ENU/?guid=GUID-1D87D5C3-21BC-499E-A560-79592348D47E)

## Objectif d’interface

Ton interface doit permettre à un agent cadastral de toujours savoir :

1. **Où il est** : plan ouvert, commune, section, échelle, système de coordonnées.
2. **Ce qu’il fait** : outil actif — sélectionner, dessiner, découper, créer une parcelle.
3. **Sur quoi il travaille** : calque actif, parcelle sélectionnée, dossier associé.
4. **Quelle sera la sortie** : plan A3/A2/PDF/DXF/GeoJSON.
5. **Comment revenir en arrière** : Undo/Redo toujours visibles et accessibles.

La règle principale :

```text
Une action fréquente doit être accessible :
- par icône ;
- par raccourci clavier ;
- par ligne de commande ;
- et, si pertinent, par clic droit.
```

Exemple :

```text
Créer une polyligne
→ icône PLINE
→ raccourci PL
→ ligne de commande : PLINE
→ menu Dessin > Polyligne
```

## Organisation recommandée

### Barre supérieure

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ BCAD Cadastre — Plan_Lomé_AB_152.bcad *                                     │
├──────────────────────────────────────────────────────────────────────────────┤
│ [Nouveau] [Ouvrir] [Enregistrer] [PDF] [Undo] [Redo]     [Cadastre ▾] [⚙]  │
├──────────────────────────────────────────────────────────────────────────────┤
│ Accueil | Dessin | Modifier | Annotation | Calques | Cadastre | Layout | SIG│
├──────────────────────────────────────────────────────────────────────────────┤
│                         Ruban contextuel                                     │
└──────────────────────────────────────────────────────────────────────────────┘
```

La petite barre supérieure doit être la **Quick Access Toolbar** :

```text
Nouveau | Ouvrir | Enregistrer | Enregistrer sous | PDF | Undo | Redo
```

Ne surcharge pas cette zone avec 30 boutons. Elle doit contenir les actions utilisées toutes les heures.

### Ruban principal

Pour ton objectif cadastral, utilise ces onglets.

| Onglet | Contenu principal |
|---|---|
| Accueil | Sélection, outils rapides, calque actif, propriétés rapides |
| Dessin | Ligne, polyligne, cercle, arc, rectangle, point, borne |
| Modifier | Déplacer, copier, tourner, miroir, étirer, couper, prolonger, décaler, raccord, chanfrein |
| Annotation | Texte, numéro de parcelle, surface, cote, hachure, flèche nord, légende |
| Calques | Créer, modifier, verrouiller, geler, isoler, filtre de calques |
| Cadastre | Parcelle, limite, borne, division, fusion, validation, plan cadastral |
| Layout | Papier, cartouche, viewport, échelle, grille, aperçu, PDF |
| SIG | GeoJSON, GeoPackage, CSV, coordonnées, projection, ArcGIS |
| Affichage | Zoom, pan, grille, OSNAP, thèmes, panneaux |
| Aide | Guide, raccourcis, diagnostic, informations version |

Les logiciels AutoCAD récents permettent justement de masquer, afficher, déplacer ou réorganiser les onglets et panneaux du ruban selon l’espace de travail. [help.autodesk](https://help.autodesk.com/cloudhelp/2026/ENU/AutoCAD-LT-DidYouKnow/files/GUID-1D87D5C3-21BC-499E-A560-79592348D47E.htm)

## Espace de travail « BCAD Cadastre »

Au lancement, ne montre pas toute la puissance future de BCAD. Montre uniquement ce qui est utile à la production de plans cadastraux.

```text
┌───────────────┬────────────────────────────────────────────┬─────────────────┐
│ CALQUES       │                                            │ PROPRIÉTÉS      │
│               │                                            │                 │
│ ☑ Limites     │                                            │ Parcelle        │
│ ☑ Bornes      │              VIEWPORT                      │ Section : AB    │
│ ☑ Bâtiments   │                                            │ Numéro : 152   │
│ ☑ Voirie      │        dessin / sélection / édition        │ Commune : Lomé  │
│ ☑ Textes      │                                            │ Surface : ...   │
│               │                                            │ Statut : ...    │
│ [+] Filtrer   │                                            │                 │
├───────────────┴────────────────────────────────────────────┴─────────────────┤
│ Commande : _                                                             [ ] │
├──────────────────────────────────────────────────────────────────────────────┤
│ X: ...  Y: ... | CAD-LIMITE-PARCELLE | SNAP | ORTHO | 1:500 | UTM 31N       │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Panneau gauche : Calques

Toujours visible par défaut :

```text
- Visibilité
- Verrouillage
- Couleur
- Calque actif
- Recherche
- Filtre : "Cadastre", "Annotations", "Fond SIG"
```

### Panneau droit : Propriétés

Toujours visible, mais contextuel :

```text
Aucune sélection
→ aide courte : « Sélectionnez un objet pour afficher ses propriétés »

Ligne sélectionnée
→ calque, couleur, longueur, type de ligne

Parcelle sélectionnée
→ section, numéro, commune, contenance, surface géométrique,
  statut, référence dossier, propriétaire, nature
```

### Centre : Viewport

C’est la zone la plus importante. Elle doit avoir le maximum d’espace.

Évite :

- des panneaux flottants ouverts par défaut ;
- des boîtes de dialogue qui bloquent le travail ;
- des rubans trop hauts ;
- des icônes minuscules.

Les palettes AutoCAD peuvent être dockées, ancrées ou flottantes ; reprends ce principe, mais avec des réglages simples au départ. [help.autodesk](https://help.autodesk.com/view/ACD/2026/ENU/?caas=caas/documentation/ACDLT/2014/ENU/files/GUID-CF105B57-A6CB-48B2-B129-6E73C4B74D11-htm.html)

## Système d’icônes

Les icônes sont cruciales, mais elles ne doivent jamais être le seul moyen de comprendre une action.

### Règles visuelles

- Utiliser des icônes **vectorielles SVG**.
- Utiliser une grille de dessin identique pour toutes les icônes.
- Prévoir au moins :
  - 20 × 20 px dans les menus ;
  - 24 × 24 px dans les barres compactes ;
  - 32 × 32 px dans le ruban ;
  - 40 × 40 px pour les actions principales.
- Utiliser une seule épaisseur de trait graphique.
- Utiliser des formes simples à contraste élevé.
- Ne pas utiliser uniquement la couleur pour transmettre une information.
- Fournir un état :
  - normal ;
  - survol ;
  - actif ;
  - désactivé ;
  - erreur ;
  - validation.

### Convention de couleurs

Ne copie pas exactement les couleurs AutoCAD. Crée une convention BCAD stable.

| Couleur | Signification |
|---|---|
| Bleu BCAD | Action normale / outil actif |
| Vert | Valider, créer, exporter avec succès |
| Orange | Attention, modification non sauvegardée |
| Rouge | Supprimer, erreur, topologie invalide |
| Violet | Outils Cadastre |
| Turquoise | Outils SIG / géoréférencement |
| Gris | Action indisponible ou propriété en lecture seule |

Les couleurs doivent compléter l’icône, pas remplacer son sens.

Exemple mauvais :

```text
Un carré rouge sans texte
```

Exemple bon :

```text
[🗑] Supprimer
```

avec rouge uniquement comme signal complémentaire.

### Icônes indispensables

| Action | Icône recommandée | Libellé / tooltip |
|---|---|---|
| Nouveau | feuille avec `+` | Nouveau dessin |
| Ouvrir | dossier | Ouvrir un plan |
| Enregistrer | disquette moderne | Enregistrer |
| Export PDF | feuille `PDF` | Exporter le plan en PDF |
| Undo | flèche courbe gauche | Annuler : Déplacer une limite |
| Redo | flèche courbe droite | Rétablir : Déplacer une limite |
| Sélection | curseur flèche | Sélectionner |
| Ligne | segment avec deux points | Ligne (`L`) |
| Polyligne | segments reliés | Polyligne (`PL`) |
| Cercle | cercle avec centre | Cercle (`C`) |
| Déplacer | quatre flèches | Déplacer (`M`) |
| Copier | deux formes superposées | Copier (`CO`) |
| Décaler | deux lignes parallèles | Décaler (`O`) |
| Couper | ciseaux / ligne coupée | Ajuster (`TR`) |
| Prolonger | segment avec flèche | Prolonger (`EX`) |
| Hachure | zone rayée | Hachure (`H`) |
| Texte | `A` | Texte (`T`) |
| Cote | ligne avec flèches | Cote linéaire |
| Calque | piles de feuilles | Gestionnaire de calques |
| Parcelle | polygone avec numéro | Créer une parcelle |
| Borne | point repère | Ajouter une borne |
| Diviser | polygone séparé par trait | Diviser une parcelle |
| Fusionner | deux polygones reliés | Fusionner des parcelles |
| Contrôle | bouclier avec coche | Valider le plan |
| Erreur topo | triangle d’alerte sur polygone | Afficher les erreurs |
| SIG | carte avec repère | Export SIG |
| Nord | flèche nord | Insérer flèche nord |
| Échelle | règle graduée | Insérer barre d’échelle |
| Layout | feuille avec viewport | Mise en page |

### Tooltips obligatoires

Chaque icône doit afficher une info-bulle riche après un court survol :

```text
Polyligne
Raccourci : PL
Crée une suite de segments connectés.
Utilisez Entrée pour terminer.
```

Pour les outils cadastraux :

```text
Diviser une parcelle
Crée deux parcelles à partir d'une limite de division.
Le contrôle topologique est exécuté avant validation.
```

AutoCAD permet également de personnaliser les tooltips et macros de commandes dans ses éléments d’interface ; pour BCAD, garde cette idée mais propose d’abord une configuration simple en JSON ou dans les plugins. [help.autodesk](https://help.autodesk.com/view/ACD/2026/ENU/?caas=caas/documentation/ACDLT/2014/ENU/files/GUID-2285BF2B-C190-48F5-983A-A783140D813C-htm.html)

## Ne pas tout montrer

L’erreur serait de construire un ruban avec toutes les futures fonctions :

```text
BIM | 3D | CAM | FEM | Mesh | Robot | Rendering | Electrical | Mechanical
```

Cela perdrait immédiatement un agent cadastral.

Commence avec trois profils.

| Espace de travail | Utilisateur | Ce qui est affiché |
|---|---|---|
| BCAD Cadastre | Agent cadastral / dessinateur | Parcelles, bornes, annotations, calques, plans, PDF, SIG |
| BCAD Topographie | Géomètre | Points, coordonnées, levé, surfaces, contrôles, export SIG |
| BCAD Draft | Dessinateur général | Dessin 2D, modification, textes, cotes, calques, layouts |

Plus tard :

```text
BCAD Architecture
BCAD Mécanique
BCAD Réseaux
```

Les espaces de travail orientés tâche sont justement le bon modèle : l’utilisateur ne voit que les menus, rubans et palettes pertinents pour son activité. [help.autodesk](https://help.autodesk.com/view/ACD/2026/ENU/?caas=caas/documentation/ACDLT/2014/ENU/files/GUID-F1887CC8-7479-4770-A1F9-5573F9A25CFB-htm.html)

## Architecture à développer

Ne mets pas la définition visuelle du ruban en dur dans `MainWindow.cpp`.

Crée un système déclaratif.

```text
include/bcad/app/
├── RibbonBar.h
├── Workspace.h
├── WorkspaceManager.h
├── ActionRegistry.h
├── IconProvider.h
├── CommandLineController.h
├── StatusBarController.h
└── ToolController.h
```

Puis, dans les ressources ou plugins :

```text
resources/
├── icons/
│   ├── draw/
│   ├── modify/
│   ├── annotation/
│   ├── layers/
│   ├── cadastre/
│   ├── layout/
│   └── gis/
├── workspaces/
│   ├── cadastre.json
│   ├── topography.json
│   └── draft.json
└── themes/
    ├── dark.json
    └── light.json
```

Exemple de définition de ruban pour le plugin Cadastre :

```json
{
  "workspace": "BCAD Cadastre",
  "tabs": [
    {
      "id": "cadastre",
      "title": "Cadastre",
      "panels": [
        {
          "title": "Parcelles",
          "actions": [
            "cadastre.createParcel",
            "cadastre.splitParcel",
            "cadastre.mergeParcels",
            "cadastre.validateParcel"
          ]
        },
        {
          "title": "Repères",
          "actions": [
            "cadastre.createBoundary",
            "cadastre.addSurveyMark",
            "cadastre.addEasement"
          ]
        },
        {
          "title": "Plan",
          "actions": [
            "cadastre.generateLabel",
            "cadastre.generatePlan",
            "file.exportPdf"
          ]
        }
      ]
    }
  ]
}
```

Cela est meilleur que :

```cpp
if (cadastrePluginLoaded) {
    ribbon_->addButton(...);
    ribbon_->addButton(...);
    ribbon_->addButton(...);
}
```

Parce qu’un plugin peut alors enregistrer :

- ses commandes ;
- ses icônes ;
- ses panels ;
- ses raccourcis ;
- son espace de travail ;
- ses tooltips ;
- ses menus contextuels.

## Plan pratique

### Étape 1 — Interface de base

Développer :

- Barre d’accès rapide.
- Onglets du ruban.
- Panneaux dockables.
- Ligne de commande.
- Barre de statut.
- Menus contextuels.
- Thème sombre et clair.
- Tooltips riches.
- Icônes SVG cohérentes.

### Étape 2 — Workspace Cadastre

Développer :

- Onglet Cadastre.
- Calques cadastraux.
- Parcelle, borne, limite, servitude.
- Validation topologique.
- Informations de parcelle.
- Numérotation et surface.
- Export PDF, DXF, GeoJSON, CSV.

### Étape 3 — Production de plans

Développer :

- Layout A4/A3/A2/A1.
- Cartouche.
- Viewport à échelle.
- Flèche nord.
- Barre d’échelle.
- Grille de coordonnées.
- Légende.
- PDF vectoriel.
- Modèles de plans cadastraux.

### Étape 4 — Personnalisation

Développer :

- Sauvegarde de la position des panneaux.
- Workspace par utilisateur.
- Raccourcis personnalisables.
- Choix des onglets affichés.
- Thème clair/sombre.
- Icônes à haute densité.
- Langue française en priorité.
- Modèles administratifs propres au cadastre concerné.

## Recommandation finale

Pour que l’utilisateur ne soit jamais perdu, applique cette règle :

```text
Même action = même nom + même icône + même raccourci
partout dans BCAD.
```

Exemple :

```text
Déplacer
- Ruban : icône quatre flèches
- Menu : Modifier > Déplacer
- Ligne de commande : MOVE
- Raccourci : M
- Menu clic droit : Déplacer
- Tooltip : Déplacer (M)
```

Et pour le domaine Cadastre :

```text
Créer une parcelle
- Ruban Cadastre : polygone violet avec +
- Menu : Cadastre > Nouvelle parcelle
- Ligne de commande : PARCELLE
- Raccourci : PA
- Tooltip : Crée une parcelle cadastrale depuis un contour fermé
```

C’est cette cohérence, plus que le nombre de boutons, qui fera paraître BCAD professionnel et facile à apprendre.



Pour configurer les palettes ancrables dans BCAD Cadastre, utilise `QDockWidget` autour de chaque panneau métier, puis sauvegarde la disposition par espace de travail avec `QMainWindow::saveState()` et `restoreState()`. Qt prévoit exactement cela : un `QDockWidget` peut être ancré dans une `QMainWindow`, déplacé, fermé ou rendu flottant ; l’état complet des docks et toolbars peut être sauvegardé et restauré. [doc.qt](https://doc.qt.io/qt-6/qmainwindow.html)

## Palettes à prévoir

Pour l’espace de travail **BCAD Cadastre**, je recommande cette disposition par défaut :

```text
┌─────────────────────────────────────────────────────────────────────┐
│ Ribbon / menus                                                       │
├───────────────┬──────────────────────────────────────┬──────────────┤
│ CALQUES       │                                      │ PROPRIÉTÉS   │
│               │                                      │              │
│ Parcelles     │              VIEWPORT                │ Sélection    │
│ Bornes        │                                      │ Parcelle     │
│ Fonds SIG     │                                      │ Section      │
│               │                                      │ Numéro       │
│               │                                      │ Surface      │
├───────────────┤                                      ├──────────────┤
│ VALIDATION    │                                      │ OUTILS       │
│               │                                      │ CADASTRE     │
│ Erreurs topo  │                                      │              │
│ Avertissements│                                      │ Parcelle     │
│               │                                      │ Borne        │
├───────────────┴──────────────────────────────────────┴──────────────┤
│ Ligne de commande                                                     │
├─────────────────────────────────────────────────────────────────────┤
│ X / Y | Calque | Snap | Ortho | Échelle | SCR                        │
└─────────────────────────────────────────────────────────────────────┘
```

### Palettes recommandées

| Palette | Position par défaut | Peut flotter ? | Rôle |
|---|---|---:|---|
| Calques | Gauche | Oui | Visibilité, verrouillage, calque actif |
| Propriétés | Droite | Oui | Propriétés de la sélection |
| Outils Cadastre | Droite, sous Propriétés | Oui | Parcelle, borne, limite, division, fusion |
| Validation | Bas gauche | Oui | Chevauchements, contours ouverts, erreurs topo |
| Blocs / Symboles | Gauche ou flottante | Oui | Bornes, nord, cartouches, légendes |
| Layouts | Bas | Oui | Modèle, Layout A3, Layout A2, etc. |
| Historique / Dossier | Bas ou droite | Oui | Dossier foncier, versions, références |
| Console / ligne de commande | Bas | Non recommandé en flottant | Commandes et messages |

## Créer les palettes Qt

Dans `MainWindow.cpp`, ta méthode `buildDockWidgets()` peut ressembler à ceci :

```cpp
#include "bcad/app/LayerPanel.h"
#include "bcad/app/PropertiesPanel.h"

#include <QDockWidget>

void MainWindow::buildDockWidgets() {
    // -------------------------------------------------------------
    // Palette Calques : gauche
    // -------------------------------------------------------------
    auto* layerDock = new QDockWidget(tr("Calques"), this);
    layerDock->setObjectName("CadastreLayerDock");

    layerDock->setAllowedAreas(
        Qt::LeftDockWidgetArea |
        Qt::RightDockWidgetArea
    );

    layerDock->setFeatures(
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable
    );

    layerPanel_ = new LayerPanel(layerDock);
    layerDock->setWidget(layerPanel_);

    addDockWidget(Qt::LeftDockWidgetArea, layerDock);


    // -------------------------------------------------------------
    // Palette Propriétés : droite
    // -------------------------------------------------------------
    auto* propertiesDock = new QDockWidget(tr("Propriétés"), this);
    propertiesDock->setObjectName("CadastrePropertiesDock");

    propertiesDock->setAllowedAreas(
        Qt::LeftDockWidgetArea |
        Qt::RightDockWidgetArea
    );

    propertiesDock->setFeatures(
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable
    );

    propertiesPanel_ = new PropertiesPanel(propertiesDock);
    propertiesDock->setWidget(propertiesPanel_);

    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);


    // -------------------------------------------------------------
    // Palette Validation cadastrale : bas gauche
    // -------------------------------------------------------------
    auto* validationDock = new QDockWidget(
        tr("Validation cadastrale"),
        this
    );

    validationDock->setObjectName("CadastreValidationDock");

    validationDock->setAllowedAreas(
        Qt::LeftDockWidgetArea |
        Qt::RightDockWidgetArea |
        Qt::BottomDockWidgetArea
    );

    validationDock->setFeatures(
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable
    );

    auto* validationPanel = new CadastreValidationPanel(validationDock);
    validationDock->setWidget(validationPanel);

    addDockWidget(Qt::LeftDockWidgetArea, validationDock);


    // -------------------------------------------------------------
    // Palette Outils Cadastre : droite, sous Propriétés
    // -------------------------------------------------------------
    auto* cadastreToolsDock = new QDockWidget(
        tr("Outils Cadastre"),
        this
    );

    cadastreToolsDock->setObjectName("CadastreToolsDock");

    cadastreToolsDock->setAllowedAreas(
        Qt::LeftDockWidgetArea |
        Qt::RightDockWidgetArea
    );

    cadastreToolsDock->setFeatures(
        QDockWidget::DockWidgetClosable |
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable
    );

    auto* cadastreToolsPanel = new CadastreToolsPanel(cadastreToolsDock);
    cadastreToolsDock->setWidget(cadastreToolsPanel);

    addDockWidget(Qt::RightDockWidgetArea, cadastreToolsDock);


    // Les palettes Propriétés et Outils Cadastre occupent la même colonne.
    splitDockWidget(
        propertiesDock,
        cadastreToolsDock,
        Qt::Vertical
    );

    // Les palettes Calques et Validation occupent la même colonne.
    splitDockWidget(
        layerDock,
        validationDock,
        Qt::Vertical
    );

    // Taille initiale raisonnable.
    resizeDocks(
        {layerDock, propertiesDock},
        {280, 330},
        Qt::Horizontal
    );

    resizeDocks(
        {propertiesDock, cadastreToolsDock},
        {520, 300},
        Qt::Vertical
    );

    resizeDocks(
        {layerDock, validationDock},
        {520, 220},
        Qt::Vertical
    );
}
```

## Règle importante : `objectName`

Chaque `QDockWidget` doit avoir un nom stable et unique :

```cpp
layerDock->setObjectName("CadastreLayerDock");
propertiesDock->setObjectName("CadastrePropertiesDock");
validationDock->setObjectName("CadastreValidationDock");
```

Sans cela, Qt ne peut pas restaurer correctement la position des palettes.

Ne fais jamais ceci :

```cpp
layerDock->setObjectName(tr("Calques"));
```

Car la traduction pourrait changer selon la langue :

```text
Français : Calques
Anglais : Layers
```

Utilise un identifiant technique stable :

```cpp
"CadastreLayerDock"
```

## Sauvegarder la disposition utilisateur

Ajoute ces méthodes dans `MainWindow`.

### Dans `MainWindow.h`

```cpp
private:
    void restoreWorkspaceLayout(const QString& workspaceId);
    void saveWorkspaceLayout(const QString& workspaceId) const;
    void resetWorkspaceLayout(const QString& workspaceId);

    static constexpr int kWorkspaceLayoutVersion = 1;
```

### Dans `MainWindow.cpp`

```cpp
#include <QSettings>

void MainWindow::restoreWorkspaceLayout(const QString& workspaceId) {
    QSettings settings;

    settings.beginGroup("workspaces/" + workspaceId);

    const QByteArray geometry = settings.value("geometry").toByteArray();
    const QByteArray state = settings.value("state").toByteArray();

    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    if (!state.isEmpty()) {
        const bool restored = restoreState(
            state,
            kWorkspaceLayoutVersion
        );

        if (!restored) {
            // Le layout a changé ou la version n'est plus compatible.
            // On conserve alors la disposition par défaut créée par
            // buildDockWidgets().
        }
    }

    settings.endGroup();
}

void MainWindow::saveWorkspaceLayout(const QString& workspaceId) const {
    QSettings settings;

    settings.beginGroup("workspaces/" + workspaceId);

    settings.setValue("geometry", saveGeometry());
    settings.setValue(
        "state",
        saveState(kWorkspaceLayoutVersion)
    );

    settings.endGroup();
}

void MainWindow::resetWorkspaceLayout(const QString& workspaceId) {
    QSettings settings;

    settings.beginGroup("workspaces/" + workspaceId);
    settings.remove("");
    settings.endGroup();

    // Il faudra ensuite recréer ou réappliquer le layout par défaut.
    // La stratégie la plus simple est de demander un redémarrage,
    // ou d'appeler une méthode applyCadastreDefaultLayout().
}
```

Qt sauvegarde les tailles et positions relatives des docks et toolbars avec `saveState()`, puis les restaure avec `restoreState()`. Le numéro de version sert à invalider proprement un ancien layout lorsque l’application change sa structure. [doc.qt](https://doc.qt.io/qt-6/qmainwindow.html)

## Charger le workspace Cadastre

Dans le constructeur de `MainWindow` :

```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    buildMenusAndRibbon();
    buildDockWidgets();
    buildCommandLine();
    buildStatusBar();
    applyDarkTheme();

    restoreWorkspaceLayout("cadastre");
}
```

Puis dans `closeEvent()` :

```cpp
void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSaveBeforeDestructiveAction(tr("fermer BCAD"))) {
        event->ignore();
        return;
    }

    saveWorkspaceLayout("cadastre");

    event->accept();
}
```

## Prévoir plusieurs espaces de travail

Ne sauvegarde pas un seul layout général. Sauvegarde un layout par métier.

```text
workspaces/
├── cadastre
├── topography
├── draft
├── architecture
└── mechanical
```

Exemple :

```cpp
restoreWorkspaceLayout("cadastre");
restoreWorkspaceLayout("topography");
restoreWorkspaceLayout("draft");
```

Tu peux avoir plus tard :

```cpp
enum class WorkspaceId {
    Cadastre,
    Topography,
    Draft,
    Architecture,
    Mechanical
};
```

Mais pour commencer, une simple chaîne est suffisante :

```cpp
const QString workspaceId = "cadastre";
```

Les espaces de travail orientés tâche sont une très bonne approche : ils regroupent rubans, menus, toolbars et palettes en fonction du type de travail réalisé. [help.autodesk](https://help.autodesk.com/view/ACD/2026/ENU/?caas=caas/documentation/ACDLT/2014/ENU/files/GUID-F1887CC8-7479-4770-A1F9-5573F9A25CFB-htm.html)

## Palette obligatoire vs palette optionnelle

Toutes les palettes ne doivent pas avoir la même liberté.

### Palettes libres

L’utilisateur peut les déplacer, fermer ou rendre flottantes :

```cpp
QDockWidget::DockWidgetClosable |
QDockWidget::DockWidgetMovable |
QDockWidget::DockWidgetFloatable
```

Exemples :

```text
- Calques
- Propriétés
- Outils Cadastre
- Validation
- Blocs
- Layouts
```

### Palette semi-fixe

Pour la ligne de commande, il vaut mieux limiter les possibilités :

```cpp
commandDock->setAllowedAreas(Qt::BottomDockWidgetArea);

commandDock->setFeatures(
    QDockWidget::DockWidgetMovable
);
```

Ou même :

```cpp
commandDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
```

si tu veux qu’elle reste toujours en bas.

Pourquoi ? Parce que la ligne de commande est une référence visuelle essentielle pour les utilisateurs qui viennent d’AutoCAD. Si elle disparaît ou flotte au hasard, ils risquent d’être perdus.

### Viewport non ancrable

Le viewport doit rester central :

```cpp
setCentralWidget(viewport_);
```

Il ne doit pas être une palette.

## Ajouter un menu « Affichage > Palettes »

L’utilisateur doit pouvoir réafficher une palette qu’il a fermée.

```cpp
void MainWindow::buildViewMenu() {
    auto* viewMenu = menuBar()->addMenu(tr("&Affichage"));

    viewMenu->addAction(layerDock->toggleViewAction());
    viewMenu->addAction(propertiesDock->toggleViewAction());
    viewMenu->addAction(validationDock->toggleViewAction());
    viewMenu->addAction(cadastreToolsDock->toggleViewAction());

    viewMenu->addSeparator();

    auto* resetLayoutAction = viewMenu->addAction(
        tr("Réinitialiser l'espace de travail Cadastre")
    );

    connect(
        resetLayoutAction,
        &QAction::triggered,
        this,
        [this] {
            resetWorkspaceLayout("cadastre");
        }
    );
}
```

`toggleViewAction()` fournit automatiquement une action Qt liée à l’affichage de la palette :

```text
☑ Calques
☑ Propriétés
☐ Validation cadastrale
☑ Outils Cadastre
```

## Recommandation pour ton projet

Commence avec cette configuration exacte :

```text
Gauche :
- Calques
- Validation cadastrale

Droite :
- Propriétés
- Outils Cadastre

Centre :
- Viewport

Bas :
- Ligne de commande
- Barre de statut

Flottantes facultatives :
- Blocs et symboles
- Layouts
- Historique de dossier
- Bibliothèque de cartouches
```

Puis ajoute dans le menu :

```text
Affichage
├── Palettes
│   ├── Calques
│   ├── Propriétés
│   ├── Validation cadastrale
│   ├── Outils Cadastre
│   ├── Blocs et symboles
│   └── Layouts
└── Réinitialiser l’espace de travail Cadastre
```

Cela donne une expérience très proche de la logique AutoCAD — palettes ancrées, flottantes ou masquées, avec une disposition restaurée par workspace — sans rendre l’interface trop compliquée. AutoCAD applique également ce principe de palettes dockées, ancrées ou flottantes, contrôlées par espace de travail. [help.autodesk](https://help.autodesk.com/view/ACDLTM/2026/ENU/?caas=caas/documentation/ACD/2014/ENU/files/GUID-CF105B57-A6CB-48B2-B129-6E73C4B74D11-htm.html)
