// Le ruban dans la fenetre : sa rangee pleine largeur au-dessus des palettes,
// comme AutoCAD, et les widgets qu'il porte (liste des calques, L-02). Le
// contenu des onglets se declare dans MainWindowMenus.cpp.

#include "MainWindow.h"

#include "LayerCombo.h"
#include "LayerPanel.h"
#include "RibbonBar.h"

#include <QAction>
#include <QToolBar>
#include <QWidgetAction>

namespace bcad::app {

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

} // namespace bcad::app
