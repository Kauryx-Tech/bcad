// Mediation entre l'hote et les points d'extension declares par les modules
// metiers : workbenches, validateurs, exporteurs de fichier.
//
// L'hote ne connait le metier de personne (ADR-016 principe 4). Il ne fait que
// lire ce que chaque module declare : les TypeIds auxquels une action s'applique
// et la strategie de rassemblement des arguments, les diagnostics tels que les
// ecrit le validateur, le libelle et l'extension tels que les declare
// l'exporteur. Aucune regle, aucun nom de domaine ne s'ecrit ici.

#include "MainWindow.h"

#include "QtCommandAdapter.h"
#include "RibbonBar.h"
#include "Viewport.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/plugin/FileExporter.h"
#include "bcad/plugin/Validator.h"

#include <QColor>
#include <QDockWidget>
#include <QFileDialog>
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

// Rassemble les arguments d'une action de workbench selon une strategie
// generique : l'hote ne connait ni le nom ni la signification des entites, il
// filtre la selection sur les TypeIds que le plugin a declares.
namespace {

bool typeMatches(const geom::Entity& entity,
                 const std::vector<std::string>& acceptedTypes) {
    if (acceptedTypes.empty()) return true;
    for (const auto& type : acceptedTypes) {
        if (entity.typeId().value == type) return true;
    }
    return false;
}

} // namespace

void MainWindow::executeWorkbenchAction(plugin::WorkbenchAction action) {
    std::vector<geom::Entity*> selected;
    for (const auto& entity : document_->entities()) {
        if (entity->selected && typeMatches(*entity, action.selectedTypes))
            selected.push_back(entity.get());
    }
    const int count = static_cast<int>(selected.size());
    if (count < action.minSelected ||
        (action.maxSelected > 0 && count > action.maxSelected)) {
        statusBar()->showMessage(tr("Sélection non valable pour « %1 »")
                                     .arg(QString::fromStdString(action.label)), 4000);
        return;
    }

    std::vector<std::string> args;
    auto* polyline = selected.empty()
                         ? nullptr
                         : dynamic_cast<geom::PolylineEntity*>(selected.front());

    switch (action.params) {
    case plugin::WorkbenchParams::None:
        break;
    case plugin::WorkbenchParams::SelectionIds:
        for (auto* entity : selected)
            args.push_back(std::to_string(entity->id()));
        break;
    case plugin::WorkbenchParams::BoxSplit: {
        if (!polyline || polyline->vertices().size() < 3) {
            statusBar()->showMessage(tr("Sélection non valable"), 4000);
            return;
        }
        const auto box = polyline->boundingBox();
        const double x = (box.minX + box.maxX) * 0.5;
        args = {std::to_string(polyline->id()), std::to_string(x),
                std::to_string(box.minY - 1.0), std::to_string(x),
                std::to_string(box.maxY + 1.0)};
        break;
    }
    case plugin::WorkbenchParams::RunValidators: {
        // Aucune commande : l'hote execute les validateurs des plugins.
        // Perimetre = la selection si elle existe, sinon tout le document.
        std::vector<geom::Entity*> scope = selected;
        if (scope.empty()) {
            for (const auto& entity : document_->entities()) {
                if (typeMatches(*entity, action.selectedTypes))
                    scope.push_back(entity.get());
            }
        }
        runValidation(scope);
        return;
    }
    case plugin::WorkbenchParams::PromptText: {
        // L'hote demande une chaine, il n'en connait ni le sens ni la forme
        // attendue : c'est la factory du plugin qui accepte ou refuse.
        bool accepted = false;
        const QString text = QInputDialog::getText(
            this, QString::fromStdString(action.label),
            QString::fromStdString(action.prompt), QLineEdit::Normal,
            QString(), &accepted);
        if (!accepted) return;
        args = {text.trimmed().toStdString()};
        break;
    }
    case plugin::WorkbenchParams::Vertices: {
        if (!polyline || polyline->vertices().size() < 3) {
            statusBar()->showMessage(tr("Sélection non valable"), 4000);
            return;
        }
        bool accepted = false;
        const int index = QInputDialog::getInt(
            this, tr("Modifier un sommet"),
            tr("Sommet à modifier (1 à %1) :").arg(polyline->vertices().size()),
            1, 1, static_cast<int>(polyline->vertices().size()), 1, &accepted);
        if (!accepted) return;
        const auto current = polyline->vertices()[static_cast<size_t>(index - 1)];
        const double x = QInputDialog::getDouble(
            this, tr("Modifier un sommet"), tr("Nouvelle coordonnée X :"),
            current.x_, -1e12, 1e12, 6, &accepted);
        if (!accepted) return;
        const double y = QInputDialog::getDouble(
            this, tr("Modifier un sommet"), tr("Nouvelle coordonnée Y :"),
            current.y_, -1e12, 1e12, 6, &accepted);
        if (!accepted) return;
        auto vertices = polyline->vertices();
        vertices[static_cast<size_t>(index - 1)] = {x, y};
        args.push_back(std::to_string(polyline->id()));
        for (const auto& vertex : vertices) {
            args.push_back(std::to_string(vertex.x_));
            args.push_back(std::to_string(vertex.y_));
        }
        break;
    }
    }

    auto command = commands::CommandRegistry::instance().createCommand(
        action.commandName, args);
    if (!command) {
        // Deux causes possibles, et l'hote ne peut pas les distinguer : le
        // plugin n'est pas charge, ou il a refuse des arguments qu'il est seul
        // à connaître. Le message le dit plutot que d'accuser le chargement.
        statusBar()->showMessage(
            tr("Commande « %1 » indisponible : plugin non chargé ou arguments refusés")
                .arg(QString::fromStdString(action.commandName)), 6000);
        return;
    }
    const QString label = QString::fromStdString(action.label);
    if (action.modal || !action.modifiesDocument) {
        command->execute(*document_);
    } else {
        undoStack_.push(new QtCommandAdapter(document_.get(), std::move(command), label));
    }
    // Une action qui ne change pas le dessin (une recherche, un livrable
    // exterieur) ne doit pas faire passer le document pour modifie ni pousser
    // quoi que ce soit dans la pile d'annulation.
    if (action.modifiesDocument) {
        dirty_ = true;
        updateWindowTitle();
    }
    if (!action.modifiesDocument) {
        // Le seul effet visible est la selection : le compte la rend lisible.
        // Les types acceptes sont declares par le plugin, pas devines ici.
        int selectedAfter = 0;
        for (const auto& entity : document_->entities()) {
            if (entity->selected && typeMatches(*entity, action.selectedTypes))
                ++selectedAfter;
        }
        statusBar()->showMessage(
            tr("%1 entité(s) sélectionnée(s) sur %2")
                .arg(selectedAfter).arg(document_->entities().size()), 6000);
    }
    viewport_->update();
}

