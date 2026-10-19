// Presentation de la fenetre : la rangee pleine largeur du ruban au-dessus des
// palettes, comme AutoCAD, les widgets qu'il porte (liste des calques, L-02),
// et l'echelle de l'interface. Le contenu des onglets se declare dans
// MainWindowMenus.cpp.

#include "MainWindow.h"

#include "ActionIcons.h"
#include "LayerCombo.h"
#include "LayerPanel.h"
#include "RibbonBar.h"
#include "Viewport.h"
#include "UiScale.h"
#include "theme/Theme.h"

#include <QAction>
#include <QStyle>
#include <QFrame>
#include <QIcon>
#include <QToolButton>
#include <QMenuBar>
#include <QActionGroup>
#include <QMenu>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QWidgetAction>

#include <cmath>
#include <functional>
#include <utility>

namespace bcad::app {

void MainWindow::installQuickAccess(const QList<QAction*>& fileActions,
                                    const QList<QAction*>& historyActions) {
    auto* quickToolbar = addToolBar(tr("Accès rapide"));
    quickToolbar->setObjectName("quickAccessToolbar");
    quickToolbar->setMovable(false);
    quickToolbar->setIconSize(QSize(theme::kIconQuickAccess, theme::kIconQuickAccess));
    quickToolbar->addActions(fileActions);
    quickToolbar->addSeparator();
    quickToolbar->addActions(historyActions);

    // Bouton ▾ de personnalisation, comme AutoCAD : la barre des menus est
    // masquee par defaut (MENUBAR = 0) et se reaffiche d'ici ; le choix est
    // retenu. Ses raccourcis restent actifs quand elle est cachee : chaque
    // action des menus est aussi attachee a la fenetre.
    std::function<void(QMenu*)> attach = [&](QMenu* menu) {
        for (QAction* action : menu->actions()) {
            if (action->menu()) attach(action->menu());
            else if (!action->shortcut().isEmpty()) addAction(action);
        }
    };
    for (QAction* top : menuBar()->actions())
        if (top->menu()) attach(top->menu());
    const bool showMenuBar = QSettings().value(QStringLiteral("interface/menuBar"), false).toBool();
    menuBar()->setVisible(showMenuBar);
    auto* customize = new QToolButton(quickToolbar);
    customize->setObjectName("quickAccessCustomize");
    customize->setIcon(QIcon(QStringLiteral(":/icons/chevron-down.svg")));
    customize->setPopupMode(QToolButton::InstantPopup);
    customize->setToolTip(tr("Personnaliser la barre d'outils Accès rapide"));
    auto* menu = new QMenu(customize);
    QAction* toggle = menu->addAction(tr("Afficher la barre des menus"));
    toggle->setCheckable(true);
    toggle->setChecked(showMenuBar);
    connect(toggle, &QAction::toggled, this, [this](bool on) {
        menuBar()->setVisible(on);
        QSettings().setValue(QStringLiteral("interface/menuBar"), on);
    });
    customize->setMenu(menu);
    quickToolbar->addWidget(customize);
}

void MainWindow::installRibbonRow() {
    addToolBarBreak(Qt::TopToolBarArea);
    auto* ribbonToolbar = addToolBar(tr("Ruban"));
    ribbonToolbar->setObjectName("ribbonToolbar");
    ribbonToolbar->setMovable(false);
    ribbonToolbar->setFloatable(false);
    ribbonToolbar->toggleViewAction()->setVisible(false);
    ribbon_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ribbonToolbar->addWidget(ribbon_);
}

QAction* MainWindow::layerComboAction() {
    layerCombo_ = new LayerCombo(this);
    layerCombo_->setDocument(document_);
    connect(layerPanel_, &LayerPanel::layersChanged, layerCombo_, &LayerCombo::refresh);
    auto* action = new QWidgetAction(this);
    action->setText(tr("Calque courant"));
    action->setDefaultWidget(layerCombo_);
    return action;
}

void MainWindow::addUiScaleMenu(QMenu* viewMenu) {
    QMenu* menu = viewMenu->addMenu(tr("É&chelle de l'interface"));
    auto* group = new QActionGroup(menu);
    const double current = QSettings().value(QString::fromLatin1(kUiScaleSetting), 0.0).toDouble();
    const std::pair<double, QString> choices[] = {
        {0.0, tr("Automatique (échelle de l'affichage)")}, {1.0, QStringLiteral("100 %")},
        {1.25, QStringLiteral("125 %")}, {1.5, QStringLiteral("150 %")},
        {1.75, QStringLiteral("175 %")}, {2.0, QStringLiteral("200 %")}};
    for (const auto& [scale, label] : choices) {
        QAction* action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(std::abs(current - scale) < 0.01);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, scale = scale] {
            QSettings().setValue(QString::fromLatin1(kUiScaleSetting), scale);
            statusBar()->showMessage(tr("Échelle de l'interface : prise en compte au prochain démarrage de BCAD."),
                                     8000);
        });
    }
}

void MainWindow::addStatusBarButton(QAction* action) {
    auto* button = new QToolButton(statusBar());
    button->setDefaultAction(action);
    button->setIconSize(QSize(theme::kIconStatusBar, theme::kIconStatusBar));
    button->setAutoRaise(true);
    statusBar()->addPermanentWidget(button);
}

void MainWindow::addStatusBarSeparator() {
    auto* line = new QFrame(statusBar());
    line->setFrameShape(QFrame::VLine);
    line->setObjectName("statusSeparator");
    statusBar()->addPermanentWidget(line);
}

std::pair<QAction*, QAction*> MainWindow::displayToggleActions(QMenu* viewMenu) {
    QAction* lineWeight =
        viewMenu->addAction(tr("Afficher les é&paisseurs de ligne"), viewport_, &Viewport::toggleLineWeights);
    setActionIcon(this, lineWeight, QStyle::SP_FileDialogDetailedView, QStringLiteral("lineweight"));
    QAction* dynamic = viewMenu->addAction(tr("Saisie &dynamique"), viewport_, &Viewport::toggleDynamicInput);
    dynamic->setShortcut(Qt::Key_F12);
    setActionIcon(this, dynamic, QStyle::SP_FileDialogContentsView, QStringLiteral("dynamic-input"));
    return {lineWeight, dynamic};
}

} // namespace bcad::app
