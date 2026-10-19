// Menu du clic droit au repos, comme le menu contextuel par defaut
// d'AutoCAD. Il est fait des actions deja construites (outils, historique,
// zooms) : memes libelles et icones que le ruban, rien de redeclare.
// Pendant une commande, le clic droit reste « Entree » (Viewport).

#include "MainWindow.h"

#include "Viewport.h"

#include <QAction>
#include <QDockWidget>
#include <QMenu>

namespace bcad::app {

void MainWindow::showCanvasMenu(const QPoint& globalPos) {
    QMenu menu(this);
    // Repeter la derniere commande, et les recentes.
    if (QAction* last = toolAction(viewport_->lastCommand()); last && viewport_->lastCommand() != ToolMode::Select) {
        menu.addAction(last->icon(), tr("Répéter %1").arg(last->toolTip().remove(QLatin1Char('&'))),
                       last, &QAction::trigger);
    }
    if (viewport_->recentCommands().size() > 1) {
        QMenu* recent = menu.addMenu(tr("Entrée récente"));
        for (ToolMode mode : viewport_->recentCommands())
            if (QAction* action = toolAction(mode)) recent->addAction(action);
    }
    menu.addSeparator();
    if (viewport_->hasSelection()) {
        // Avec une selection : ce qu'AutoCAD propose sur des objets designes.
        menu.addAction(deleteAction_);
        for (ToolMode mode : {ToolMode::Move, ToolMode::Copy, ToolMode::Scale, ToolMode::Rotate})
            if (QAction* action = toolAction(mode)) menu.addAction(action);
        menu.addSeparator();
        menu.addAction(tr("Désélectionner tout"), viewport_, &Viewport::deselectAll);
        menu.addAction(tr("Propriétés"), this, [this] {
            propertiesDock_->show();
            propertiesDock_->raise();
        });
    } else {
        menu.addAction(undoAction_);
        menu.addAction(redoAction_);
        menu.addSeparator();
        if (QAction* pan = toolAction(ToolMode::Pan)) menu.addAction(pan);
        QMenu* zoom = menu.addMenu(tr("Zoom"));
        zoom->addAction(zoomFitAction_);
        if (QAction* window = toolAction(ToolMode::ZoomWindow)) zoom->addAction(window);
        zoom->addAction(zoomPrevAction_);
        menu.addSeparator();
        menu.addAction(selectAllAction_);
    }
    menu.exec(globalPos);
}

} // namespace bcad::app