// Execute les validateurs enregistres par les plugins. L'hote ne decide d'aucune
// regle : il filtre les entites selon les TypeIds que chaque validateur declare,
// et affiche les diagnostics tels qu'ils sont ecrits par le plugin.
void MainWindow::runValidation(std::vector<geom::Entity*> scope) {
    const auto validators = plugin::ValidatorRegistry::instance().validators();
    validationTree_->clear();
    if (validators.empty()) {
        statusBar()->showMessage(tr("Aucun validateur enregistré : chargez un module métier"), 5000);
        return;
    }

    int errors = 0;
    int warnings = 0;
    int notes = 0;
    for (const auto* validator : validators) {
        std::vector<geom::Entity*> relevant;
        for (auto* entity : scope) {
            if (typeMatches(*entity, validator->applicableTypes()))
                relevant.push_back(entity);
        }
        if (relevant.empty()) continue;

        const auto diagnostics = validator->validate(relevant);
        if (diagnostics.empty()) continue;

        auto* group = new QTreeWidgetItem(validationTree_);
        group->setText(0, QString::fromStdString(validator->label()));
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
            QList<QAction*> panelActions;
            for (const auto& action : panel.actions) {
                QAction* item = menu->addAction(QString::fromStdString(action.label), this,
                    [this, action] { executeWorkbenchAction(action); });
                if (!action.tooltip.empty())
                    item->setToolTip(QString::fromStdString(action.tooltip));
                panelActions.push_back(item);
            }
            if (!panel.title.empty())
                ribbon_->addPanel(QString::fromStdString(workbench->label()),
                                  QString::fromStdString(panel.title), panelActions);
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
