// Fichier > Importer : un menu reconstruit depuis FileImporterRegistry.

#include "MainWindow.h"

#include "Commands.h"
#include "Viewport.h"
#include "bcad/plugin/FileImporter.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>

#include <unordered_set>
#include <vector>

namespace bcad::app {

void MainWindow::rebuildImportMenu() {
    if (!importMenu_) return;
    importMenu_->clear();
    const auto importers = plugin::FileImporterRegistry::instance().importers();
    if (importers.empty()) {
        importMenu_->addAction(tr("Aucun format disponible"))->setEnabled(false);
        return;
    }
    for (const auto* importer : importers) {
        const std::string id = importer->id();
        importMenu_->addAction(tr("&%1...").arg(QString::fromStdString(importer->label())),
                               this, [this, id] { runFileImporter(id); });
    }
}

void MainWindow::runFileImporter(const std::string& id) {
    const auto* importer = plugin::FileImporterRegistry::instance().find(id);
    if (!importer) return;
    const QString label = QString::fromStdString(importer->label());
    const QString exts = QString::fromStdString(importer->extensions());
    QStringList extPatterns;
    for (const auto& e : exts.split(' ', Qt::SkipEmptyParts)) extPatterns << "*." + e;
    const QString filter = tr("%1 (%2)").arg(label, extPatterns.join(' '));
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Importer %1").arg(label), {}, filter);
    if (path.isEmpty()) return;

    // Les entites presentes avant l'import : tout ce qui apparait ensuite vient
    // de l'importeur et devient annulable d'un seul Ctrl+Z.
    std::unordered_set<int> before;
    for (const auto& entity : document_->entities()) before.insert(entity->id());

    std::string error;
    if (!importer->readDocument(*document_, path.toStdString(), &error)) {
        QMessageBox::warning(this, tr("Import impossible"), QString::fromStdString(error));
        return;
    }
    std::vector<int> added;
    for (const auto& entity : document_->entities())
        if (!before.count(entity->id())) added.push_back(entity->id());
    if (!added.empty())
        undoStack_->push(new RecordedAdditionCommand(document_, added, tr("Importer %1").arg(label)));
    viewport_->zoomToFit();
    // Un import reussi peut porter des remarques (lignes ecartees…) : elles
    // restent lisibles plutot que perdues.
    statusBar()->showMessage(
        error.empty() ? tr("%1 importé : %2").arg(label, path)
                      : tr("%1 importé avec remarques : %2").arg(label, QString::fromStdString(error)),
        error.empty() ? 4000 : 12000);
}

} // namespace bcad::app
