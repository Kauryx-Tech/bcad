// La fenetre elle-meme : ce qu'elle possede et comment elle le monte.
//
// Classe repartie sur cinq unites de traduction par responsabilite, sans
// changement de comportement — l'ordre des menus et des panneaux du ruban vient
// de la sequence d'appels du constructeur, pas de la repartition des methodes.
// Cette liste est la seule copie : l'en-tete src/app/MainWindow.h n'en reprend
// que le principe, une table recopiee a deux endroits derive.
//   MainWindow.cpp            constructeur, docks, ligne de commande, theme
//   MainWindowTools.cpp       les outils interactifs et leur table
//   MainWindowMenus.cpp       les menus de l'hote et le ruban
//   MainWindowPlugins.cpp     workbenches, validateurs, exporteurs des modules
//   MainWindowDocument.cpp    document, fichiers, autosave, impression
// Les outils sont construits avant les menus parce que ces derniers ne font que
// référencer les memes objets QAction.

#include "MainWindow.h"

#include "LayerPanel.h"
#include "PropertiesPanel.h"
#include "RibbonBar.h"
#include "bcad/io/Exporters.h"
#include "bcad/plugin/Plugin.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <memory>

namespace bcad::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    document_ = std::make_unique<core::Document>();
    undoStack_.setUndoLimit(100);

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

    buildToolActions();
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
    connect(&undoStack_, &QUndoStack::indexChanged, this, [this] {
        dirty_ = true;
        updateWindowTitle();
    });

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

    // Les donnees livrees avec un module (gabarits de profil, styles) suivent le
    // meme principe : l'hote propose des emplacements generiques — ceux de
    // l'installation (bin/../share) et ceux de l'arbre de build
    // (src/app/../../share) — et le module y cherche le fichier qu'il nomme
    // lui-meme. Aucun chemin d'installation n'est ecrit dans un module.
    for (const QString& directory : {appDir + "/../share/bcad/plugins",
                                     appDir + "/../../share/bcad/plugins",
                                     xdgData + "/bcad/plugins"}) {
        plugin::pluginManager().addDataDirectory(directory.toStdString());
    }

    const auto loadedPlugins = plugin::pluginManager().loadAllDiscovered();
    if (!loadedPlugins.empty()) {
        statusBar()->showMessage(tr("%1 plugin(s) chargé(s)").arg(loadedPlugins.size()), 5000);
    }

    // Calques déclarés par les modules : chaque IStyleProvider liste les calques
    // qu'il gère ; l'hôte les crée dans le document au démarrage (§7.1).
    applyStyleProvidersToDocument(*document_);

    // Les formats d'echange du noyau tombent dans le meme registre que ceux
    // d'un module metier : un seul menu, aucune action codée à la main.
    io::initializeNativeFileExporters();

    // Les workbenches des plugins sont connus qu'apres leur chargement.
    buildPluginMenus();

    updateWindowTitle();
    resize(1280, 800);

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

    // Le menu « Calque » ne porte que la bascule des calques ; les autres
    // panneaux sont des vues et vont dans « Affichage ». Ils etaient tous les
    // trois dans « Calque », et « Outils »/« Aide » etaient recrees ici alors
    // qu'ils existent deja : deux menus du meme nom dans la barre.
    layerMenu_->addAction(layersDock->toggleViewAction());
    viewMenu_->addSeparator();
    viewMenu_->addAction(propertiesDock->toggleViewAction());
    viewMenu_->addAction(validationDock->toggleViewAction());

    // Les menus et rubans metiers ne sont pas ecrits ici : ils sont declares
    // par les plugins via leurs workbenches (voir buildPluginMenus()).
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
    // Applique a l'application, pas a la fenetre : les boites de dialogue
    // fichier, message ou saisie sont creees hors de la hierarchie de cette
    // fenetre et heritaient donc du theme clair du systeme.
    qApp->setStyleSheet(R"(
        QMainWindow, QDialog, QDockWidget, QMenuBar, QMenu, QStatusBar { background-color: #2b2d31; color: #e0e0e0; }
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

void MainWindow::onCursorMoved(double x, double y) {
    coordLabel_->setText(tr("X : %1  Y : %2").arg(x, 0, 'f', 3).arg(y, 0, 'f', 3));
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
