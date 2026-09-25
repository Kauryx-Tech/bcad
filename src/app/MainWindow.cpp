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
#include "bcad/io/Exporters.h"
#include "bcad/layout/Cartouche.h"
#include "bcad/layout/PdfExport.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include "bcad/plugin/FileExporter.h"
#include "bcad/plugin/Plugin.h"
#include "bcad/plugin/Validator.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/app/QtCommandAdapter.h"
#include <QAction>
#include <QActionGroup>
#include <QColor>
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
#include <QTreeWidget>
#include <QVariant>
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

    // Les modules metiers sont decouverts, pas nommes : l'hote fournit des
    // repertoires generiques (install, arbre de build, donnees utilisateur) et
    // le manager y prend tout module valide (ADR-016 principe 4).
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString xdgData = qEnvironmentVariable("XDG_DATA_HOME",
                                                 QDir::homePath() + "/.local/share");
    for (const QString& directory : {appDir + "/../lib/bcad/plugins",
                                     appDir + "/../plugins",
                                     appDir + "/../../plugins",
                                     xdgData + "/bcad/plugins"}) {
        plugin::pluginManager().addSearchDirectory(directory.toStdString());
    }

    const auto loadedPlugins = plugin::pluginManager().loadAllDiscovered();
    if (!loadedPlugins.empty()) {
        statusBar()->showMessage(tr("%1 plugin(s) chargé(s)").arg(loadedPlugins.size()), 5000);
    }

    // Les formats d'echange du noyau tombent dans le meme registre que ceux
    // d'un module metier : un seul menu, aucune action codée à la main.
    io::initializeNativeFileExporters();

    // Les workbenches des plugins sont connus qu'apres leur chargement.
    buildPluginMenus();

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
    // Le contenu est dresse depuis le registre des exporteurs une fois les
    // plugins charges (rebuildExportMenu) : l'hote ne nomme aucun format.
    exportMenu_ = fileMenu->addMenu(tr("&Exporter"));
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

// Rassemble les arguments d'une action de workbench selon une strategie
// generique : l'hote ne connait ni le nom ni la signification des entites, il
// filtre la selection sur les TypeIds que le plugin a declares.
namespace {

bool typeMatches(const geom::Entity& entity,
                 const std::vector<std::string>& acceptedTypes) {
    if (acceptedTypes.empty()) return true;
    for (const auto& type : acceptedTypes) {
        if (entity.typeId().value == type) return true;
    }
    return false;
}

} // namespace

void MainWindow::executeWorkbenchAction(plugin::WorkbenchAction action) {
    std::vector<geom::Entity*> selected;
    for (const auto& entity : document_->entities()) {
        if (entity->selected && typeMatches(*entity, action.selectedTypes))
            selected.push_back(entity.get());
    }
    const int count = static_cast<int>(selected.size());
    if (count < action.minSelected ||
        (action.maxSelected > 0 && count > action.maxSelected)) {
        statusBar()->showMessage(tr("Sélection non valable pour « %1 »")
                                     .arg(QString::fromStdString(action.label)), 4000);
        return;
    }

    std::vector<std::string> args;
    auto* polyline = selected.empty()
                         ? nullptr
                         : dynamic_cast<geom::PolylineEntity*>(selected.front());

    switch (action.params) {
    case plugin::WorkbenchParams::None:
        break;
    case plugin::WorkbenchParams::SelectionIds:
        for (auto* entity : selected)
            args.push_back(std::to_string(entity->id()));
        break;
    case plugin::WorkbenchParams::BoxSplit: {
        if (!polyline || polyline->vertices().size() < 3) {
            statusBar()->showMessage(tr("Sélection non valable"), 4000);
            return;
        }
        const auto box = polyline->boundingBox();
        const double x = (box.minX + box.maxX) * 0.5;
        args = {std::to_string(polyline->id()), std::to_string(x),
                std::to_string(box.minY - 1.0), std::to_string(x),
                std::to_string(box.maxY + 1.0)};
        break;
    }
    case plugin::WorkbenchParams::RunValidators: {
        // Aucune commande : l'hote execute les validateurs des plugins.
        // Perimetre = la selection si elle existe, sinon tout le document.
        std::vector<geom::Entity*> scope = selected;
        if (scope.empty()) {
            for (const auto& entity : document_->entities()) {
                if (typeMatches(*entity, action.selectedTypes))
                    scope.push_back(entity.get());
            }
        }
        runValidation(scope);
        return;
    }
    case plugin::WorkbenchParams::Vertices: {
        if (!polyline || polyline->vertices().size() < 3) {
            statusBar()->showMessage(tr("Sélection non valable"), 4000);
            return;
        }
        bool accepted = false;
        const int index = QInputDialog::getInt(
            this, tr("Modifier un sommet"),
            tr("Sommet à modifier (1 à %1) :").arg(polyline->vertices().size()),
            1, 1, static_cast<int>(polyline->vertices().size()), 1, &accepted);
        if (!accepted) return;
        const auto current = polyline->vertices()[static_cast<size_t>(index - 1)];
        const double x = QInputDialog::getDouble(
            this, tr("Modifier un sommet"), tr("Nouvelle coordonnée X :"),
            current.x_, -1e12, 1e12, 6, &accepted);
        if (!accepted) return;
        const double y = QInputDialog::getDouble(
            this, tr("Modifier un sommet"), tr("Nouvelle coordonnée Y :"),
            current.y_, -1e12, 1e12, 6, &accepted);
        if (!accepted) return;
        auto vertices = polyline->vertices();
        vertices[static_cast<size_t>(index - 1)] = {x, y};
        args.push_back(std::to_string(polyline->id()));
        for (const auto& vertex : vertices) {
            args.push_back(std::to_string(vertex.x_));
            args.push_back(std::to_string(vertex.y_));
        }
        break;
    }
    }

    auto command = commands::CommandRegistry::instance().createCommand(
        action.commandName, args);
    if (!command) {
        statusBar()->showMessage(
            tr("Commande indisponible : vérifiez que le plugin est chargé"), 5000);
        return;
    }
    const QString label = QString::fromStdString(action.label);
    if (action.modal) {
        command->execute(*document_);
    } else {
        undoStack_.push(new QtCommandAdapter(document_.get(), std::move(command), label));
    }
    dirty_ = true;
    viewport_->update();
}

