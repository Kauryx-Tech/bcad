#include "bcad/app/MainWindow.h"

#include "bcad/app/LayerPanel.h"
#include "bcad/app/PropertiesPanel.h"
#include "bcad/app/RibbonBar.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/io/Database.h"
#include "bcad/io/DxfReader.h"
#include "bcad/io/DxfWriter.h"
#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>

namespace bcad::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    document_ = std::make_unique<core::Document>();

    viewport_ = new Viewport(this);
    viewport_->setDocument(document_.get());
    viewport_->setUndoStack(&undoStack_);

    ribbon_ = new RibbonBar(this);

    // Le ruban se place entre la barre de menus et le canevas, comme le
    // ruban des versions récentes d'AutoCAD — enveloppé dans un simple
    // conteneur puisque l'emplacement du widget central de QMainWindow
    // n'accepte qu'un seul widget.
    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(ribbon_);
    centralLayout->addWidget(viewport_, 1);
    setCentralWidget(central);

    buildMenusAndRibbon();
    buildDockWidgets();
    buildCommandLine();
    applyDarkTheme();

    coordLabel_ = new QLabel(tr("X: 0.000  Y: 0.000"), this);
    toolLabel_ = new QLabel(tr("Select"), this);
    statusBar()->addPermanentWidget(toolLabel_);
    statusBar()->addPermanentWidget(coordLabel_);

    connect(viewport_, &Viewport::cursorWorldPositionChanged, this, &MainWindow::onCursorMoved);
    connect(viewport_, &Viewport::toolChanged, this, &MainWindow::onToolChanged);
    connect(viewport_, &Viewport::typedInputRequested, this, &MainWindow::onTypedInputRequested);
    connect(viewport_, &Viewport::selectionChanged, propertiesPanel_, &PropertiesPanel::refresh);

    setWindowTitle(tr("bcad"));
    resize(1280, 800);
}

