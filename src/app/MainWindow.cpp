#include "bcad/app/MainWindow.h"

#include "bcad/app/LayerPanel.h"
#include "bcad/app/PropertiesPanel.h"
#include "bcad/app/RibbonBar.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/io/Database.h"
#include "bcad/io/DxfReader.h"
#include "bcad/io/DxfWriter.h"
#include "bcad/layout/Cartouche.h"
#include "bcad/layout/PdfExport.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include "bcad/plugin/Plugin.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/app/QtCommandAdapter.h"
#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPainter>
#include <QStatusBar>
#include <QStyle>
#include <QInputDialog>
#include <QIcon>
#include <QToolBar>
#include <QTimer>
#include <QVBoxLayout>
#include <fstream>
#include <iomanip>
#include <QDir>
#include <QCoreApplication>

namespace bcad::app {

namespace {
void setActionIcon(QWidget* widget, QAction* action, QStyle::StandardPixmap fallback,
                   const QString& themeName) {
    QIcon icon = QIcon::fromTheme(themeName);
    if (icon.isNull()) icon = widget->style()->standardIcon(fallback);
    action->setIcon(icon);
}
}

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

    coordLabel_ = new QLabel(tr("X : 0,000  Y : 0,000"), this);
    toolLabel_ = new QLabel(tr("Sélection"), this);
    statusBar()->addPermanentWidget(toolLabel_);
    statusBar()->addPermanentWidget(coordLabel_);

    connect(viewport_, &Viewport::cursorWorldPositionChanged, this, &MainWindow::onCursorMoved);
    connect(viewport_, &Viewport::toolChanged, this, &MainWindow::onToolChanged);
    connect(viewport_, &Viewport::typedInputRequested, this, &MainWindow::onTypedInputRequested);
    connect(viewport_, &Viewport::selectionChanged, propertiesPanel_, &PropertiesPanel::refresh);
    connect(&undoStack_, &QUndoStack::indexChanged, this, [this] { dirty_ = true; });

    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setInterval(120000); // 2 min
    connect(autosaveTimer_, &QTimer::timeout, this, &MainWindow::onAutosaveTimeout);
    autosaveTimer_->start();

    const QByteArray configuredPlugin = qgetenv("BCAD_PLUGIN_PATH");
    QString pluginPath = QString::fromUtf8(configuredPlugin);
    if (pluginPath.isEmpty()) {
        const QString pluginBase = QDir(QCoreApplication::applicationDirPath())
                                       .filePath("../../plugins/bcad_cadastre_plugin");
        pluginPath = pluginBase;
#ifdef Q_OS_WIN
        if (!QFileInfo::exists(pluginPath)) pluginPath += ".dll";
#elif defined(Q_OS_MACOS)
        if (!QFileInfo::exists(pluginPath)) pluginPath += ".dylib";
#else
        // CMake emits this plugin without a suffix in the development tree.
        // Installed builds may use the conventional .so suffix.
        if (!QFileInfo::exists(pluginPath)) pluginPath += ".so";
#endif
    }
    cadastrePlugin_ = plugin::pluginManager().loadPlugin(pluginPath.toStdString());
    if (cadastrePlugin_) {
        statusBar()->showMessage(
            tr("Plugin cadastral chargé : %1").arg(QString::fromStdString(cadastrePlugin_->info.name)),
            5000);
    } else if (!configuredPlugin.isEmpty()) {
        statusBar()->showMessage(tr("Impossible de charger le plugin cadastral : %1").arg(pluginPath), 10000);
    }

