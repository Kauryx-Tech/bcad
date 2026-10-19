// Presse-papiers (E-01) : actions du menu Edition, avec les raccourcis
// d'AutoCAD (Ctrl+X, Ctrl+C, Ctrl+Maj+C, Ctrl+V). Le ruban (Debut ›
// Presse-papiers) et le menu du clic droit reprennent les memes actions.
// Dans la ligne de commande, Ctrl+C / Ctrl+V gardent leur sens de texte : le
// champ qui a le focus passe avant le raccourci de la fenetre.

#include "MainWindow.h"

#include "ActionIcons.h"
#include "Viewport.h"

#include <QAction>
#include <QMenu>

namespace bcad::app {

MainWindow::ClipboardActions MainWindow::buildClipboardActions(QMenu* editMenu) {
    ClipboardActions a{};
    a.cut = editMenu->addAction(tr("Co&uper"), QKeySequence::Cut, viewport_, &Viewport::cutClip);
    a.copy = editMenu->addAction(tr("&Copier dans le presse-papiers"), QKeySequence::Copy, viewport_,
                                 &Viewport::copyClip);
    a.copyBase = editMenu->addAction(tr("Copier avec &point de base"), QKeySequence(tr("Ctrl+Shift+C")),
                                     viewport_, &Viewport::copyBase);
    a.paste = editMenu->addAction(tr("C&oller"), QKeySequence::Paste, viewport_, &Viewport::pasteClip);
    a.pasteOriginal = editMenu->addAction(tr("Coller aux coordonnées d'&origine"), viewport_,
                                          &Viewport::pasteOriginal);
    setActionIcon(this, a.cut, QStyle::SP_DialogDiscardButton, QStringLiteral("edit-cut"));
    setActionIcon(this, a.copy, QStyle::SP_FileDialogContentsView, QStringLiteral("edit-copy"));
    setActionIcon(this, a.copyBase, QStyle::SP_FileDialogContentsView, QStringLiteral("edit-copy"));
    setActionIcon(this, a.paste, QStyle::SP_DialogApplyButton, QStringLiteral("edit-paste"));
    setActionIcon(this, a.pasteOriginal, QStyle::SP_DialogApplyButton, QStringLiteral("edit-paste"));
    // Libelles courts du ruban, comme AutoCAD.
    a.copy->setIconText(tr("Copier"));
    a.copyBase->setIconText(tr("Copier avec point de base"));
    a.pasteOriginal->setIconText(tr("Coller aux coordonnées d'origine"));
    editMenu->addSeparator();
    return a;
}

} // namespace bcad::app