void MainWindow::buildMenusAndRibbon() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), QKeySequence::New, this, &MainWindow::onNew);
    fileMenu->addAction(tr("&Open..."), QKeySequence::Open, this, &MainWindow::onOpen);
    fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::onSave);
    fileMenu->addAction(tr("Save &As..."), QKeySequence::SaveAs, this, &MainWindow::onSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Import DXF..."), this, &MainWindow::onImportDxf);
    fileMenu->addAction(tr("&Export DXF..."), this, &MainWindow::onExportDxf);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = undoStack_.createUndoAction(this, tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAction);
    QAction* redoAction = undoStack_.createRedoAction(this, tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    QAction* deleteAction =
        editMenu->addAction(tr("&Delete"), QKeySequence::Delete, viewport_, &Viewport::deleteSelected);
    QAction* explodeAction = editMenu->addAction(tr("E&xplode"), viewport_, &Viewport::explodeSelected);
    QAction* joinAction = editMenu->addAction(tr("&Join"), viewport_, &Viewport::joinSelected);
    editMenu->addSeparator();
    QAction* selectAllAction = editMenu->addAction(tr("Select &All"), QKeySequence::SelectAll, viewport_,
                                                    &Viewport::selectAll);
    QAction* selectLastAction = editMenu->addAction(tr("Select &Last"), viewport_, &Viewport::selectLast);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QAction* zoomFitAction = viewMenu->addAction(tr("Zoom to &Fit"), Qt::Key_F, viewport_, &Viewport::zoomToFit);
    QAction* snapAction =
        viewMenu->addAction(tr("Toggle Object &Snap"), Qt::Key_F3, viewport_, &Viewport::toggleSnap);
    QAction* gridAction = viewMenu->addAction(tr("Toggle &Grid"), Qt::Key_F7, viewport_, &Viewport::toggleGrid);
    QAction* gridSnapAction =
        viewMenu->addAction(tr("Toggle Grid Sn&ap"), Qt::Key_F9, viewport_, &Viewport::toggleGridSnap);
    QAction* orthoAction = viewMenu->addAction(tr("Toggle &Ortho"), Qt::Key_F8, viewport_, &Viewport::toggleOrtho);

    // Libellés courts volontairement : le contexte du panneau/sous-menu
    // "Boolean" indique déjà de quoi il s'agit, et c'est le préfixe qui
    // faisait dépasser "Boolean Symmetric Difference" au-delà de ce qu'un
    // bouton du ruban peut afficher sans être tronqué en quelque chose
    // d'indiscernable de ses voisins.
    QMenu* modifyMenu = menuBar()->addMenu(tr("&Modify"));
    QMenu* booleanMenu = modifyMenu->addMenu(tr("&Boolean"));
    QAction* unionAction =
        booleanMenu->addAction(tr("&Union"), this, [this] { viewport_->booleanOperation(geom::BooleanOp::Union); });
    QAction* intersectAction = booleanMenu->addAction(
        tr("&Intersection"), this, [this] { viewport_->booleanOperation(geom::BooleanOp::Intersection); });
    QAction* diffAction = booleanMenu->addAction(
        tr("&Difference"), this, [this] { viewport_->booleanOperation(geom::BooleanOp::Difference); });
    QAction* symDiffAction =
        booleanMenu->addAction(tr("&Symmetric Difference"), this,
                                [this] { viewport_->booleanOperation(geom::BooleanOp::SymmetricDifference); });

    // --- Ruban : réutilise exactement les mêmes objets QAction comme
    // boutons regroupés en onglets/panneaux, de sorte que déclencher un
    // bouton du ruban et l'élément de menu/raccourci équivalent fassent
    // la même chose, sans rien à maintenir synchronisé.
    auto* toolGroup = new QActionGroup(this);
    auto addToolAction = [&](const QString& label, ToolMode mode) {
        QAction* action = new QAction(label, this);
        action->setCheckable(true);
        action->setActionGroup(toolGroup);
        connect(action, &QAction::triggered, this, [this, mode] { viewport_->setTool(mode); });
        return action;
    };
    QAction* selectAction = addToolAction(tr("Select"), ToolMode::Select);
    QAction* moveAction = addToolAction(tr("Move"), ToolMode::Move);
    QAction* copyAction = addToolAction(tr("Copy"), ToolMode::Copy);
    QAction* rotateAction = addToolAction(tr("Rotate"), ToolMode::Rotate);
    QAction* scaleAction = addToolAction(tr("Scale"), ToolMode::Scale);
    QAction* mirrorAction = addToolAction(tr("Mirror"), ToolMode::Mirror);
    QAction* trimAction = addToolAction(tr("Trim"), ToolMode::Trim);
    QAction* extendAction = addToolAction(tr("Extend"), ToolMode::Extend);
    QAction* breakAction = addToolAction(tr("Break"), ToolMode::Break);
    QAction* lineAction = addToolAction(tr("Line"), ToolMode::Line);
    QAction* circleAction = addToolAction(tr("Circle"), ToolMode::Circle);
    QAction* arcAction = addToolAction(tr("Arc"), ToolMode::Arc);
    QAction* polylineAction = addToolAction(tr("Polyline"), ToolMode::Polyline);
    selectAction->setChecked(true);

    ribbon_->addPanel(tr("Home"), tr("Draw"),
                       { selectAction, moveAction, lineAction, circleAction, arcAction, polylineAction });

    ribbon_->addPanel(tr("Modify"), tr("Transform"), { copyAction, rotateAction, scaleAction, mirrorAction });
    ribbon_->addPanel(tr("Modify"), tr("Trim"), { trimAction, extendAction, breakAction });
    ribbon_->addPanel(tr("Modify"), tr("Boolean"), { unionAction, intersectAction, diffAction, symDiffAction });
    ribbon_->addPanel(tr("Modify"), tr("Edit"),
                       { deleteAction, explodeAction, joinAction, undoAction, redoAction });
    ribbon_->addPanel(tr("Home"), tr("Select"), { selectAllAction, selectLastAction });

    ribbon_->addPanel(tr("View"), tr("Navigate"), { zoomFitAction });
    ribbon_->addPanel(tr("View"), tr("Snapping"), { snapAction, gridAction, gridSnapAction, orthoAction });
}

void MainWindow::buildDockWidgets() {
    auto* layersDock = new QDockWidget(tr("Layers"), this);
    layersDock->setObjectName("layersDock");
    layerPanel_ = new LayerPanel(layersDock);
    layerPanel_->setDocument(document_.get());
    layersDock->setWidget(layerPanel_);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);

    // Regroupé en onglets avec Calques par défaut (comme les docks
    // Propriétés/Calques des outils de la famille AutoCAD) plutôt
    // qu'empilé, afin que les deux restent accessibles sans diviser
    // en permanence la colonne de droite en deux.
    auto* propertiesDock = new QDockWidget(tr("Properties"), this);
    propertiesDock->setObjectName("propertiesDock");
    propertiesPanel_ = new PropertiesPanel(propertiesDock);
    propertiesPanel_->setDocument(document_.get());
    propertiesPanel_->setUndoStack(&undoStack_);
    propertiesDock->setWidget(propertiesPanel_);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);
    tabifyDockWidget(layersDock, propertiesDock);
    layersDock->raise();

    ribbon_->addPanel(tr("Home"), tr("Panels"),
                       { layersDock->toggleViewAction(), propertiesDock->toggleViewAction() });
}