    setWindowTitle(tr("bcad"));
    resize(1280, 800);
}

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
    fileMenu->addAction(tr("&Importer DXF..."), this, &MainWindow::onImportDxf);
    fileMenu->addAction(tr("&Exporter DXF..."), this, &MainWindow::onExportDxf);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Exporter &GeoJSON..."), this, &MainWindow::onExportGeoJson);
    fileMenu->addAction(tr("Exporter &CSV des coordonnées..."), this, &MainWindow::onExportCsv);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Aperçu avant &impression..."), Qt::CTRL | Qt::Key_P, this, &MainWindow::onPrintPreview);
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

    QMenu* viewMenu = menuBar()->addMenu(tr("&Affichage"));
    QAction* zoomFitAction = viewMenu->addAction(tr("Zoomer sur &tout"), Qt::Key_F, viewport_, &Viewport::zoomToFit);
    QAction* snapAction =
        viewMenu->addAction(tr("Activer l'&accrochage objet"), Qt::Key_F3, viewport_, &Viewport::toggleSnap);
    QAction* gridAction = viewMenu->addAction(tr("Afficher la &grille"), Qt::Key_F7, viewport_, &Viewport::toggleGrid);
    QAction* gridSnapAction =
        viewMenu->addAction(tr("Activer l'accrochage à la &grille"), Qt::Key_F9, viewport_, &Viewport::toggleGridSnap);
    QAction* orthoAction = viewMenu->addAction(tr("Activer le mode &orthogonal"), Qt::Key_F8, viewport_, &Viewport::toggleOrtho);
    iconAction(zoomFitAction, QStyle::SP_FileDialogContentsView, "zoom-fit-best");
    iconAction(snapAction, QStyle::SP_DialogApplyButton, "snap-object");
    iconAction(gridAction, QStyle::SP_DialogHelpButton, "view-grid");
    iconAction(gridSnapAction, QStyle::SP_DialogApplyButton, "snap-grid");
    iconAction(orthoAction, QStyle::SP_ArrowRight, "orthogonal");

    QMenu* drawMenu = menuBar()->addMenu(tr("&Dessin"));
    auto addDrawAction = [&](const QString& label, ToolMode mode) {
        return drawMenu->addAction(label, this, [this, mode] { viewport_->setTool(mode); });
    };
    QAction* selectMenuAction = addDrawAction(tr("&Sélectionner"), ToolMode::Select);
    drawMenu->addSeparator();
    QAction* lineMenuAction = addDrawAction(tr("&Ligne"), ToolMode::Line);
    QAction* polylineMenuAction = addDrawAction(tr("&Polyligne"), ToolMode::Polyline);
    QAction* circleMenuAction = addDrawAction(tr("&Cercle"), ToolMode::Circle);
    QAction* arcMenuAction = addDrawAction(tr("&Arc"), ToolMode::Arc);
    QAction* rectangleMenuAction = addDrawAction(tr("&Rectangle"), ToolMode::Rectangle);
    QAction* pointMenuAction = addDrawAction(tr("&Point"), ToolMode::Point);
    iconAction(selectMenuAction, QStyle::SP_FileDialogContentsView, "cursor-arrow");
    iconAction(lineMenuAction, QStyle::SP_LineEditClearButton, "draw-line");
    iconAction(polylineMenuAction, QStyle::SP_FileDialogListView, "draw-polyline");
    iconAction(circleMenuAction, QStyle::SP_DialogYesButton, "draw-circle");
    iconAction(arcMenuAction, QStyle::SP_BrowserReload, "draw-arc");
    iconAction(rectangleMenuAction, QStyle::SP_FileDialogDetailedView, "draw-rectangle");
    iconAction(pointMenuAction, QStyle::SP_DialogOkButton, "draw-point");

    // Libellés courts volontairement : le contexte du panneau/sous-menu
    // "Boolean" indique déjà de quoi il s'agit, et c'est le préfixe qui
    // faisait dépasser "Boolean Symmetric Difference" au-delà de ce qu'un
    // bouton du ruban peut afficher sans être tronqué en quelque chose
    // d'indiscernable de ses voisins.
    QMenu* modifyMenu = menuBar()->addMenu(tr("&Modifier"));
    QMenu* booleanMenu = modifyMenu->addMenu(tr("&Opérations booléennes"));
    QAction* unionAction =
        booleanMenu->addAction(tr("&Union"), this, [this] { viewport_->booleanOperation(geom::BooleanOp::Union); });
    QAction* intersectAction = booleanMenu->addAction(
        tr("&Intersection"), this, [this] { viewport_->booleanOperation(geom::BooleanOp::Intersection); });
    QAction* diffAction = booleanMenu->addAction(
        tr("&Différence"), this, [this] { viewport_->booleanOperation(geom::BooleanOp::Difference); });
    QAction* symDiffAction =
        booleanMenu->addAction(tr("Différence &symétrique"), this,
                                [this] { viewport_->booleanOperation(geom::BooleanOp::SymmetricDifference); });

    QMenu* dimensionMenu = menuBar()->addMenu(tr("&Cotation"));
    auto addDimensionMenuAction = [&](const QString& text, ToolMode mode,
                                      QStyle::StandardPixmap fallback,
                                      const QString& theme) {
        QAction* action = dimensionMenu->addAction(text, this,
            [this, mode] { viewport_->setTool(mode); });
        iconAction(action, fallback, theme);
        return action;
    };
    QAction* linearDimensionMenuAction = addDimensionMenuAction(
        tr("Cotation &linéaire"), ToolMode::DimensionLinear,
        QStyle::SP_LineEditClearButton, "measure");
    QAction* alignedDimensionMenuAction = addDimensionMenuAction(
        tr("Cotation &alignée"), ToolMode::DimensionAligned,
        QStyle::SP_LineEditClearButton, "measure");
    QAction* angularDimensionMenuAction = addDimensionMenuAction(
        tr("Cotation &angulaire"), ToolMode::DimensionAngular,
        QStyle::SP_BrowserReload, "measure-angle");
    QAction* radiusDimensionMenuAction = addDimensionMenuAction(
        tr("Cotation de &rayon"), ToolMode::DimensionRadius,
        QStyle::SP_DialogYesButton, "measure-radius");
    QAction* diameterDimensionMenuAction = addDimensionMenuAction(
        tr("Cotation de &diamètre"), ToolMode::DimensionDiameter,
        QStyle::SP_DialogYesButton, "measure-diameter");

    layerMenu_ = menuBar()->addMenu(tr("&Calque"));

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
    QAction* selectAction = addToolAction(tr("Sélection"), ToolMode::Select);
    QAction* moveAction = addToolAction(tr("Déplacer"), ToolMode::Move);
    QAction* copyAction = addToolAction(tr("Copier"), ToolMode::Copy);
    QAction* rotateAction = addToolAction(tr("Tourner"), ToolMode::Rotate);
    QAction* scaleAction = addToolAction(tr("Échelle"), ToolMode::Scale);
    QAction* mirrorAction = addToolAction(tr("Symétrie"), ToolMode::Mirror);
    QAction* trimAction = addToolAction(tr("Rogner"), ToolMode::Trim);
    QAction* extendAction = addToolAction(tr("Prolonger"), ToolMode::Extend);
    QAction* breakAction = addToolAction(tr("Scinder"), ToolMode::Break);
    QAction* lineAction = addToolAction(tr("Ligne"), ToolMode::Line);
    QAction* circleAction = addToolAction(tr("Cercle"), ToolMode::Circle);
    QAction* arcAction = addToolAction(tr("Arc"), ToolMode::Arc);
    QAction* polylineAction = addToolAction(tr("Polyligne"), ToolMode::Polyline);
    QAction* rectangleAction = addToolAction(tr("Rectangle"), ToolMode::Rectangle);
    QAction* pointAction = addToolAction(tr("Point"), ToolMode::Point);
    QAction* dimensionAction = addToolAction(tr("Linéaire"), ToolMode::DimensionLinear);
    QAction* alignedDimensionAction = addToolAction(tr("Alignée"), ToolMode::DimensionAligned);
    QAction* angularDimensionAction = addToolAction(tr("Angulaire"), ToolMode::DimensionAngular);
    QAction* radiusDimensionAction = addToolAction(tr("Rayon"), ToolMode::DimensionRadius);
    QAction* diameterDimensionAction = addToolAction(tr("Diamètre"), ToolMode::DimensionDiameter);
    iconAction(selectAction, QStyle::SP_FileDialogContentsView, "cursor-arrow");
    iconAction(moveAction, QStyle::SP_ArrowRight, "transform-move");
    iconAction(copyAction, QStyle::SP_FileDialogContentsView, "edit-copy");
    iconAction(rotateAction, QStyle::SP_BrowserReload, "object-rotate-right");
    iconAction(scaleAction, QStyle::SP_ArrowUp, "transform-scale");
    iconAction(mirrorAction, QStyle::SP_ArrowLeft, "object-flip-horizontal");
    iconAction(trimAction, QStyle::SP_LineEditClearButton, "edit-cut");
    iconAction(extendAction, QStyle::SP_ArrowRight, "go-next");
    iconAction(breakAction, QStyle::SP_BrowserStop, "edit-split");
    iconAction(lineAction, QStyle::SP_LineEditClearButton, "draw-line");
    iconAction(circleAction, QStyle::SP_DialogYesButton, "draw-circle");
    iconAction(arcAction, QStyle::SP_BrowserReload, "draw-arc");
    iconAction(polylineAction, QStyle::SP_FileDialogListView, "draw-polyline");
    iconAction(rectangleAction, QStyle::SP_FileDialogDetailedView, "draw-rectangle");
    iconAction(pointAction, QStyle::SP_DialogOkButton, "draw-point");
    iconAction(dimensionAction, QStyle::SP_LineEditClearButton, "measure");
    iconAction(alignedDimensionAction, QStyle::SP_LineEditClearButton, "measure");
    iconAction(angularDimensionAction, QStyle::SP_BrowserReload, "measure-angle");
    iconAction(radiusDimensionAction, QStyle::SP_DialogYesButton, "measure-radius");
    iconAction(diameterDimensionAction, QStyle::SP_DialogYesButton, "measure-diameter");
    selectAction->setChecked(true);

    ribbon_->addPanel(tr("Accueil"), tr("Dessin"),
                       { selectAction, moveAction, lineAction, circleAction, arcAction, polylineAction,
                         rectangleAction, pointAction });
    ribbon_->addPanel(tr("Annoter"), tr("Longueurs"),
                      {dimensionAction, alignedDimensionAction});
    ribbon_->addPanel(tr("Annoter"), tr("Angles et rayons"),
                      {angularDimensionAction, radiusDimensionAction,
                       diameterDimensionAction});
    ribbon_->addPanel(tr("Accueil"), tr("Projet"),
                       { newAction, openAction, saveAction, saveAsAction,
                         undoAction, redoAction });

    ribbon_->addPanel(tr("Modifier"), tr("Transformation"), { copyAction, rotateAction, scaleAction, mirrorAction });
    ribbon_->addPanel(tr("Modifier"), tr("Rogner"), { trimAction, extendAction, breakAction });
    ribbon_->addPanel(tr("Modifier"), tr("Booléen"), { unionAction, intersectAction, diffAction, symDiffAction });
    ribbon_->addPanel(tr("Modifier"), tr("Édition"),
                       { deleteAction, explodeAction, joinAction, undoAction, redoAction });
    ribbon_->addPanel(tr("Accueil"), tr("Sélection"), { selectAllAction, selectLastAction });

    ribbon_->addPanel(tr("Affichage"), tr("Navigation"), { zoomFitAction });
    ribbon_->addPanel(tr("Affichage"), tr("Accrochage"), { snapAction, gridAction, gridSnapAction, orthoAction });

    auto* quickToolbar = addToolBar(tr("Accès rapide"));
    quickToolbar->setObjectName("quickAccessToolbar");
    quickToolbar->setMovable(false);
    quickToolbar->setIconSize(QSize(18, 18));
    quickToolbar->addAction(newAction);
    quickToolbar->addAction(openAction);
    quickToolbar->addAction(saveAction);
    quickToolbar->addSeparator();
    quickToolbar->addAction(undoAction);
    quickToolbar->addAction(redoAction);
    quickToolbar->addSeparator();
    quickToolbar->addAction(selectAction);
    quickToolbar->addAction(lineAction);
    quickToolbar->addAction(polylineAction);
    quickToolbar->addAction(zoomFitAction);
}

