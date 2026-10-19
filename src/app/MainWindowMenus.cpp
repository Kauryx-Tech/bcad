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
#include "LayerCombo.h"
#include "LayerPanel.h"
#include "RibbonBar.h"
#include "bcad/geometry/BooleanOps.h"

#include <QAction>
#include <QMenu>
#include <QWidgetAction>
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
    QAction* exportDxfAction =
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
        editMenu->addAction(tr("&Effacer"), QKeySequence::Delete, viewport_, &Viewport::deleteSelected);
    QAction* explodeAction = editMenu->addAction(tr("&Décomposer"), viewport_, &Viewport::explodeSelected);
    QAction* joinAction = editMenu->addAction(tr("&Joindre"), viewport_, &Viewport::joinSelected);
    editMenu->addSeparator();
    QAction* selectAllAction = editMenu->addAction(tr("&Sélectionner tout"), QKeySequence::SelectAll, viewport_,
                                                    &Viewport::selectAll);
    QAction* selectLastAction = editMenu->addAction(tr("Sélectionner le &dernier"), viewport_, &Viewport::selectLast);
    // Modifier un texte (D-01b) : aussi par double-clic sur le texte.
    QAction* editTextAction = editMenu->addAction(tr("Modifier le &texte"), viewport_,
                                                  &Viewport::editSelectedText);
    iconAction(editTextAction, QStyle::SP_FileDialogDetailedView, "text-edit");
    iconAction(deleteAction, QStyle::SP_TrashIcon, "edit-delete");
    iconAction(explodeAction, QStyle::SP_FileDialogDetailedView, "object-explode");
    iconAction(joinAction, QStyle::SP_FileDialogListView, "object-join");
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
    addToolActions(viewMenu_, {ToolMode::ZoomWindow});
    QAction* zoomPrevAction = viewMenu_->addAction(tr("Zoom &précédent"), viewport_, &Viewport::zoomPrevious);
    QAction* zoomNextAction = viewMenu_->addAction(tr("Zoom s&uivant"), viewport_, &Viewport::zoomNext);
    addToolActions(viewMenu_, {ToolMode::Pan});
    viewMenu_->addSeparator();
    QAction* snapAction = viewMenu_->addAction(tr("Activer l'&accrochage objet"), Qt::Key_F3,
        viewport_, &Viewport::toggleSnap);
    QAction* gridAction = viewMenu_->addAction(tr("Afficher la &grille"), Qt::Key_F7,
        viewport_, &Viewport::toggleGrid);
    QAction* gridSnapAction = viewMenu_->addAction(tr("Activer l'accrochage à la &grille"), Qt::Key_F9,
        viewport_, &Viewport::toggleGridSnap);
    QAction* orthoAction = viewMenu_->addAction(tr("Activer le mode &orthogonal"), Qt::Key_F8,
        viewport_, &Viewport::toggleOrtho);
    iconAction(zoomFitAction, QStyle::SP_FileDialogContentsView, "zoom-fit-best");
    iconAction(zoomPrevAction, QStyle::SP_ArrowBack, "zoom-previous");
    iconAction(zoomNextAction, QStyle::SP_ArrowForward, "zoom-next");
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
    // Barre de navigation : tout, fenetre, precedent, main — utilisable au
    // pave tactile, sans bouton du milieu (V-01 a V-03).
    addStatusButton(zoomFitAction);
    for (QAction* action : toolActionsFor({ToolMode::ZoomWindow})) addStatusButton(action);
    addStatusButton(zoomPrevAction);
    for (QAction* action : toolActionsFor({ToolMode::Pan})) addStatusButton(action);
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
                              ToolMode::Polygon, ToolMode::Hatch, ToolMode::Point, ToolMode::Text});

    // Libellés courts volontairement : le contexte du panneau/sous-menu
    // "Boolean" indique déjà de quoi il s'agit, et c'est le préfixe qui
    // faisait dépasser "Boolean Symmetric Difference" au-delà de ce qu'un
    // bouton du ruban peut afficher sans être tronqué en quelque chose
    // d'indiscernable de ses voisins.
    // Menu Modifier dans l'ordre de celui d'AutoCAD, booleens a la fin.
    QMenu* modifyMenu = menuBar()->addMenu(tr("&Modifier"));
    addToolActions(modifyMenu, {ToolMode::Move, ToolMode::Rotate, ToolMode::Copy, ToolMode::Mirror,
                                ToolMode::Scale, ToolMode::Stretch, ToolMode::Trim, ToolMode::Extend,
                                ToolMode::Break, ToolMode::Chamfer, ToolMode::Fillet, ToolMode::Array,
                                ToolMode::Offset});
    modifyMenu->addActions({joinAction, explodeAction, deleteAction});
    modifyMenu->addSeparator();
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
    // Renseignements, comme le menu Outils d'AutoCAD : mesurer sans rien creer.
    addToolActions(toolsMenu->addMenu(tr("&Renseignements")), {ToolMode::Measure, ToolMode::Area, ToolMode::Coordinates});

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
    // Ruban d'AutoCAD 2024 en francais : onglets Debut, Inserer, Annoter, Vue,
    // Sortie (les onglets des modules suivent), memes panneaux, memes
    // libelles, boutons deroulants (Ajuster ▾, Raccord ▾…) et panneaux
    // etendus (fleche ▼). Un outil pas encore realise n'a pas de bouton : son
    // panneau apparait le jour de sa livraison (TODO_OUTILS.md).
    auto tools = [this](std::initializer_list<ToolMode> modes) { return toolActionsFor(modes); };
    auto split = [this](const QList<QAction*>& variants) { return RibbonBar::splitAction(this, variants); };
    auto icon = [](QAction* action) { return RibbonBar::iconOnly(action); };
    QAction* layerPropsAction = iconAction(new QAction(tr("Propriétés des calques"), this),
                                           QStyle::SP_FileDialogListView, "layers");
    layerPropsAction->setIconText(tr("Propriétés\ndes calques"));
    connect(layerPropsAction, &QAction::triggered, this, [this] {
        layersDock_->show();
        layersDock_->raise();
    });
    QAction* propsPaletteAction = iconAction(new QAction(tr("Propriétés"), this),
                                             QStyle::SP_FileDialogDetailedView, "properties");
    connect(propsPaletteAction, &QAction::triggered, this, [this] {
        propertiesDock_->show();
        propertiesDock_->raise();
    });

    // --- Debut. Les petits boutons s'empilent par colonnes de trois :
    // Modification reprend les rangees d'AutoCAD (Deplacer / Rotation /
    // Ajuster / Effacer, Copier / Miroir / Raccord / Decomposer, Etirer /
    // Echelle / Reseau / Decaler).
    const QString debut = tr("Début");
    ribbon_->addPanel(debut, tr("Dessin"),
                      tools({ToolMode::Line, ToolMode::Polyline, ToolMode::Circle, ToolMode::Arc})
                          + QList<QAction*>{split(tools({ToolMode::Rectangle, ToolMode::Polygon}))}
                          + tools({ToolMode::Hatch}),
                      4, tools({ToolMode::Point}));
    ribbon_->addPanel(debut, tr("Modification"),
                      tools({ToolMode::Move, ToolMode::Copy, ToolMode::Stretch,
                             ToolMode::Rotate, ToolMode::Mirror, ToolMode::Scale})
                          + QList<QAction*>{split(tools({ToolMode::Trim, ToolMode::Extend})),
                                            split(tools({ToolMode::Fillet, ToolMode::Chamfer})),
                                            toolAction(ToolMode::Array), icon(deleteAction),
                                            icon(explodeAction), icon(toolAction(ToolMode::Offset))},
                      0,
                      tools({ToolMode::Break}) + QList<QAction*>{joinAction, booleanMenu->menuAction()});
    ribbon_->addPanel(debut, tr("Annotation"),
                      tools({ToolMode::Text})
                          + QList<QAction*>{split(tools({ToolMode::DimensionLinear, ToolMode::DimensionAligned,
                                                         ToolMode::DimensionAngular, ToolMode::DimensionRadius,
                                                         ToolMode::DimensionDiameter}))},
                      2, {editTextAction});
    // Calques : Proprietes des calques et la liste des calques (L-02).
    layerCombo_ = new LayerCombo(this);
    layerCombo_->setDocument(document_);
    connect(layerPanel_, &LayerPanel::layersChanged, layerCombo_, &LayerCombo::refresh);
    auto* layerComboAction = new QWidgetAction(this);
    layerComboAction->setText(tr("Calque courant"));
    layerComboAction->setDefaultWidget(layerCombo_);
    ribbon_->addPanel(debut, tr("Calques"), {layerPropsAction, layerComboAction}, 1);
    // Proprietes (Copier les proprietes, listes Couleur / Epaisseur / Type de
    // ligne : M-10, P-03) apparaitra avec ses outils ; la palette s'ouvre
    // depuis Vue › Palettes et la barre d'etat.
    ribbon_->addPanel(debut, tr("Utilitaires"),
                      QList<QAction*>{split(tools({ToolMode::Measure, ToolMode::Area}))}
                          + tools({ToolMode::Coordinates}) + QList<QAction*>{selectAllAction},
                      1, {selectLastAction});

    // --- Inserer : ce qui fait entrer des donnees dans le dessin.
    ribbon_->addPanel(tr("Insérer"), tr("Importer"), {importDxfAction, importMenu_->menuAction()}, 2);

    // --- Annoter : texte et cotations.
    ribbon_->addPanel(tr("Annoter"), tr("Texte"), tools({ToolMode::Text}) + QList<QAction*>{editTextAction}, 1);
    ribbon_->addPanel(tr("Annoter"), tr("Cotations"),
                      QList<QAction*>{split(tools({ToolMode::DimensionLinear, ToolMode::DimensionAligned,
                                                   ToolMode::DimensionAngular, ToolMode::DimensionRadius,
                                                   ToolMode::DimensionDiameter}))},
                      1);

    // --- Vue : navigation 2D et palettes ; libelles courts du bouton Zoom ▾
    // d'AutoCAD (les menus gardent les noms complets).
    zoomFitAction->setIconText(tr("Étendu"));
    zoomPrevAction->setIconText(tr("Précédent"));
    zoomNextAction->setIconText(tr("Suivant"));
    if (QAction* zoomWindow = toolAction(ToolMode::ZoomWindow)) zoomWindow->setIconText(tr("Fenêtre"));
    ribbon_->addPanel(tr("Vue"), tr("Navigation 2D"),
                      tools({ToolMode::Pan})
                          + QList<QAction*>{split(QList<QAction*>{zoomFitAction} + tools({ToolMode::ZoomWindow})
                                                  + QList<QAction*>{zoomPrevAction, zoomNextAction})},
                      2);
    ribbon_->addPanel(tr("Vue"), tr("Palettes"),
                      {layerPropsAction, propsPaletteAction, validationDock_->toggleViewAction()}, 3);

    // --- Sortie : tracer et exporter.
    ribbon_->addPanel(tr("Sortie"), tr("Tracer"), {printAction}, 1);
    ribbon_->addPanel(tr("Sortie"), tr("Exporter"), {exportDxfAction, exportMenu_->menuAction()}, 2);

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

    // Le ruban, sur sa propre rangee pleine largeur sous l'acces rapide.
    addToolBarBreak(Qt::TopToolBarArea);
    auto* ribbonToolbar = addToolBar(tr("Ruban"));
    ribbonToolbar->setObjectName("ribbonToolbar");
    ribbonToolbar->setMovable(false);
    ribbonToolbar->setFloatable(false);
    ribbonToolbar->toggleViewAction()->setVisible(false);
    ribbon_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ribbonToolbar->addWidget(ribbon_);
}

} // namespace bcad::app