// Execute les validateurs enregistres par les plugins. L'hote ne decide d'aucune
// regle : il filtre les entites selon les TypeIds que chaque validateur declare,
// et affiche les diagnostics tels qu'ils sont ecrits par le plugin.
void MainWindow::runValidation(std::vector<geom::Entity*> scope) {
    const auto validators = plugin::ValidatorRegistry::instance().validators();
    validationTree_->clear();
    if (validators.empty()) {
        statusBar()->showMessage(tr("Aucun validateur enregistré : chargez un module métier"), 5000);
        return;
    }

    int errors = 0;
    int warnings = 0;
    int notes = 0;
    for (const auto* validator : validators) {
        std::vector<geom::Entity*> relevant;
        for (auto* entity : scope) {
            if (typeMatches(*entity, validator->applicableTypes()))
                relevant.push_back(entity);
        }
        if (relevant.empty()) continue;

        const auto diagnostics = validator->validate(relevant);
        if (diagnostics.empty()) continue;

        auto* group = new QTreeWidgetItem(validationTree_);
        group->setText(0, QString::fromStdString(validator->label()));
        group->setText(1, tr("%1 constat(s)").arg(diagnostics.size()));
        for (const auto& diagnostic : diagnostics) {
            QString severity;
            switch (diagnostic.severity) {
            case validation::Severity::Error:
                severity = tr("Erreur");
                ++errors;
                break;
            case validation::Severity::Warning:
                severity = tr("Avertissement");
                ++warnings;
                break;
            case validation::Severity::Info:
                severity = tr("Info");
                ++notes;
                break;
            }
            auto* item = new QTreeWidgetItem(group);
            item->setText(0, severity);
            item->setText(1, QString::fromStdString(diagnostic.message));
            QVariantList ids;
            for (const int id : diagnostic.entityIds) ids.push_back(id);
            item->setData(0, Qt::UserRole, ids);
            item->setForeground(0, QColor(
                diagnostic.severity == validation::Severity::Error ? "#e06c6c"
                : diagnostic.severity == validation::Severity::Warning ? "#e0b060"
                                                                       : "#9aa0a6"));
        }
    }

    validationTree_->expandAll();
    // Le dock peut avoir ete ferme par l'utilisateur : sans lui, les resultats
    // seraient produits et jetes.
    if (validationDock_) {
        validationDock_->setVisible(true);
        validationDock_->raise();
    }
    statusBar()->showMessage(tr("%1 erreur(s), %2 avertissement(s), %3 info(s) sur %4 entité(s) vérifiée(s)")
                                 .arg(errors).arg(warnings).arg(notes)
                                 .arg(scope.size()), 8000);
}

