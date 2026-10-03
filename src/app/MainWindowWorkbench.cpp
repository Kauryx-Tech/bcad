// Execution d'une action de workbench declaree par un module metier.
//
// L'hote rassemble les arguments selon une strategie generique (WorkbenchParams),
// filtre la selection sur les TypeIds que le module a declares, puis cree la
// commande par son nom dans le CommandRegistry. Il ne sait ni ce que fait la
// commande ni ce que signifient ses arguments : la factory du module accepte ou
// refuse (ADR-016).

#include "MainWindow.h"

#include "QtCommandAdapter.h"
#include "TypeFilter.h"
#include "Viewport.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/geometry/Polyline.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QStatusBar>

#include <string>
#include <vector>

namespace bcad::app {

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
    case plugin::WorkbenchParams::BoxSplit:
    case plugin::WorkbenchParams::PromptBoxSplit: {
        if (!polyline || polyline->vertices().size() < 3) {
            statusBar()->showMessage(tr("Sélection non valable"), 4000);
            return;
        }
        args = {std::to_string(polyline->id())};
        if (action.params == plugin::WorkbenchParams::PromptBoxSplit) {
            bool accepted = false;
            const QString value = QInputDialog::getText(
                this, QString::fromStdString(action.label),
                QString::fromStdString(action.prompt), QLineEdit::Normal, QString(), &accepted);
            if (!accepted) return;
            args.push_back(value.trimmed().toStdString());
        }
        // Ligne verticale mediane de l'emprise, debordant d'une unite de part
        // et d'autre pour couper franchement le contour.
        const auto box = polyline->boundingBox();
        const double x = (box.minX + box.maxX) * 0.5;
        for (double v : {x, box.minY - 1.0, x, box.maxY + 1.0}) args.push_back(std::to_string(v));
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

} // namespace bcad::app