void MainWindow::buildDockWidgets() {
    auto* layersDock = new QDockWidget(tr("Calques"), this);
    layersDock->setObjectName("layersDock");
    layerPanel_ = new LayerPanel(layersDock);
    layerPanel_->setDocument(document_.get());
    layersDock->setWidget(layerPanel_);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);

    // Regroupé en onglets avec Calques par défaut (comme les docks
    // Propriétés/Calques des outils de la famille AutoCAD) plutôt
    // qu'empilé, afin que les deux restent accessibles sans diviser
    // en permanence la colonne de droite en deux.
    auto* propertiesDock = new QDockWidget(tr("Propriétés"), this);
    propertiesDock->setObjectName("propertiesDock");
    propertiesPanel_ = new PropertiesPanel(propertiesDock);
    propertiesPanel_->setDocument(document_.get());
    propertiesPanel_->setUndoStack(&undoStack_);
    propertiesDock->setWidget(propertiesPanel_);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);
    tabifyDockWidget(layersDock, propertiesDock);
    layersDock->raise();

    ribbon_->addPanel(tr("Accueil"), tr("Panneaux"),
                       { layersDock->toggleViewAction(), propertiesDock->toggleViewAction() });

    layerMenu_->addAction(layersDock->toggleViewAction());
    layerMenu_->addAction(propertiesDock->toggleViewAction());

    QMenu* toolsMenu = menuBar()->addMenu(tr("&Outils"));
    toolsMenu->addAction(tr("Aperçu avant impression..."), this, &MainWindow::onPrintPreview);

    QMenu* cadastreMenu = menuBar()->addMenu(tr("&Cadastre"));
    QAction* createParcelAction = cadastreMenu->addAction(tr("Créer une parcelle"));
    setActionIcon(this, createParcelAction, QStyle::SP_FileDialogNewFolder, "list-add");
    createParcelAction->setToolTip(tr("Ajoute une parcelle cadastrale rectangulaire"));
    connect(createParcelAction, &QAction::triggered, this, [this] {
        auto command = commands::CommandRegistry::instance().createCommand(
            "cadastre.create_parcel", {});
        if (!command) {
            statusBar()->showMessage(tr("Le plugin cadastral n'est pas chargé"), 5000);
            return;
        }
        undoStack_.push(new QtCommandAdapter(document_.get(), std::move(command),
                                             tr("Créer une parcelle")));
        dirty_ = true;
        viewport_->zoomToFit();
        viewport_->update();
        statusBar()->showMessage(tr("Parcelle cadastrale créée"), 3000);
    });
    QAction* planAction = cadastreMenu->addAction(tr("Générer le plan cadastral..."));
    setActionIcon(this, planAction, QStyle::SP_DialogSaveButton, "document-print");
    planAction->setToolTip(tr("Crée un PDF cadastral à partir du document courant"));
    connect(planAction, &QAction::triggered, this, [this] {
        auto command = commands::CommandRegistry::instance().createCommand(
            "cadastre.generate_plan_sheet", {});
        if (!command) {
            statusBar()->showMessage(tr("Le plugin cadastral n'est pas chargé"), 5000);
            return;
        }
        undoStack_.push(new QtCommandAdapter(document_.get(), std::move(command),
                                             tr("Générer le plan cadastral")));
        dirty_ = true;
        statusBar()->showMessage(tr("Plan cadastral généré : plan_cadastral.pdf"), 5000);
    });
    QAction* mergeParcelsAction = cadastreMenu->addAction(
        tr("Fusionner les parcelles sélectionnées"));
    setActionIcon(this, mergeParcelsAction, QStyle::SP_FileDialogDetailedView, "object-group");
    mergeParcelsAction->setToolTip(tr("Fusionne exactement deux parcelles sélectionnées"));
    connect(mergeParcelsAction, &QAction::triggered, this, [this] {
        std::vector<std::string> ids;
        for (const auto& entity : document_->entities()) {
            if (entity->selected && entity->typeId().value == "cadastre.parcel")
                ids.push_back(std::to_string(entity->id()));
        }
        if (ids.size() != 2) {
            statusBar()->showMessage(tr("Sélectionnez exactement deux parcelles"), 4000);
            return;
        }
        auto command = commands::CommandRegistry::instance().createCommand(
            "cadastre.merge_parcels", ids);
        if (!command) {
            statusBar()->showMessage(tr("Le plugin cadastral n'est pas chargé"), 5000);
            return;
        }
        undoStack_.push(new QtCommandAdapter(document_.get(), std::move(command),
                                             tr("Fusionner les parcelles")));
        viewport_->update();
    });
    QAction* splitParcelAction = cadastreMenu->addAction(
        tr("Scinder la parcelle sélectionnée"));
    setActionIcon(this, splitParcelAction, QStyle::SP_BrowserReload, "edit-split");
    splitParcelAction->setToolTip(tr("Scinde la parcelle selon une ligne verticale médiane"));
    connect(splitParcelAction, &QAction::triggered, this, [this] {
        const geom::PolylineEntity* parcel = nullptr;
        for (const auto& entity : document_->entities()) {
            if (entity->selected && entity->typeId().value == "cadastre.parcel") {
                parcel = dynamic_cast<const geom::PolylineEntity*>(entity.get());
                break;
            }
        }
        if (!parcel || parcel->vertices().size() < 3) {
            statusBar()->showMessage(tr("Sélectionnez une parcelle valide"), 4000);
            return;
        }
        const auto box = parcel->boundingBox();
        const double x = (box.minX + box.maxX) * 0.5;
        std::vector<std::string> args{
            std::to_string(parcel->id()), std::to_string(x),
            std::to_string(box.minY - 1.0), std::to_string(x),
            std::to_string(box.maxY + 1.0)};
        auto command = commands::CommandRegistry::instance().createCommand(
            "cadastre.split_parcel", args);
        if (!command) {
            statusBar()->showMessage(tr("Le plugin cadastral n'est pas chargé"), 5000);
            return;
        }
        undoStack_.push(new QtCommandAdapter(document_.get(), std::move(command),
                                             tr("Scinder la parcelle")));
        viewport_->zoomToFit();
    });
    QAction* editBoundaryAction = cadastreMenu->addAction(
        tr("Modifier la limite de la parcelle"));
    setActionIcon(this, editBoundaryAction, QStyle::SP_FileDialogContentsView, "draw-polygon");
    editBoundaryAction->setToolTip(
        tr("Déplace un sommet de la parcelle sélectionnée"));
    connect(editBoundaryAction, &QAction::triggered, this, [this] {
        geom::PolylineEntity* parcel = nullptr;
        for (const auto& entity : document_->entities()) {
            if (!entity->selected) continue;
            if (entity->typeId().value != "cadastre.parcel") continue;
            parcel = dynamic_cast<geom::PolylineEntity*>(entity.get());
            break;
        }
        if (!parcel || parcel->vertices().size() < 3) {
            statusBar()->showMessage(tr("Sélectionnez une parcelle valide"), 4000);
            return;
        }

        bool accepted = false;
        const int index = QInputDialog::getInt(
            this, tr("Modifier la limite"),
            tr("Sommet à modifier (1 à %1) :").arg(parcel->vertices().size()),
            1, 1, static_cast<int>(parcel->vertices().size()), 1, &accepted);
        if (!accepted) return;
        const auto current = parcel->vertices()[static_cast<std::size_t>(index - 1)];
        const double x = QInputDialog::getDouble(
            this, tr("Modifier la limite"), tr("Nouvelle coordonnée X :"),
            current.x_, -1e12, 1e12, 6, &accepted);
        if (!accepted) return;
        const double y = QInputDialog::getDouble(
            this, tr("Modifier la limite"), tr("Nouvelle coordonnée Y :"),
            current.y_, -1e12, 1e12, 6, &accepted);
        if (!accepted) return;

        auto vertices = parcel->vertices();
        vertices[static_cast<std::size_t>(index - 1)] = {x, y};
        std::vector<std::string> args{std::to_string(parcel->id())};
        args.reserve(1 + vertices.size() * 2);
        for (const auto& vertex : vertices) {
            args.push_back(std::to_string(vertex.x_));
            args.push_back(std::to_string(vertex.y_));
        }
        auto command = commands::CommandRegistry::instance().createCommand(
            "cadastre.edit_parcel_boundary", args);
        if (!command) {
            statusBar()->showMessage(tr("Le plugin cadastral n'est pas chargé"), 5000);
            return;
        }
        undoStack_.push(new QtCommandAdapter(document_.get(), std::move(command),
                                             tr("Modifier la limite")));
        dirty_ = true;
        viewport_->update();
        statusBar()->showMessage(tr("Limite de parcelle modifiée"), 3000);
    });
    cadastreMenu->addSeparator();
    ribbon_->addPanel(tr("Cadastre"), tr("Parcelles"),
                      {createParcelAction, splitParcelAction, mergeParcelsAction,
                       editBoundaryAction, planAction});
    cadastreMenu->addSeparator();
    cadastreMenu->addAction(tr("Commandes disponibles"), this, [this] {
        const auto names = commands::CommandRegistry::instance().getRegisteredCommands();
        statusBar()->showMessage(
            names.empty() ? tr("Aucune commande de plugin chargée")
                          : tr("%1 commande(s) enregistrée(s)").arg(names.size()), 4000);
    });

    QMenu* helpMenu = menuBar()->addMenu(tr("&Aide"));
    helpMenu->addAction(tr("À propos de BCAD"), this, [this] {
        QMessageBox::about(this, tr("À propos de BCAD"),
                           tr("BCAD est une application de CAO 2D extensible."));
    });
}

