// Cycle de vie du document : nouveau, ouvert, enregistre, reprise apres
// plantage, import DXF, impression.
//
// Le point commun de ces chemins est qu'ils remplacent tous le contenu du
// document : chacun passe donc par confirmDiscard(), et chacun recalera les vues
// dependantes par refreshDocumentViews(). Le contenu est remplace en place, sous
// le verrou du document, plutot que reconstruit : Document porte un
// shared_mutex, il n'est ni copiable ni deplacable.

#include "MainWindow.h"

#include "PropertiesPanel.h"
#include "Viewport.h"
#include "bcad/io/Database.h"
#include "bcad/plugin/StyleProvider.h"
#ifdef BCAD_HAVE_DXF_BRIDGE
#include "bcad/io/DxfBridge.h"
#else
#include "bcad/io/DxfReader.h"
#include "bcad/io/DxfWriter.h"
#endif
#include "bcad/layout/PdfExport.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>

#include <memory>

namespace bcad::app {

void MainWindow::onNew() {
    if (!confirmDiscard()) return;
    document_->clear();
    document_->layerManager().reset();
    applyStyleProvidersToDocument(*document_);
    undoStack_.clear();
    currentFilePath_.clear();
    dirty_ = false;
    refreshDocumentViews();
}

// Faux = l'utilisateur a renonce. Appelle avant chaque chemin qui remplace le
// contenu du document : Nouveau, Ouvrir, Importer, et la fermeture de la
// fenetre. Jusqu'ici seul « Fermer » proposait de sauvegarder, et un
// Import DXF écrasait le travail en cours sans rien demander.
bool MainWindow::confirmDiscard() {
    if (!dirty_) return true;
    const QString name = currentFilePath_.isEmpty()
                             ? tr("Nouveau document")
                             : QFileInfo(currentFilePath_).fileName();
    auto reply = QMessageBox::question(
        this, tr("Modifications non enregistrées"),
        tr("« %1 » porte des modifications non enregistrées.\n"
           "Les enregistrer avant de poursuivre ?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (reply == QMessageBox::Cancel) return false;
    if (reply == QMessageBox::Discard) {
        dirty_ = false;
        return true;
    }
    if (currentFilePath_.isEmpty()) onSaveAs();
    else saveToPath(currentFilePath_);
    return !dirty_;   // l'enregistrement a ete annule en boiete
}

// Recale tout ce qui affiche le contenu du document. Le panneau de proprietes
// reconstruit ses champs a chaque rafraichissement : sans cet appel il
// continuait d'afficher les champs d'entites detruites.
void MainWindow::refreshDocumentViews() {
    propertiesPanel_->refresh();
    validationTree_->clear();
    viewport_->update();
    updateWindowTitle();
}

// Le nom du fichier et le marqueur de modifications : rien d'autre n'indique
// a l'utilisateur quel projet est ouvert ni s'il est enregistre.
void MainWindow::updateWindowTitle() {
    const QString name = currentFilePath_.isEmpty()
                             ? tr("Nouveau document")
                             : QFileInfo(currentFilePath_).fileName();
    setWindowTitle(tr("%1%2 — BCAD").arg(name, dirty_ ? tr(" *") : QString()));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

QString MainWindow::resolveRecoveryPath(const QString& path) {
    QString autosavePath = autosavePathFor(path);
    QFileInfo autosaveInfo(autosavePath);
    if (!autosaveInfo.exists()) return path;
    if (autosaveInfo.lastModified() <= QFileInfo(path).lastModified()) return path;

    auto reply = QMessageBox::question(
        this, tr("Récupérer la sauvegarde automatique"),
        tr("Une sauvegarde automatique de « %1 » est plus récente que le "
           "fichier lui-même (récupération après plantage ?).\n"
           "Charger la sauvegarde automatique ?")
            .arg(QFileInfo(path).fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    return reply == QMessageBox::Yes ? autosavePath : path;
}

void MainWindow::onOpen() {
    if (!confirmDiscard()) return;
    QString path = QFileDialog::getOpenFileName(this, tr("Ouvrir un projet"), {},
                                                tr("Projet bcad (*.bcad)"));
    if (path.isEmpty()) return;

    QString loadPath = resolveRecoveryPath(path);
    std::vector<std::string> diag;
    if (!io::Database::load(loadPath.toStdString(), *document_, &diag)) {
        QMessageBox::warning(this, tr("Ouverture impossible"),
                             tr("Impossible d'ouvrir « %1 ».").arg(loadPath));
        return;
    }
    applyStyleProvidersToDocument(*document_);
    if (!diag.empty()) {
        QString msg;
        for (const auto& d : diag) msg += QString::fromStdString(d) + '\n';
        statusBar()->showMessage(
            tr("%1 avertissement(s) au chargement — voir Outils > Diagnostics").arg(diag.size()), 8000);
        Q_UNUSED(msg); // réservé pour un panneau dédié
    }
    undoStack_.clear();
    // currentFilePath_ reste le vrai fichier projet même si on a chargé la
    // sauvegarde automatique, pour que Ctrl+S écrive dessus, pas sur le
    // fichier .autosave — et on garde `dirty_` à true dans ce cas pour que
    // le contenu récupéré ne soit pas perdu silencieusement.
    currentFilePath_ = path;
    dirty_ = (loadPath != path);
    viewport_->zoomToFit();
    refreshDocumentViews();
}

bool MainWindow::saveToPath(const QString& path) {
    if (!io::Database::save(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Enregistrement impossible"),
                             tr("Impossible d'enregistrer dans « %1 ».").arg(path));
        return false;
    }
    currentFilePath_ = path;
    dirty_ = false;
    updateWindowTitle();
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
    QString path = QFileDialog::getSaveFileName(this, tr("Enregistrer le projet sous"), {},
                                                tr("Projet bcad (*.bcad)"));
    if (path.isEmpty()) return;
    saveToPath(path);
}

void MainWindow::onImportDxf() {
    if (!confirmDiscard()) return;
    QString path = QFileDialog::getOpenFileName(this, tr("Importer un DXF"), {},
                                                tr("Fichiers DXF (*.dxf)"));
    if (path.isEmpty()) return;

#ifdef BCAD_HAVE_DXF_BRIDGE
    io::DxfBridgeOptions options;
    options.recovery_mode = io::DxfRecoveryMode::Recover;
    io::DxfBridgeResult result = io::readDxfFromFile(path.toStdString(), options);

    if (!result.success || result.document == nullptr) {
        QString msg = result.error_message.empty()
                          ? tr("Impossible de lire « %1 ».").arg(path)
                          : tr("Import échoué : %1").arg(QString::fromStdString(result.error_message));
        QMessageBox::warning(this, tr("Import impossible"), msg);
        return;
    }

    if (io::hasErrors(result)) {
        QString diagMsg;
        for (const auto& d : result.diagnostics) {
            if (d.severity >= 2) {
                diagMsg += QString::fromStdString(d.code + ": " + d.message + "\n");
            }
        }
        QMessageBox::warning(this, tr("Import avec avertissements"),
                             tr("Le DXF a été lu mais contient des erreurs :\n%1").arg(diagMsg));
    }

    document_ = std::unique_ptr<core::Document>(result.document.release());
#else
    // Sans le bridge Rust, l'import passe par le lecteur natif : il remplit le
    // Document en place (il le vide d'abord), ce qui evite de fabriquer un
    // Document que l'on ne pourrait pas=deplacer ensuite.
    if (!io::readDxf(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Import impossible"),
                             tr("Impossible de lire « %1 ».").arg(path));
        return;
    }
#endif
    undoStack_.clear();
    currentFilePath_.clear();
    dirty_ = true;
    viewport_->zoomToFit();
    refreshDocumentViews();
}

void MainWindow::onExportDxf() {
    QString path = QFileDialog::getSaveFileName(this, tr("Exporter en DXF"), {},
                                                tr("Fichiers DXF (*.dxf)"));
    if (path.isEmpty()) return;

#ifdef BCAD_HAVE_DXF_BRIDGE
    io::DxfWriteResult result = io::writeDxfToFile(*document_, path.toStdString());

    if (!result.success) {
        QString msg = result.error_message.empty()
                          ? tr("Impossible d'exporter « %1 ».").arg(path)
                          : tr("Export échoué : %1").arg(QString::fromStdString(result.error_message));
        QMessageBox::warning(this, tr("Export impossible"), msg);
        return;
    }
#else
    if (!io::writeDxf(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Export impossible"),
                             tr("Impossible d'exporter « %1 ».").arg(path));
        return;
    }
#endif

    QMessageBox::information(this, tr("Export réussi"),
                             tr("Le fichier DXF a été exporté dans « %1 ».").arg(path));
}

void MainWindow::onPrintPreview() {
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this](QPrinter* printer) {
        // L'aperçu EST la feuille exportée : même composition, même peintre, aucun
        // code de dessin ici. Sans module, aucun meuble : l'hôte ne connaît ni
        // métadonnées projet ni vocabulaire à peindre — c'est le plugin qui les
        // porte (ADR-016, ADR-017).
        layout::PdfExportOptions options;
        options.sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        options.viewport.setSource(document_->extents());
        if (!options.viewport.source().isValid()) return;
        options.document = document_.get();
        layout::applyFittingScale(options);
        layout::applyPageLayout(printer, options.sheet);

        QPainter painter(printer);
        if (!painter.isActive()) return;
        layout::drawSheet(painter, options);
    });
    preview.exec();
}

// Conversion ACI → RGB (palette standard DXF, couleurs 1-9).
static geom::Color aciToColor(int aci) {
    switch (aci) {
        case 1: return geom::Color::fromRgb255(255,   0,   0);
        case 2: return geom::Color::fromRgb255(255, 255,   0);
        case 3: return geom::Color::fromRgb255(  0, 255,   0);
        case 4: return geom::Color::fromRgb255(  0, 255, 255);
        case 5: return geom::Color::fromRgb255(  0,   0, 255);
        case 6: return geom::Color::fromRgb255(255,   0, 255);
        case 7: return geom::Color::fromRgb255(255, 255, 255);
        case 8: return geom::Color::fromRgb255(128, 128, 128);
        case 9: return geom::Color::fromRgb255(192, 192, 192);
        default: return geom::Color::fromRgb255(255, 255, 255);
    }
}

// Applique les calques déclarés par les IStyleProvider enregistrés au document.
// Appelé une fois après loadAllDiscovered() — premier consommateur de IStyleProvider (§7.1).
void MainWindow::applyStyleProvidersToDocument(core::Document& doc) {
    for (const auto* provider : plugin::StyleProviderRegistry::instance().providers()) {
        for (const auto& ls : provider->layerStyles()) {
            auto& layer = doc.layerManager().createLayer(ls.name, aciToColor(ls.aci));
            layer.visible = ls.visible;
            layer.locked  = ls.locked;
        }
    }
}

} // namespace bcad::app
