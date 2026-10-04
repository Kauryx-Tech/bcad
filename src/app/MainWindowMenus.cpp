// Les menus de l'hote et les panneaux du ruban.
//
// Toute entree qui depend d'un metier est absente d'ici : le menu « Exporter »
// est dresse depuis le registre des exporteurs (voir MainWindowPlugins.cpp) et
// les menus des modules metiers sont inseres par buildPluginMenus(). L'ordre des
// menus suit UI_CONVENTIONS.md : « Aide » est cree en dernier et les menus
// declares par les modules sont inseres juste avant, pour ne pas dependre de
// l'ordre de chargement.

#include "MainWindow.h"

#include "ActionIcons.h"
#include "RibbonBar.h"
#include "bcad/geometry/BooleanOps.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabBar>
#include <QToolBar>
#include <QDockWidget>
#include <QFrame>
#include <QToolButton>

#include <initializer_list>
#include <utility>

namespace bcad::app {

void MainWindow::buildMenusAndRibbon() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&Fichier"));
    auto iconAction = [this](QAction* action, QStyle::StandardPixmap fallback,
                             const QString& themeName) {
        setActionIcon(this, action, fallback, themeName);
        return action;
    };
    QAction* newAction = fileMenu->addAction(tr("&Nouveau"), QKeySequence::New, this, &MainWindow::onNew);
    QAction* openAction = fileMenu->addAction(tr("&Ouvrir..."), QKeySequence::Open, this, &MainWindow::onOpen);
    QAction* saveAction = fileMenu->addAction(tr("&Enregistrer"), QKeySequence::Save, this, &MainWindow::onSave);
    QAction* saveAsAction = fileMenu->addAction(tr("Enregistrer &sous..."), QKeySequence::SaveAs, this, &MainWindow::onSaveAs);
    // Fermer ne concerne que le dessin de l'onglet actif (Ctrl+W / Ctrl+F4).
    fileMenu->addAction(tr("&Fermer le dessin"), QKeySequence::Close, this,
                        [this] { closeSession(activeSession_); });
    iconAction(newAction, QStyle::SP_FileIcon, "document-new");
    iconAction(openAction, QStyle::SP_DirOpenIcon, "document-open");
    iconAction(saveAction, QStyle::SP_DialogSaveButton, "document-save");
    iconAction(saveAsAction, QStyle::SP_DialogSaveButton, "document-save-as");
    fileMenu->addSeparator();
    QAction* importDxfAction = iconAction(
        fileMenu->addAction(tr("&Importer DXF..."), this, &MainWindow::onImportDxf),
        QStyle::SP_ArrowDown, "document-import");
    iconAction(fileMenu->addAction(tr("Exporter &DXF..."), this, &MainWindow::onExportDxf),
               QStyle::SP_ArrowUp, "document-export");
    // Menus dynamiques dressés depuis les registres une fois les plugins chargés.
    importMenu_ = fileMenu->addMenu(tr("&Importer"));
    exportMenu_ = fileMenu->addMenu(tr("&Exporter"));
    importMenu_->setIcon(QIcon(QStringLiteral(":/icons/document-import.svg")));
    exportMenu_->setIcon(QIcon(QStringLiteral(":/icons/document-export.svg")));
    fileMenu->addSeparator();
    QAction* printAction = fileMenu->addAction(tr("Aperçu avant &impression..."), Qt::CTRL | Qt::Key_P,
                                               this, &MainWindow::onPrintPreview);
    iconAction(printAction, QStyle::SP_FileDialogDetailedView, "document-print");
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Q&uitter"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Édition"));
    // Les actions viennent du groupe : elles suivent l'historique du dessin de
    // l'onglet actif, jamais celui d'un autre dessin.
    QAction* undoAction = undoGroup_.createUndoAction(this, tr("&Annuler"));
    undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAction);
    QAction* redoAction = undoGroup_.createRedoAction(this, tr("&Rétablir"));
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(redoAction);
    iconAction(undoAction, QStyle::SP_ArrowBack, "edit-undo");
    iconAction(redoAction, QStyle::SP_ArrowForward, "edit-redo");
    editMenu->addSeparator();
    QAction* deleteAction =
        editMenu->addAction(tr("&Supprimer"), QKeySequence::Delete, viewport_, &Viewport::deleteSelected);
    QAction* explodeAction = editMenu->addAction(tr("E&xploser"), viewport_, &Viewport::explodeSelected);
    QAction* joinAction = editMenu->addAction(tr("&Joindre"), viewport_, &Viewport::joinSelected);
    editMenu->addSeparator();
    QAction* selectAllAction = editMenu->addAction(tr("Tout &sélectionner"), QKeySequence::SelectAll, viewport_,
                                                    &Viewport::selectAll);
    QAction* selectLastAction = editMenu->addAction(tr("Sélectionner le &dernier"), viewport_, &Viewport::selectLast);
    // Modifier un texte (D-01b) : aussi par double-clic sur le texte.
    QAction* editTextAction = editMenu->addAction(tr("Modifier le &texte"), viewport_,
                                                  &Viewport::editSelectedText);
    iconAction(editTextAction, QStyle::SP_FileDialogDetailedView, "text-edit");
    iconAction(deleteAction, QStyle::SP_TrashIcon, "edit-delete");
    iconAction(explodeAction, QStyle::SP_FileDialogDetailedView, "object-ungroup");
    iconAction(joinAction, QStyle::SP_FileDialogListView, "object-group");
    iconAction(selectAllAction, QStyle::SP_DialogYesButton, "edit-select-all");
    iconAction(selectLastAction, QStyle::SP_ArrowUp, "go-last");

    viewMenu_ = menuBar()->addMenu(tr("&Affichage"));
    // Passer d'un dessin ouvert a l'autre au clavier (Ctrl+Tab, Ctrl+Maj+Tab).
    auto stepTab = [this](int step) {
        const int count = documentTabs_->count();
        if (count > 1) documentTabs_->setCurrentIndex((documentTabs_->currentIndex() + step + count) % count);
    };
    viewMenu_->addAction(tr("Dessin &suivant"), QKeySequence::NextChild, this, [stepTab] { stepTab(1); });
    viewMenu_->addAction(tr("Dessin &précédent"), QKeySequence::PreviousChild, this, [stepTab] { stepTab(-1); });
    viewMenu_->addSeparator();
    QAction* zoomFitAction = viewMenu_->addAction(tr("Zoomer sur &tout"), Qt::Key_F,
        viewport_, &Viewport::zoomToFit);
    QAction* snapAction = viewMenu_->addAction(tr("Activer l'&accrochage objet"), Qt::Key_F3,
        viewport_, &Viewport::toggleSnap);
    QAction* gridAction = viewMenu_->addAction(tr("Afficher la &grille"), Qt::Key_F7,
        viewport_, &Viewport::toggleGrid);
    QAction* gridSnapAction = viewMenu_->addAction(tr("Activer l'accrochage à la &grille"), Qt::Key_F9,
        viewport_, &Viewport::toggleGridSnap);
    QAction* orthoAction = viewMenu_->addAction(tr("Activer le mode &orthogonal"), Qt::Key_F8,
        viewport_, &Viewport::toggleOrtho);
    iconAction(zoomFitAction, QStyle::SP_FileDialogContentsView, "zoom-fit-best");
    iconAction(snapAction, QStyle::SP_DialogApplyButton, "snap-object");
    iconAction(gridAction, QStyle::SP_DialogHelpButton, "view-grid");
    iconAction(gridSnapAction, QStyle::SP_DialogApplyButton, "snap-grid");
    iconAction(orthoAction, QStyle::SP_ArrowRight, "orthogonal");
    // Bascules cochables : l'etat initial est lu dans le viewport, et chaque
    // bascule ne passe que par ces actions (raccourcis compris), donc la coche
    // du menu, du ruban et de la barre d'etat reste celle du viewport.
    // Coin inferieur droit de la barre d'etat : ce qui etait l'onglet
    // Affichage — cadrage, panneaux, puis aides au dessin — comme les
    // commandes d'affichage de la barre d'etat d'AutoCAD.
    auto addStatusButton = [this](QAction* action) {
        auto* button = new QToolButton(statusBar());
        button->setDefaultAction(action);
        button->setIconSize(QSize(18, 18));
        button->setAutoRaise(true);
        statusBar()->addPermanentWidget(button);
    };
    auto addStatusSeparator = [this] {
        auto* line = new QFrame(statusBar());
        line->setFrameShape(QFrame::VLine);
        line->setObjectName("statusSeparator");
        statusBar()->addPermanentWidget(line);
    };
    addStatusButton(zoomFitAction);
    addStatusSeparator();
    for (QDockWidget* dock : {layersDock_, propertiesDock_, validationDock_})
        addStatusButton(dock->toggleViewAction());
    addStatusSeparator();
    const std::pair<QAction*, bool> toggles[] = {
        {snapAction, viewport_->snapEnabled()}, {gridAction, viewport_->gridVisible()},
        {gridSnapAction, viewport_->gridSnapEnabled()}, {orthoAction, viewport_->orthoEnabled()}};
    for (const auto& [action, on] : toggles) {
        action->setCheckable(true);
        action->setChecked(on);
        addStatusButton(action);
    }
    // Les panneaux sont des vues : Proprietes et Verifications dans Affichage,
    // Calques dans le menu Calque.
    viewMenu_->addSeparator();
    viewMenu_->addAction(propertiesDock_->toggleViewAction());
    viewMenu_->addAction(validationDock_->toggleViewAction());

    QMenu* drawMenu = menuBar()->addMenu(tr("&Dessin"));
    addToolActions(drawMenu, {ToolMode::Select, ToolMode::Line, ToolMode::Polyline,
                              ToolMode::Circle, ToolMode::Arc, ToolMode::Rectangle,
                              ToolMode::Point, ToolMode::Text});

    // Libellés courts volontairement : le contexte du panneau/sous-menu
    // "Boolean" indique déjà de quoi il s'agit, et c'est le préfixe qui
    // faisait dépasser "Boolean Symmetric Difference" au-delà de ce qu'un
    // bouton du ruban peut afficher sans être tronqué en quelque chose
    // d'indiscernable de ses voisins.
    QMenu* modifyMenu = menuBar()->addMenu(tr("&Modifier"));
    QMenu* booleanMenu = modifyMenu->addMenu(tr("&Opérations booléennes"));
    QAction* unionAction = booleanMenu->addAction(tr("&Union"), this,
        [this] { viewport_->booleanOperation(geom::BooleanOp::Union); });
    QAction* intersectAction = booleanMenu->addAction(tr("&Intersection"), this,
        [this] { viewport_->booleanOperation(geom::BooleanOp::Intersection); });
    QAction* diffAction = booleanMenu->addAction(tr("&Différence"), this,
        [this] { viewport_->booleanOperation(geom::BooleanOp::Difference); });
    QAction* symDiffAction = booleanMenu->addAction(tr("Différence &symétrique"), this,
        [this] { viewport_->booleanOperation(geom::BooleanOp::SymmetricDifference); });
    iconAction(unionAction, QStyle::SP_DialogYesButton, "boolean-union");
    iconAction(intersectAction, QStyle::SP_DialogYesButton, "boolean-intersection");
    iconAction(diffAction, QStyle::SP_DialogNoButton, "boolean-difference");
    iconAction(symDiffAction, QStyle::SP_DialogNoButton, "boolean-symdiff");

    QMenu* dimensionMenu = menuBar()->addMenu(tr("&Cotation"));
    addToolActions(dimensionMenu, {ToolMode::DimensionLinear, ToolMode::DimensionAligned,
                                   ToolMode::DimensionAngular, ToolMode::DimensionRadius,
                                   ToolMode::DimensionDiameter});

    layerMenu_ = menuBar()->addMenu(tr("&Calque"));
    layerMenu_->addAction(layersDock_->toggleViewAction());

    auto* toolsMenu = menuBar()->addMenu(tr("&Outils"));
    toolsMenu->addAction(printAction);

    // « Aide » est cree ici, en dernier, et les menus des modules metiers sont
    // inseres juste avant (buildPluginMenus) : l'ordre annonce dans
    // UI_CONVENTIONS.md ne doit donc pas dependre de l'ordre de chargement des
    // modules. Sans cela, un module charge se retrouvait apres « Aide ».
    helpMenu_ = menuBar()->addMenu(tr("&Aide"));
    helpMenu_->addAction(tr("À propos de BCAD"), this, [this] {
        QMessageBox::about(this, tr("À propos de BCAD"),
                           tr("BCAD est une application de CAO 2D extensible."));
    });

    // --- Ruban : les memes objets QAction que les menus, regroupes en
    // onglets/panneaux comme dans AutoCAD — les outils majeurs en grands
    // boutons, les autres empiles par trois. Un bouton et l'element de menu
    // equivalent ne font qu'un, donc l'etat coche est partage.
    ribbon_->setApplicationMenu(fileMenu);
    // Outils de l'organisation d'AutoCAD pas encore realises : montres grises
    // avec la tache du plan, pour que la disposition soit familiere des
    // maintenant et que chaque outil s'allume a sa livraison, a sa place.
    auto upcoming = [this](const QString& text, const char* task) {
        auto* action = new QAction(text, this);
        action->setEnabled(false);
        action->setToolTip(tr("%1 — à venir (tâche %2 de TODO_OUTILS.md)")
                               .arg(text, QString::fromLatin1(task)));
        return action;
    };
    auto tools = [this](std::initializer_list<ToolMode> modes) { return toolActionsFor(modes); };

    // --- Accueil : blocs de l'onglet Debut d'AutoCAD, dans son ordre.
    ribbon_->addPanel(tr("Accueil"), tr("Dessin"),
                      tools({ToolMode::Line, ToolMode::Polyline, ToolMode::Circle, ToolMode::Arc,
                             ToolMode::Rectangle, ToolMode::Point}), 4);
    ribbon_->addPanel(tr("Accueil"), tr("Modification"),
                      tools({ToolMode::Move, ToolMode::Copy, ToolMode::Rotate, ToolMode::Mirror,
                             ToolMode::Scale, ToolMode::Trim, ToolMode::Extend, ToolMode::Break})
                          + QList<QAction*>{deleteAction, explodeAction, joinAction, unionAction,
                                            intersectAction, diffAction, symDiffAction,
                                            upcoming(tr("Décaler"), "M-01"),
                                            upcoming(tr("Raccord"), "M-04"),
                                            upcoming(tr("Étirer"), "M-03"),
                                            upcoming(tr("Réseau"), "M-06")}, 0);
    ribbon_->addPanel(tr("Accueil"), tr("Annotation"),
                      tools({ToolMode::DimensionLinear})
                          + tools({ToolMode::Text})
                          + QList<QAction*>{upcoming(tr("Ligne de repère"), "A-05"),
                                            upcoming(tr("Tableau"), "A-08")}, 1);
    ribbon_->addPanel(tr("Accueil"), tr("Calques"),
                      {layersDock_->toggleViewAction(), upcoming(tr("Rendre courant"), "L-05"),
                       upcoming(tr("Isoler"), "L-07"), upcoming(tr("Fusionner"), "L-08")}, 1);
    ribbon_->addPanel(tr("Accueil"), tr("Bloc"),
                      {upcoming(tr("Insérer"), "D-09"), upcoming(tr("Créer"), "D-09")}, 0);
    ribbon_->addPanel(tr("Accueil"), tr("Propriétés"),
                      {propertiesDock_->toggleViewAction(),
                       upcoming(tr("Copier les propriétés"), "M-10")}, 1);
    ribbon_->addPanel(tr("Accueil"), tr("Groupes"),
                      {upcoming(tr("Grouper"), "P-09"), upcoming(tr("Dégrouper"), "P-09")}, 0);
    ribbon_->addPanel(tr("Accueil"), tr("Utilitaires"),
                      {selectAllAction, selectLastAction, upcoming(tr("Sélection rapide"), "M-16"),
                       upcoming(tr("Mesurer"), "I-01"), upcoming(tr("Surface"), "I-02"),
                       upcoming(tr("Coordonnées"), "I-06")}, 0);
    ribbon_->addPanel(tr("Accueil"), tr("Presse-papiers"),
                      {upcoming(tr("Coller"), "E-01"), upcoming(tr("Copier"), "E-01"),
                       upcoming(tr("Couper"), "E-01")}, 0);

    // --- Insertion : ce qui fait entrer des donnees dans le dessin.
    ribbon_->addPanel(tr("Insertion"), tr("Bloc"),
                      {upcoming(tr("Insérer"), "D-09"), upcoming(tr("Modifier"), "D-09")}, 0);
    ribbon_->addPanel(tr("Insertion"), tr("Définition de bloc"),
                      {upcoming(tr("Créer un bloc"), "D-09"), upcoming(tr("Définir les attributs"), "D-09"),
                       upcoming(tr("Gérer les attributs"), "D-09"), upcoming(tr("Éditeur de blocs"), "D-09")}, 0);
    ribbon_->addPanel(tr("Insertion"), tr("Référence"),
                      {upcoming(tr("Attacher"), "D-10"), upcoming(tr("Découper"), "D-10"),
                       upcoming(tr("Ajuster"), "D-10"), upcoming(tr("Caler sur des points"), "Q-15"),
                       upcoming(tr("Cadres"), "D-10")}, 0);
    ribbon_->addPanel(tr("Insertion"), tr("Importer"),
                      {importDxfAction, importMenu_->menuAction()}, 2);
    ribbon_->addPanel(tr("Insertion"), tr("Données"),
                      {upcoming(tr("Champ"), "D-21"), upcoming(tr("Lien de données"), "D-21")}, 0);
    ribbon_->addPanel(tr("Insertion"), tr("Liaison et extraction"),
                      {upcoming(tr("Extraire des données"), "A-08"),
                       upcoming(tr("Mettre à jour les champs"), "D-21")}, 0);
    ribbon_->addPanel(tr("Insertion"), tr("Localisation"),
                      {upcoming(tr("Définir l'emplacement"), "K-45"),
                       upcoming(tr("Système de coordonnées"), "K-06")}, 0);

    // --- Annoter : texte, cotations, lignes d'axe, lignes de repere, tableaux.
    ribbon_->addPanel(tr("Annoter"), tr("Texte"),
                      tools({ToolMode::Text})
                          + QList<QAction*>{editTextAction, upcoming(tr("Texte multiligne"), "D-11"),
                                            upcoming(tr("Style de texte"), "A-07"),
                                            upcoming(tr("Rechercher et remplacer"), "E-03"),
                                            upcoming(tr("Aligner les textes"), "M-19")}, 1);
    ribbon_->addPanel(tr("Annoter"), tr("Cotation"),
                      tools({ToolMode::DimensionLinear, ToolMode::DimensionAligned,
                             ToolMode::DimensionAngular, ToolMode::DimensionRadius,
                             ToolMode::DimensionDiameter})
                          + QList<QAction*>{upcoming(tr("Longueur d'arc"), "A-10"),
                                            upcoming(tr("Ordonnée"), "A-04"),
                                            upcoming(tr("Continue"), "A-03"),
                                            upcoming(tr("Ligne de base"), "A-03"),
                                            upcoming(tr("Cotation rapide"), "A-11"),
                                            upcoming(tr("Interrompre"), "A-15"),
                                            upcoming(tr("Espacer"), "A-15"),
                                            upcoming(tr("Style de cote"), "A-06")}, 1);
    ribbon_->addPanel(tr("Annoter"), tr("Lignes d'axe"),
                      {upcoming(tr("Marque de centre"), "A-14"), upcoming(tr("Ligne d'axe"), "A-14")}, 0);
    ribbon_->addPanel(tr("Annoter"), tr("Lignes de repère"),
                      {upcoming(tr("Repère multiple"), "A-05"), upcoming(tr("Ajouter un repère"), "A-05"),
                       upcoming(tr("Retirer un repère"), "A-05"), upcoming(tr("Aligner les repères"), "A-05")}, 0);
    ribbon_->addPanel(tr("Annoter"), tr("Tableaux"),
                      {upcoming(tr("Tableau"), "A-08"), upcoming(tr("Extraire des données"), "A-08")}, 0);

    // Barre d'acces rapide : les actions de fichier et d'historique, comme
    // la barre d'acces rapide d'AutoCAD (elles ne sont plus dans le ruban).
    auto* quickToolbar = addToolBar(tr("Accès rapide"));
    quickToolbar->setObjectName("quickAccessToolbar");
    quickToolbar->setMovable(false);
    quickToolbar->setIconSize(QSize(18, 18));
    quickToolbar->addActions({newAction, openAction, saveAction, saveAsAction, printAction});
    quickToolbar->addSeparator();
    quickToolbar->addActions({undoAction, redoAction});
    quickToolbar->addSeparator();
    quickToolbar->addAction(zoomFitAction);
}

} // namespace bcad::app
