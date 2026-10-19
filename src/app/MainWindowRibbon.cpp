// Presentation de la fenetre : la rangee pleine largeur du ruban au-dessus des
// palettes, comme AutoCAD, les widgets qu'il porte (liste des calques, L-02),
// et l'echelle de l'interface. Le contenu des onglets se declare dans
// MainWindowMenus.cpp.

#include "MainWindow.h"

#include "LayerCombo.h"
#include "LayerPanel.h"
#include "RibbonBar.h"
#include "UiScale.h"
#include "theme/Theme.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QWidgetAction>

#include <cmath>
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

} // namespace bcad::app
