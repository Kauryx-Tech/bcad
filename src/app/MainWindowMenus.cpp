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
#include <QToolBar>
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
    iconAction(newAction, QStyle::SP_FileIcon, "document-new");
    iconAction(openAction, QStyle::SP_DirOpenIcon, "document-open");
    iconAction(saveAction, QStyle::SP_DialogSaveButton, "document-save");
    iconAction(saveAsAction, QStyle::SP_DialogSaveButton, "document-save-as");
    fileMenu->addSeparator();
    iconAction(fileMenu->addAction(tr("&Importer DXF..."), this, &MainWindow::onImportDxf),
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
    QAction* undoAction = undoStack_.createUndoAction(this, tr("&Annuler"));
    undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAction);
    QAction* redoAction = undoStack_.createRedoAction(this, tr("&Rétablir"));
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
    iconAction(deleteAction, QStyle::SP_TrashIcon, "edit-delete");
    iconAction(explodeAction, QStyle::SP_FileDialogDetailedView, "object-ungroup");
    iconAction(joinAction, QStyle::SP_FileDialogListView, "object-group");
    iconAction(selectAllAction, QStyle::SP_DialogYesButton, "edit-select-all");
    iconAction(selectLastAction, QStyle::SP_ArrowUp, "go-last");

    viewMenu_ = menuBar()->addMenu(tr("&Affichage"));
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
    const std::pair<QAction*, bool> toggles[] = {
        {snapAction, viewport_->snapEnabled()}, {gridAction, viewport_->gridVisible()},
        {gridSnapAction, viewport_->gridSnapEnabled()}, {orthoAction, viewport_->orthoEnabled()}};
    for (const auto& [action, on] : toggles) {
        action->setCheckable(true);
        action->setChecked(on);
        auto* button = new QToolButton(statusBar());
        button->setDefaultAction(action);
        button->setIconSize(QSize(18, 18));
        button->setAutoRaise(true);
        statusBar()->addPermanentWidget(button);
    }
    // Les bascules des panneaux viennent de la suite : ce sont des vues, pas des
    // calques, et elles etaient rangees dans le menu « Calque ».

    QMenu* drawMenu = menuBar()->addMenu(tr("&Dessin"));
    addToolActions(drawMenu, {ToolMode::Select, ToolMode::Line, ToolMode::Polyline,
                              ToolMode::Circle, ToolMode::Arc, ToolMode::Rectangle,
                              ToolMode::Point});

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
    ribbon_->addPanel(tr("Accueil"), tr("Dessin"),
                      toolActionsFor({ToolMode::Line, ToolMode::Polyline, ToolMode::Circle,
                                      ToolMode::Arc, ToolMode::Rectangle, ToolMode::Point}), 4);
    ribbon_->addPanel(tr("Accueil"), tr("Modification"),
                      toolActionsFor({ToolMode::Move, ToolMode::Copy, ToolMode::Rotate,
                                      ToolMode::Mirror, ToolMode::Scale, ToolMode::Trim,
                                      ToolMode::Extend, ToolMode::Break})
                          + QList<QAction*>{deleteAction}, 0);
    ribbon_->addPanel(tr("Accueil"), tr("Annotation"),
                      toolActionsFor({ToolMode::DimensionLinear, ToolMode::DimensionAligned,
                                      ToolMode::DimensionAngular, ToolMode::DimensionRadius}), 1);
    ribbon_->addPanel(tr("Accueil"), tr("Sélection"),
                      toolActionsFor({ToolMode::Select}) + QList<QAction*>{selectAllAction, selectLastAction}, 1);

    ribbon_->addPanel(tr("Modifier"), tr("Transformation"),
                      toolActionsFor({ToolMode::Move, ToolMode::Copy, ToolMode::Rotate,
                                      ToolMode::Scale, ToolMode::Mirror}), 2);
    ribbon_->addPanel(tr("Modifier"), tr("Rogner"),
                      toolActionsFor({ToolMode::Trim, ToolMode::Extend, ToolMode::Break}), 1);
    ribbon_->addPanel(tr("Modifier"), tr("Booléen"),
                      { unionAction, intersectAction, diffAction, symDiffAction }, 2);
    ribbon_->addPanel(tr("Modifier"), tr("Édition"),
                      { deleteAction, explodeAction, joinAction, undoAction, redoAction }, 1);

    ribbon_->addPanel(tr("Annoter"), tr("Longueurs"),
                      toolActionsFor({ToolMode::DimensionLinear, ToolMode::DimensionAligned}));
    ribbon_->addPanel(tr("Annoter"), tr("Angles et rayons"),
                      toolActionsFor({ToolMode::DimensionAngular, ToolMode::DimensionRadius,
                                      ToolMode::DimensionDiameter}));

    ribbon_->addPanel(tr("Affichage"), tr("Navigation"), { zoomFitAction });
    ribbon_->addPanel(tr("Affichage"), tr("Accrochage"),
                      { snapAction, gridAction, gridSnapAction, orthoAction }, 2);

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
