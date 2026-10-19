// Boite Options (S-15) : reglages de l'utilisateur, appliques tout de suite et
// gardes d'une session a l'autre (QSettings, jamais le dessin). Relus au
// demarrage. applyOptions est le seul endroit qui les distribue : un reglage
// nouveau y ajoute son effet (voir detail/UserOptions.h).

#include "window/MainWindow.h"

#include "viewport/Viewport.h"
#include "widgets/OptionsDialog.h"

#include <QAction>
#include <QMenu>

namespace bcad::app {

void MainWindow::installOptions(QMenu* applicationMenu, QMenu* toolsMenu) {
    applyOptions(loadUserOptions());
    optionsAction_ = new QAction(tr("&Options..."), this);
    optionsAction_->setToolTip(tr("Options (OP) : réticule, cible de sélection"));
    connect(optionsAction_, &QAction::triggered, this, &MainWindow::showOptions);
    connect(viewport_, &Viewport::optionsRequested, this, &MainWindow::showOptions);
    // Comme AutoCAD : en bas du menu de l'application, apres un separateur.
    applicationMenu->addSeparator();
    applicationMenu->addAction(optionsAction_);
    toolsMenu->addSeparator();
    toolsMenu->addAction(optionsAction_);
}

void MainWindow::applyOptions(const detail::UserOptions& options) {
    options_ = options;
    viewport_->setCursorStyle(options.cursor);
}

void MainWindow::showOptions() {
    OptionsDialog dialog(options_, this);
    connect(&dialog, &OptionsDialog::applied, this, [this](const detail::UserOptions& options) {
        applyOptions(options);
        saveUserOptions(options);
    });
    dialog.exec();
}

} // namespace bcad::app
