// L'outil Texte et la modification d'un texte (D-01, D-01b).
//
// Comme la commande TEXTE d'AutoCAD : point de depart, hauteur (valeur tapee,
// ou second clic donnant la distance ; Entree garde la precedente), angle
// (degres tapes, ou clic ; Entree garde le precedent), puis le texte tape dans
// la ligne de commande. Chaque Entree pose une ligne et prepare la suivante
// en dessous ; une ligne vide termine la commande.

#include "Viewport.h"

#include "Commands.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/TextEntity.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QUndoStack>

#include <cmath>
#include <numbers>

namespace bcad::app {

using geom::Point2;

namespace {

// Nombre tape : virgule ou point decimal.
std::optional<double> typedNumber(QString text) {
    text = text.trimmed().replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double value = text.toDouble(&ok);
    if (!ok || !std::isfinite(value)) return std::nullopt;
    return value;
}

} // namespace

void Viewport::placeText(const Point2& world) {
    switch (textStage_) {
        case 0:
            toolPoints_ = {world};
            textStage_ = 1;
            break;
        case 1: {
            const double h = geom::distance(toolPoints_.front(), world);
            if (h > geom::Tolerance::kDegenerateLength) textHeight_ = h;
            textStage_ = 2;
            break;
        }
        case 2:
            textRotation_ = geom::angleOf(toolPoints_.front(), world);
            textStage_ = 3;
            emit typedInputRequested(QString());   // le texte se tape dans la ligne de commande
            break;
        default:
            // Pendant la saisie du contenu, un clic deplace le point de la
            // ligne suivante, comme AutoCAD.
            toolPoints_ = {world};
            break;
    }
}

void Viewport::submitTextValue(const QString& text) {
    switch (textStage_) {
        case 1:
            if (!text.trimmed().isEmpty()) {
                const auto h = typedNumber(text);
                if (!h || *h <= 0.0) {
                    emit statusMessage(tr("Hauteur de texte invalide : %1").arg(text));
                    return;
                }
                textHeight_ = *h;
            }
            textStage_ = 2;
            break;
        case 2:
            if (!text.trimmed().isEmpty()) {
                const auto degrees = typedNumber(text);
                if (!degrees) {
                    emit statusMessage(tr("Angle invalide : %1").arg(text));
                    return;
                }
                textRotation_ = *degrees * std::numbers::pi / 180.0;
            }
            textStage_ = 3;
            emit typedInputRequested(QString());
            break;
        case 3: {
            if (text.isEmpty()) {
                endCommand();
                return;
            }
            commitEntity(std::make_unique<geom::TextEntity>(toolPoints_.front(), text.toStdString(),
                                                            textHeight_, textRotation_),
                         tr("Texte"));
            // Ligne suivante : 1,5 hauteur plus bas, perpendiculairement a l'angle.
            const double step = textHeight_ * 1.5;
            const Point2 base = toolPoints_.front();
            toolPoints_ = {Point2(base.x_ + std::sin(textRotation_) * step,
                                  base.y_ - std::cos(textRotation_) * step)};
            emit typedInputRequested(QString());
            break;
        }
        default:
            break;
    }
    notifyPrompt();
    update();
}

void Viewport::editText(int entityId) {
    if (!doc_) return;
    auto* text = dynamic_cast<geom::TextEntity*>(doc_->findEntity(entityId));
    if (!text) return;
    bool accepted = false;
    const QString edited = QInputDialog::getText(
        this, tr("Modifier le texte"), tr("Texte :"), QLineEdit::Normal,
        QString::fromStdString(text->text()), &accepted);
    if (!accepted || edited.toStdString() == text->text()) return;
    if (edited.isEmpty()) {
        emit statusMessage(tr("Un texte vide ne se garde pas : supprimez l'objet pour l'effacer."));
        return;
    }
    if (undoStack_) {
        undoStack_->push(new SetTextCommand(doc_, text, edited.toStdString(), tr("Modifier le texte")));
    } else {
        text->setText(edited.toStdString());
        doc_->notifyEntityChanged(text);
    }
    update();
}

void Viewport::editSelectedText() {
    for (geom::Entity* e : selectedEntities()) {
        if (dynamic_cast<geom::TextEntity*>(e)) {
            editText(e->id());
            return;
        }
    }
    emit statusMessage(tr("Sélectionnez d'abord un texte à modifier."));
}

} // namespace bcad::app
