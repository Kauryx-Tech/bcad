// Mediation entre l'hote et les points d'extension declares par les modules
// metiers : workbenches (menus et ruban), validateurs, exporteurs de fichier.
// L'execution d'une action de workbench vit dans MainWindowWorkbench.cpp.
//
// L'hote ne connait le metier de personne (ADR-016 principe 4). Il ne fait que
// lire ce que chaque module declare : les TypeIds auxquels une action s'applique
// et la strategie de rassemblement des arguments, les diagnostics tels que les
// ecrit le validateur, le libelle et l'extension tels que les declare
// l'exporteur. Aucune regle, aucun nom de domaine ne s'ecrit ici.

#include "MainWindow.h"

#include "QtCommandAdapter.h"
#include "RibbonBar.h"
#include "TypeFilter.h"
#include "Viewport.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/plugin/FileExporter.h"
#include "bcad/plugin/FileImporter.h"
#include "bcad/plugin/Validator.h"

#include <QColor>
#include <QDockWidget>
#include <QFileDialog>
#include <QIcon>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTreeWidget>
#include <QVariant>

#include <string>
#include <vector>

namespace bcad::app {


// Execute les validateurs enregistres par les plugins. L'hote ne decide d'aucune
// regle : il filtre les entites selon les TypeIds que chaque validateur declare,
// passe le document entier aux validateurs de document (feuilles, vues,
// attributs du dossier — ADR-017 decision 5), et affiche les diagnostics tels
// qu'ils sont ecrits par le plugin.
void MainWindow::runValidation(std::vector<geom::Entity*> scope) {
    const auto validators = plugin::ValidatorRegistry::instance().validators();
    const auto documentValidators = plugin::DocumentValidatorRegistry::instance().validators();
    validationTree_->clear();
    if (validators.empty() && documentValidators.empty()) {
        statusBar()->showMessage(tr("Aucun validateur enregistré : chargez un module métier"), 5000);
        return;
    }

    int errors = 0;
    int warnings = 0;
    int notes = 0;
    auto showGroup = [&](const std::string& label,
                         const std::vector<validation::Diagnostic>& diagnostics) {
        if (diagnostics.empty()) return;
        auto* group = new QTreeWidgetItem(validationTree_);
        group->setText(0, QString::fromStdString(label));
        group->setText(1, tr("%1 constat(s)").arg(diagnostics.size()));
        for (const auto& diagnostic : diagnostics) {
            QString severity;
            switch (diagnostic.severity) {
            case validation::Severity::Error:
                severity = tr("Erreur");
                ++errors;
                break;
            case validation::Severity::Warning:
                severity = tr("Avertissement");
                ++warnings;
                break;
            case validation::Severity::Info:
                severity = tr("Info");
                ++notes;
                break;
            }
            auto* item = new QTreeWidgetItem(group);
            item->setText(0, severity);
            item->setText(1, QString::fromStdString(diagnostic.message));
            QVariantList ids;
            for (const int id : diagnostic.entityIds) ids.push_back(id);
            item->setData(0, Qt::UserRole, ids);
            item->setForeground(0, QColor(
                diagnostic.severity == validation::Severity::Error ? "#e06c6c"
                : diagnostic.severity == validation::Severity::Warning ? "#e0b060"
                                                                       : "#9aa0a6"));
        }
    };

    for (const auto* validator : validators) {
        std::vector<geom::Entity*> relevant;
        for (auto* entity : scope) {
            if (typeMatches(*entity, validator->applicableTypes()))
                relevant.push_back(entity);
        }
        if (relevant.empty()) continue;

        showGroup(validator->label(), validator->validate(relevant));
    }

    if (document_) {
        for (const auto* validator : documentValidators)
            showGroup(validator->label(), validator->validateDocument(*document_));
    }

    validationTree_->expandAll();
    // Le dock peut avoir ete ferme par l'utilisateur : sans lui, les resultats
    // seraient produits et jetes.
    if (validationDock_) {
        validationDock_->setVisible(true);
        validationDock_->raise();
    }
    statusBar()->showMessage(tr("%1 erreur(s), %2 avertissement(s), %3 info(s) sur %4 entité(s) vérifiée(s)")
                                 .arg(errors).arg(warnings).arg(notes)
                                 .arg(scope.size()), 8000);
}

void MainWindow::buildPluginMenus() {
    rebuildImportMenu();
    rebuildExportMenu();
    auto& registry = plugin::WorkbenchRegistry::instance();
    for (const auto* workbench : registry.workbenches()) {
        QMenu* menu = new QMenu("&" + QString::fromStdString(workbench->label()), this);
        // Insere avant « Aide » et non ajoute a la fin : l'ordre des menus est
        // celui annonce dans UI_CONVENTIONS.md, il ne doit pas dependre de
        // l'ordre (ni du nombre) des modules charges.
        menuBar()->insertMenu(helpMenu_->menuAction(), menu);
        const QString description = QString::fromStdString(workbench->description());
        // La description du workbench etait lue puis jetee : c'est l'infobulle
        // de l'entree de menu. Les infobulles des actions s'affichent ensuite
        // dans le menu ouvert, qu'il y ait une description ou non.
        menu->setToolTipsVisible(true);
        if (!description.isEmpty()) menu->menuAction()->setToolTip(description);
        for (const auto& panel : workbench->panels()) {
            if (!panel.title.empty())
                menu->addSection(QString::fromStdString(panel.title));
            // Ruban : les actions `prominent` en grands boutons, en tete ; les
            // autres empilees. L'icone est un fichier du module, lu tel quel.
            QList<QAction*> large, small;
            for (const auto& action : panel.actions) {
                QAction* item = menu->addAction(QString::fromStdString(action.label), this,
                    [this, action] { executeWorkbenchAction(action); });
                if (!action.tooltip.empty())
                    item->setToolTip(QString::fromStdString(action.tooltip));
                if (!action.icon.empty()) item->setIcon(QIcon(QString::fromStdString(action.icon)));
                (action.prominent ? large : small).push_back(item);
            }
            if (!panel.title.empty())
                ribbon_->addPanel(QString::fromStdString(workbench->label()),
                                  QString::fromStdString(panel.title), large + small,
                                  static_cast<int>(large.size()));
        }
        menu->addSeparator();
        menu->addAction(tr("Commandes disponibles"), this, [this] {
            const auto names = commands::CommandRegistry::instance().getRegisteredCommands();
            statusBar()->showMessage(
                names.empty() ? tr("Aucune commande de plugin chargée")
                              : tr("%1 commande(s) enregistrée(s)").arg(names.size()), 4000);
        });
    }
}

// Le menu « Exporter » n'énumère aucun format : il dresse la liste des
// exporteurs enregistres, y compris ceux d'un module metier charge. Libelle et
// extension viennent de leur declarant (ADR-016).
void MainWindow::rebuildExportMenu() {
    if (!exportMenu_) return;
    exportMenu_->clear();
    const auto exporters = plugin::FileExporterRegistry::instance().exporters();
    if (exporters.empty()) {
        exportMenu_->addAction(tr("Aucun format disponible"))->setEnabled(false);
        return;
    }
    for (const auto* exporter : exporters) {
        const std::string id = exporter->id();
        exportMenu_->addAction(tr("&%1...").arg(QString::fromStdString(exporter->label())),
                               this, [this, id] { runFileExporter(id); });
    }
}

void MainWindow::runFileExporter(const std::string& id) {
    const auto* exporter = plugin::FileExporterRegistry::instance().find(id);
    if (!exporter) return;
    const QString label = QString::fromStdString(exporter->label());
    const QString filter = tr("%1 (*.%2)").arg(label,
                                               QString::fromStdString(exporter->extension()));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exporter %1").arg(label), {}, filter);
    if (path.isEmpty()) return;

    std::string error;
    if (!exporter->writeDocument(*document_, path.toStdString(), &error)) {
        QMessageBox::warning(this, tr("Export impossible"), QString::fromStdString(error));
        return;
    }
    statusBar()->showMessage(tr("%1 exporté : %2").arg(label, path), 4000);
}

} // namespace bcad::app
