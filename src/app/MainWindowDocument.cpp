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
#include "SaveDialog.h"
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

// Nouveau ouvre un dessin dans un nouvel onglet : le dessin en cours reste
// ouvert, rien n'est a confirmer.
void MainWindow::onNew() {
    activateSession(newUntitledSession());
}

// Faux = l'utilisateur a renonce. Porte sur le dessin de l'onglet actif ;
// appele avant de le fermer, seul ou avec la fenetre.
bool MainWindow::confirmDiscard() {
    if (!session().dirty) return true;
    const QString name = session().displayName();
    auto reply = QMessageBox::question(
        this, tr("Modifications non enregistrées"),
        tr("« %1 » porte des modifications non enregistrées.\n"
           "Les enregistrer avant de poursuivre ?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (reply == QMessageBox::Cancel) return false;
    if (reply == QMessageBox::Discard) {
        session().dirty = false;
        return true;
    }
    if (session().filePath.isEmpty()) onSaveAs();
    else saveToPath(session().filePath);
    return !session().dirty;   // l'enregistrement a ete annule en boiete
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
    setWindowTitle(tr("%1 — BCAD").arg(session().tabLabel()));
}

// Chaque dessin modifie est propose a l'enregistrement, onglet par onglet ;
// renoncer une seule fois garde la fenetre ouverte.
void MainWindow::closeEvent(QCloseEvent* event) {
    for (int i = 0; i < sessions_.count(); ++i) {
        if (!sessions_.at(i).dirty) continue;
        activateSession(i);
        if (!confirmDiscard()) {
            event->ignore();
            return;
        }
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

// Plusieurs fichiers peuvent etre choisis d'un coup : chacun s'ouvre dans son
// onglet (voir openFile, MainWindowSessions.cpp).
void MainWindow::onOpen() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Ouvrir des dessins"), {}, tr("Projet bcad (*.bcad)"));
    for (const QString& path : paths) openFile(path);
}

bool MainWindow::saveToPath(const QString& path) {
    if (!io::Database::save(path.toStdString(), *document_)) {
        QMessageBox::warning(this, tr("Enregistrement impossible"),
                             tr("Impossible d'enregistrer dans « %1 ».").arg(path));
        return false;
    }
    session().filePath = path;
    session().dirty = false;
    updateTabLabel(activeSession_);
    updateWindowTitle();
    // Le fichier réel étant maintenant à jour, une sauvegarde automatique
    // plus ancienne n'a plus lieu d'être proposée à la prochaine ouverture.
    QFile::remove(autosavePathFor(path));
    return true;
}

// Tous les dessins ouverts, pas seulement l'onglet actif.
void MainWindow::onAutosaveTimeout() {
    for (int i = 0; i < sessions_.count(); ++i) {
        const DocumentSession& open = sessions_.at(i);
        if (open.filePath.isEmpty() || !open.dirty) continue;
        io::Database::save(autosavePathFor(open.filePath).toStdString(), *open.document);
    }
}

void MainWindow::onSave() {
    if (session().filePath.isEmpty()) {
        onSaveAs();
        return;
    }
    saveToPath(session().filePath);
}

void MainWindow::onSaveAs() {
    const QString path = askSavePath(this, tr("Enregistrer le projet sous"),
                                     tr("Projet bcad (*.bcad)"), QStringLiteral("bcad"));
    if (path.isEmpty()) return;
    saveToPath(path);
}

// Un DXF s'ouvre dans un nouvel onglet, nomme d'apres le fichier : le dessin
// en cours n'est plus ecrase, il n'y a donc rien a confirmer.
void MainWindow::onImportDxf() {
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

    auto imported = std::make_unique<DocumentSession>();
    imported->document = std::unique_ptr<core::Document>(result.document.release());
#else
    // Sans le bridge Rust, l'import passe par le lecteur natif : il remplit le
    // Document en place (il le vide d'abord), ce qui evite de fabriquer un
    // Document que l'on ne pourrait pas=deplacer ensuite.
    auto imported = std::make_unique<DocumentSession>();
    if (!io::readDxf(path.toStdString(), *imported->document)) {
        QMessageBox::warning(this, tr("Import impossible"),
                             tr("Impossible de lire « %1 ».").arg(path));
        return;
    }
#endif
    applyStyleProvidersToDocument(*imported->document);
    // Jamais enregistre en .bcad : nomme d'apres le DXF, et marque modifie
    // pour que la fermeture propose de l'enregistrer.
    imported->untitledName = QFileInfo(path).fileName();
    imported->dirty = true;
    activateSession(addSessionTab(sessions_.add(std::move(imported))));
    viewport_->zoomToFit();
}

void MainWindow::onExportDxf() {
    const QString path = askSavePath(this, tr("Exporter en DXF"),
                                     tr("Fichiers DXF (*.dxf)"), QStringLiteral("dxf"));
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
        options.document = document_;
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