void MainWindow::buildPluginMenus() {
    rebuildExportMenu();
    auto& registry = plugin::WorkbenchRegistry::instance();
    for (const auto* workbench : registry.workbenches()) {
        QMenu* menu = menuBar()->addMenu("&" + QString::fromStdString(workbench->label()));
        const QString description = QString::fromStdString(workbench->description());
        if (!description.isEmpty()) menu->setToolTipsVisible(true);
        QList<QAction*> ribbonActions;
        for (const auto& panel : workbench->panels()) {
            if (!panel.title.empty())
                menu->addSection(QString::fromStdString(panel.title));
            QList<QAction*> panelActions;
            for (const auto& action : panel.actions) {
                QAction* item = menu->addAction(QString::fromStdString(action.label), this,
                    [this, action] { executeWorkbenchAction(action); });
                if (!action.tooltip.empty())
                    item->setToolTip(QString::fromStdString(action.tooltip));
                panelActions.push_back(item);
                ribbonActions.push_back(item);
            }
            if (!panel.title.empty())
                ribbon_->addPanel(QString::fromStdString(workbench->label()),
                                  QString::fromStdString(panel.title), panelActions);
        }
        menu->addSeparator();
        menu->addAction(tr("Commandes disponibles"), this, [this] {
            const auto names = commands::CommandRegistry::instance().getRegisteredCommands();
            statusBar()->showMessage(
                names.empty() ? tr("Aucune commande de plugin chargée")
                              : tr("%1 commande(s) enregistrée(s)").arg(names.size()), 4000);
        });
    }
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

    // Resultats des validateurs declares par les modules metiers. L'hote ne
    // contient aucune regle : il n'affiche que les diagnostics produits (ADR-016).
    validationDock_ = new QDockWidget(tr("Vérifications"), this);
    auto* validationDock = validationDock_;
    validationDock->setObjectName("validationDock");
    validationTree_ = new QTreeWidget(validationDock);
    validationTree_->setObjectName("validationTree");
    validationTree_->setColumnCount(2);
    validationTree_->setHeaderLabels({tr("Issue"), tr("Message")});
    validationTree_->setColumnWidth(0, 120);
    // Double-clic : atteint les entites visees par le constat.
    connect(validationTree_, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int) {
        if (!item) return;
        const QVariantList raw = item->data(0, Qt::UserRole).toList();
        if (raw.isEmpty()) return;
        QList<int> ids;
        for (const auto& value : raw) ids.push_back(value.toInt());
        for (const auto& entity : document_->entities()) {
            entity->selected = ids.contains(entity->id());
        }
        propertiesPanel_->refresh();
        viewport_->update();
    });
    validationDock->setWidget(validationTree_);
    addDockWidget(Qt::RightDockWidgetArea, validationDock);
    tabifyDockWidget(propertiesDock, validationDock);
    layersDock->raise();

    ribbon_->addPanel(tr("Accueil"), tr("Panneaux"),
                       { layersDock->toggleViewAction(), propertiesDock->toggleViewAction(),
                         validationDock->toggleViewAction() });

    layerMenu_->addAction(layersDock->toggleViewAction());
    layerMenu_->addAction(propertiesDock->toggleViewAction());
    layerMenu_->addAction(validationDock->toggleViewAction());

    QMenu* toolsMenu = menuBar()->addMenu(tr("&Outils"));
    toolsMenu->addAction(tr("Aperçu avant impression..."), this, &MainWindow::onPrintPreview);

    // Les menus et rubans metiers ne sont pas ecrits ici : ils sont declares
    // par les plugins via leurs workbenches (voir buildPluginMenus()).

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

// Le menu « Exporter » n'énumère aucun format : il dresse la liste des
// exporteurs enregistres, y compris ceux d'un module metier charge. Libelle et
// extension viennent de leur declarant (ADR-016).
void MainWindow::rebuildExportMenu() {
    if (!exportMenu_) return;
    exportMenu_->clear();
    const auto exporters = plugin::FileExporterRegistry::instance().exporters();
    if (exporters.empty()) {
        exportMenu_->addAction(tr("Aucun format disponible"))->setEnabled(false);
        return;
    }
    for (const auto* exporter : exporters) {
        const std::string id = exporter->id();
        exportMenu_->addAction(tr("&%1...").arg(QString::fromStdString(exporter->label())),
                               this, [this, id] { runFileExporter(id); });
    }
}

void MainWindow::runFileExporter(const std::string& id) {
    const auto* exporter = plugin::FileExporterRegistry::instance().find(id);
    if (!exporter) return;
    const QString label = QString::fromStdString(exporter->label());
    const QString filter = tr("%1 (*.%2)").arg(label,
                                               QString::fromStdString(exporter->extension()));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exporter %1").arg(label), {}, filter);
    if (path.isEmpty()) return;

    std::string error;
    if (!exporter->writeDocument(*document_, path.toStdString(), &error)) {
        QMessageBox::warning(this, tr("Export impossible"), QString::fromStdString(error));
        return;
    }
    statusBar()->showMessage(tr("%1 exporté : %2").arg(label, path), 4000);
}

void MainWindow::onPrintPreview() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter* printer) {
        // L'aperçu EST la feuille exportée : même composition, même peintre, aucun
        // code de dessin ici. Le cartouche reste vide tant que l'hôte ne connaît pas
        // de métadonnées projet — c'est le plugin qui les porte.
        layout::PdfExportOptions options;
        options.sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        options.viewport.setSource(document_->extents());
        if (!options.viewport.source().isValid()) return;
        options.document = document_.get();
        layout::applySuggestedScale(options);
        layout::applyPageLayout(printer, options.sheet);

        QPainter painter(printer);
        if (!painter.isActive()) return;
        layout::drawSheet(painter, options);
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
