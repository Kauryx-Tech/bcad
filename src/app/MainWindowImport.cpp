// Fichier > Importer : un menu reconstruit depuis FileImporterRegistry.

#include "MainWindow.h"

#include "Viewport.h"
#include "bcad/plugin/FileImporter.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>

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

    std::string error;
    if (!importer->readDocument(*document_, path.toStdString(), &error)) {
        QMessageBox::warning(this, tr("Import impossible"), QString::fromStdString(error));
        return;
    }
    viewport_->update();
    statusBar()->showMessage(tr("%1 importé : %2").arg(label, path), 4000);
}

} // namespace bcad::app