void MainWindow::buildCommandLine() {
    commandLine_ = new QLineEdit(this);
    commandLine_->setPlaceholderText(
        tr("Type a point: x,y  |  @dx,dy  |  @dist<angle  — Enter to confirm, Esc to cancel"));
    connect(commandLine_, &QLineEdit::returnPressed, this, &MainWindow::onCommandLineSubmitted);
    statusBar()->addWidget(commandLine_, 1);
}

void MainWindow::applyDarkTheme() {
    // Un thème sombre plat, à faible saturation, dans l'esprit des espaces
    // de travail modernes AutoCAD/Fusion 360 : une interface neutre et
    // sombre pour que les couleurs pleinement saturées des entités (ce qui
    // compte vraiment sur un canevas CAO) ressortent clairement dessus.
    setStyleSheet(R"(
        QMainWindow, QDockWidget, QMenuBar, QMenu, QStatusBar { background-color: #2b2d31; color: #e0e0e0; }
        QMenuBar::item:selected, QMenu::item:selected { background-color: #3f7fbf; }
        QDockWidget::title { background-color: #202124; padding: 4px; }
        QTreeWidget { background-color: #202124; color: #e0e0e0; border: none; }
        QTreeWidget::item:selected { background-color: #3f7fbf; }
        QHeaderView::section { background-color: #2b2d31; color: #b0b0b0; border: none; padding: 2px; }
        QStatusBar QLabel { color: #b0b0b0; padding: 0 8px; }
        QPushButton { background-color: #3a3d42; color: #e0e0e0; border: 1px solid #4a4d52; border-radius: 3px; padding: 4px 10px; }
        QPushButton:hover { background-color: #45484e; }
        QLineEdit { background-color: #202124; color: #e0e0e0; border: 1px solid #4a4d52; border-radius: 3px; padding: 2px 6px; }

        QTabWidget::pane { border: none; background-color: #2b2d31; }
        RibbonBar QTabBar::tab { background-color: #2b2d31; color: #b0b0b0; padding: 4px 16px; border: none; }
        RibbonBar QTabBar::tab:selected { background-color: #35373c; color: #e0e0e0; border-bottom: 2px solid #3f7fbf; }
        RibbonBar QTabBar::tab:hover { color: #e0e0e0; }
        RibbonBar QToolButton { background-color: transparent; border: 1px solid transparent; border-radius: 3px; padding: 4px 8px; color: #e0e0e0; }
        RibbonBar QToolButton:checked { background-color: #3f7fbf; border-color: #5a9fdf; }
        RibbonBar QToolButton:hover { background-color: #3a3d42; border-color: #4a4d52; }
        #ribbonPanelCaption { color: #808388; font-size: 10px; }
        #ribbonSeparator { color: #45484e; }
    )");
}

void MainWindow::onNew() {
    document_->clear();
    document_->layerManager().reset();
    undoStack_.clear();
    currentFilePath_.clear();
    viewport_->update();
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(this, tr("Open Project"), {}, tr("bcad Project (*.bcad)"));
    if (path.isEmpty()) return;
    if (!io::Database::load(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Open Failed"), tr("Could not open '%1'.").arg(path));
        return;
    }
    undoStack_.clear();
    currentFilePath_ = path;
    viewport_->zoomToFit();
}

bool MainWindow::saveToPath(const QString& path) {
    if (!io::Database::save(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not save to '%1'.").arg(path));
        return false;
    }
    currentFilePath_ = path;
    return true;
}

void MainWindow::onSave() {
    if (currentFilePath_.isEmpty()) {
        onSaveAs();
        return;
    }
    saveToPath(currentFilePath_);
}

void MainWindow::onSaveAs() {
    QString path = QFileDialog::getSaveFileName(this, tr("Save Project As"), {}, tr("bcad Project (*.bcad)"));
    if (path.isEmpty()) return;
    saveToPath(path);
}

void MainWindow::onImportDxf() {
    QString path = QFileDialog::getOpenFileName(this, tr("Import DXF"), {}, tr("DXF Files (*.dxf)"));
    if (path.isEmpty()) return;
    if (!io::readDxf(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Import Failed"), tr("Could not read '%1'.").arg(path));
        return;
    }
    undoStack_.clear();
    currentFilePath_.clear();
    viewport_->zoomToFit();
}

void MainWindow::onExportDxf() {
    QString path = QFileDialog::getSaveFileName(this, tr("Export DXF"), {}, tr("DXF Files (*.dxf)"));
    if (path.isEmpty()) return;
    if (!io::writeDxf(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not write '%1'.").arg(path));
    }
}

void MainWindow::onCursorMoved(double x, double y) {
    coordLabel_->setText(tr("X: %1  Y: %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
}

void MainWindow::onToolChanged(ToolMode mode) {
    switch (mode) {
        case ToolMode::Select: toolLabel_->setText(tr("Select")); break;
        case ToolMode::Move: toolLabel_->setText(tr("Move")); break;
        case ToolMode::Copy: toolLabel_->setText(tr("Copy")); break;
        case ToolMode::Rotate: toolLabel_->setText(tr("Rotate")); break;
        case ToolMode::Scale: toolLabel_->setText(tr("Scale")); break;
        case ToolMode::Mirror: toolLabel_->setText(tr("Mirror")); break;
        case ToolMode::Trim: toolLabel_->setText(tr("Trim")); break;
        case ToolMode::Extend: toolLabel_->setText(tr("Extend")); break;
        case ToolMode::Break: toolLabel_->setText(tr("Break")); break;
        case ToolMode::Line: toolLabel_->setText(tr("Line")); break;
        case ToolMode::Circle: toolLabel_->setText(tr("Circle")); break;
        case ToolMode::Arc: toolLabel_->setText(tr("Arc")); break;
        case ToolMode::Polyline: toolLabel_->setText(tr("Polyline")); break;
    }
}

void MainWindow::onTypedInputRequested(const QString& initialText) {
    commandLine_->setText(initialText);
    commandLine_->setFocus();
    commandLine_->end(false);
}

void MainWindow::onCommandLineSubmitted() {
    QString text = commandLine_->text();
    commandLine_->clear();
    viewport_->submitTypedPoint(text);
    viewport_->setFocus();
}

} // namespace bcad::app