void MainWindow::buildCommandLine() {
    commandLine_ = new QLineEdit(this);
    commandLine_->setPlaceholderText(
        tr("Point : x,y | @dx,dy | @distance<angle | Entrée pour confirmer"));
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
    dirty_ = false;
    viewport_->update();
}

QString MainWindow::resolveRecoveryPath(const QString& path) {
    QString autosavePath = autosavePathFor(path);
    QFileInfo autosaveInfo(autosavePath);
    if (!autosaveInfo.exists()) return path;
    if (autosaveInfo.lastModified() <= QFileInfo(path).lastModified()) return path;

    auto reply = QMessageBox::question(
        this, tr("Recover Autosave"),
        tr("An autosave for '%1' is newer than the file itself (crash recovery?). "
           "Load the autosave instead?")
            .arg(QFileInfo(path).fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    return reply == QMessageBox::Yes ? autosavePath : path;
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(this, tr("Open Project"), {}, tr("bcad Project (*.bcad)"));
    if (path.isEmpty()) return;

    QString loadPath = resolveRecoveryPath(path);
    if (!io::Database::load(loadPath.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Open Failed"), tr("Could not open '%1'.").arg(loadPath));
        return;
    }
    undoStack_.clear();
    // currentFilePath_ reste le vrai fichier projet même si on a chargé la
    // sauvegarde automatique, pour que Ctrl+S écrive dessus, pas sur le
    // fichier .autosave — et on garde `dirty_` à true dans ce cas pour que
    // le contenu récupéré ne soit pas perdu silencieusement.
    currentFilePath_ = path;
    dirty_ = (loadPath != path);
    viewport_->zoomToFit();
}

bool MainWindow::saveToPath(const QString& path) {
    if (!io::Database::save(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Save Failed"), tr("Could not save to '%1'.").arg(path));
        return false;
    }
    currentFilePath_ = path;
    dirty_ = false;
    // Le fichier réel étant maintenant à jour, une sauvegarde automatique
    // plus ancienne n'a plus lieu d'être proposée à la prochaine ouverture.
    QFile::remove(autosavePathFor(path));
    return true;
}

void MainWindow::onAutosaveTimeout() {
    if (currentFilePath_.isEmpty() || !dirty_) return;
    io::Database::save(autosavePathFor(currentFilePath_).toStdString(), *document_);
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

void MainWindow::onExportGeoJson() {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Exporter GeoJSON"), {}, tr("GeoJSON (*.geojson *.json)"));
        if (path.isEmpty()) return;
        std::ofstream output(path.toStdString(), std::ios::binary);
        if (!output) {
            QMessageBox::warning(this, tr("Export impossible"),
                                 tr("Impossible d'écrire « %1 ».").arg(path));
            return;
        }

        output << "{\"type\":\"FeatureCollection\",\"features\":[";
        bool first = true;
        output << std::setprecision(17);
        for (const auto& entity : document_->entities()) {
            const auto* point = dynamic_cast<const geom::PointEntity*>(entity.get());
            const auto* polyline = dynamic_cast<const geom::PolylineEntity*>(entity.get());
            if (!point && !polyline) continue;
            if (!first) output << ',';
            first = false;
            output << "{\"type\":\"Feature\",\"properties\":{\"id\":" << entity->id()
                    << ",\"layer\":\"";
            for (const char c : entity->layer()) {
                if (c == '"' || c == '\\') output << '\\';
                output << c;
            }
            output << "\"},\"geometry\":";
            if (point) {
                output << "{\"type\":\"Point\",\"coordinates\":["
                       << point->position().x_ << ',' << point->position().y_ << "]}";
            } else {
                const auto& vertices = polyline->vertices();
                output << "{\"type\":\"" << (polyline->closed() ? "Polygon" : "LineString")
                       << "\",\"coordinates\":";
                if (polyline->closed()) output << '[';
                output << '[';
                for (std::size_t i = 0; i < vertices.size(); ++i) {
                    if (i) output << ',';
                    output << '[' << vertices[i].x_ << ',' << vertices[i].y_ << ']';
                }
                if (polyline->closed() && !vertices.empty())
                    output << ",[" << vertices.front().x_ << ',' << vertices.front().y_ << ']';
                output << ']';
                if (polyline->closed()) output << ']';
                output << '}';
            }
            output << '}';
        }
        output << "]}\n";
        if (!output) {
            QMessageBox::warning(this, tr("Export impossible"),
                                 tr("Une erreur est survenue pendant l'écriture de « %1 ».").arg(path));
            return;
        }
        statusBar()->showMessage(tr("GeoJSON exporté : %1").arg(path), 4000);
    }

void MainWindow::onExportCsv() {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Exporter les coordonnées CSV"), {}, tr("CSV (*.csv)"));
        if (path.isEmpty()) return;
        std::ofstream output(path.toStdString(), std::ios::binary);
        if (!output) {
            QMessageBox::warning(this, tr("Export impossible"),
                                 tr("Impossible d'écrire « %1 ».").arg(path));
            return;
        }
        output << "entity_id,vertex_index,x,y\n" << std::setprecision(17);
        for (const auto& entity : document_->entities()) {
            std::vector<geom::Point2> points;
            if (const auto* point = dynamic_cast<const geom::PointEntity*>(entity.get())) {
                points.push_back(point->position());
            } else {
                points = entity->tessellate(0.01);
            }
            for (std::size_t i = 0; i < points.size(); ++i)
                output << entity->id() << ',' << i << ',' << points[i].x_ << ',' << points[i].y_ << '\n';
        }
        if (!output) {
            QMessageBox::warning(this, tr("Export impossible"),
                                 tr("Une erreur est survenue pendant l'écriture de « %1 ».").arg(path));
            return;
        }
        statusBar()->showMessage(tr("Coordonnées CSV exportées : %1").arg(path), 4000);
}

void MainWindow::onPrintPreview() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter* printer) {
        // Même logique que PdfExport::exportPdf mais en temps réel
        layout::Sheet sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        layout::Viewport vp;
        // Calculer la bounding box du document
        geom::BoundingBox bbox;
        for (const auto& e : document_->entities()) {
            bbox.expand(e->boundingBox());
        }
        if (!bbox.isValid()) return;
        vp.setSource(bbox);
        // Auto-échelle
        vp.setScale(vp.autoScale(sheet));
        vp.setPosition(sheet.margins().left, sheet.margins().top);

        layout::Cartouche cartouche;
        cartouche.commune = "Commune";
        cartouche.section = "A";
        cartouche.echelle = "1:" + std::to_string(static_cast<int>(vp.scale()));
        cartouche.heightMm = 25.0;

        // Dessin direct sur le QPrinter via QPainter
        QPainter painter(printer);
        if (!painter.isActive()) return;

        // Configurer la page
        QPageLayout layout(QPageSize(QPageSize::A3),
            QPageLayout::Landscape,
            QMarginsF(sheet.margins().left, sheet.margins().top,
                      sheet.margins().right, sheet.margins().bottom),
            QPageLayout::Millimeter);
        printer->setPageLayout(layout);

        // Dessiner le cartouche (grille label+valeur partagée avec l'export PDF)
        QRectF pageRect = printer->pageRect(QPrinter::Millimeter);
        QRectF cartoucheRect(0, pageRect.height() - cartouche.heightMm,
                             pageRect.width(), cartouche.heightMm);
        layout::drawCartouche(painter, cartoucheRect, cartouche);

        // Dessiner les entités via le viewport
        // Calculer la transformation pour mapper la zone du document sur la zone imprimable
        double scale = vp.scale();
        bcad::geom::Point2 srcMin{vp.source().minX, vp.source().minY};
        bcad::geom::Point2 srcMax{vp.source().maxX, vp.source().maxY};
        double printableWidth = sheet.printableWidth();
        double printableHeight = sheet.printableHeight();

        // Translation + scale
        painter.save();
        painter.translate(sheet.margins().left, sheet.margins().top + printableHeight);
        painter.scale(1000.0 / scale, -1000.0 / scale); // m -> mm, inversion Y
        painter.translate(-srcMin.x_, -srcMax.y_);

        // Dessiner chaque entité
        for (const auto& e : document_->entities()) {
            QPen pen(Qt::black);
            QBrush brush(Qt::NoBrush);
            if (e->colorOverride()) {
                pen.setColor(QColor::fromRgbF(e->colorOverride()->r, e->colorOverride()->g, e->colorOverride()->b));
            }
            painter.setPen(pen);
            painter.setBrush(brush);

            // Utiliser la tessellation pour dessiner
            auto tess = e->tessellate(1.0);
            if (tess.size() >= 3 && e->typeId() != geom::TypeId_Line &&
                e->typeId() != geom::TypeId_Point) {
                QPolygonF poly;
                for (const auto& pt : tess) {
                    poly << QPointF(pt.x_, pt.y_);
                }
                painter.drawPolygon(poly);
            } else if (tess.size() >= 2) {
                QPolygonF polyline;
                for (const auto& pt : tess) {
                    polyline << QPointF(pt.x_, pt.y_);
                }
                painter.drawPolyline(polyline);
            }
        }
        painter.restore();
    });
    preview.exec();
}

void MainWindow::onCursorMoved(double x, double y) {
    coordLabel_->setText(tr("X : %1  Y : %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
}

void MainWindow::onToolChanged(ToolMode mode) {
    switch (mode) {
        case ToolMode::Select: toolLabel_->setText(tr("Sélection")); break;
        case ToolMode::Move: toolLabel_->setText(tr("Déplacer")); break;
        case ToolMode::Copy: toolLabel_->setText(tr("Copier")); break;
        case ToolMode::Rotate: toolLabel_->setText(tr("Tourner")); break;
        case ToolMode::Scale: toolLabel_->setText(tr("Échelle")); break;
        case ToolMode::Mirror: toolLabel_->setText(tr("Symétrie")); break;
        case ToolMode::Trim: toolLabel_->setText(tr("Rogner")); break;
        case ToolMode::Extend: toolLabel_->setText(tr("Prolonger")); break;
        case ToolMode::Break: toolLabel_->setText(tr("Scinder")); break;
        case ToolMode::Line: toolLabel_->setText(tr("Ligne")); break;
        case ToolMode::Circle: toolLabel_->setText(tr("Cercle")); break;
        case ToolMode::Arc: toolLabel_->setText(tr("Arc")); break;
        case ToolMode::Polyline: toolLabel_->setText(tr("Polyligne")); break;
        case ToolMode::Rectangle: toolLabel_->setText(tr("Rectangle")); break;
        case ToolMode::Point: toolLabel_->setText(tr("Point")); break;
        case ToolMode::DimensionLinear: toolLabel_->setText(tr("Cotation linéaire")); break;
        case ToolMode::DimensionAligned: toolLabel_->setText(tr("Cotation alignée")); break;
        case ToolMode::DimensionAngular: toolLabel_->setText(tr("Cotation angulaire")); break;
        case ToolMode::DimensionRadius: toolLabel_->setText(tr("Cotation de rayon")); break;
        case ToolMode::DimensionDiameter: toolLabel_->setText(tr("Cotation de diamètre")); break;
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
