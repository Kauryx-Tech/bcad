// Raccord (M-04), comme RACCORD / FILLET d'AutoCAD : on designe le premier
// objet, puis le second, au clic — sans selection prealable. Le point
// designe dit quel cote de chaque objet garder. Un arc tangent de rayon R
// les relie ; R = 0 donne un coin vif (les deux objets prolonges ou coupes
// jusqu'a leur intersection).
//
// Options tapees : un nombre (ou « R » puis le nombre) fixe le rayon, garde
// d'une commande a l'autre ; « A » bascule ajuster / ne pas ajuster les
// objets ; « M » enchaine plusieurs raccords. Objets : lignes, arcs,
// segments de polyligne. Calculs : detail::planFillet (ModifyDetail.h).

#include "viewport/Viewport.h"

#include "editing/Commands.h"
#include "editing/EditBatch.h"
#include "detail/ModifyDetail.h"
#include "viewport/ViewportTolerances.h"

#include "bcad/core/Document.h"

#include <QPainter>
#include <QUndoStack>

#include <algorithm>
#include <memory>

namespace bcad::app {

using geom::Point2;

void Viewport::applyFillet(const Point2& world) {
    geom::Entity* hit = pickEntityAt(world);
    if (!hit || !detail::filletableType(hit)) {
        emit statusMessage(tr("Raccord : désignez une ligne, un arc ou une polyligne."));
        return;
    }
    geom::Entity* first = filletFirstId_ >= 0 ? doc_->findEntity(filletFirstId_) : nullptr;
    if (!first) {
        filletFirstId_ = hit->id();
        toolPoints_.assign(1, world);
        return;
    }
    detail::FilletError err = detail::FilletError::None;
    auto plan = detail::planFillet(first, toolPoints_[0], hit, world, filletRadiusMemo_, filletTrim_, &err);
    if (!plan) {
        switch (err) {
            case detail::FilletError::Same:
                emit statusMessage(tr("Raccord : désignez un second objet, différent du premier."));
                return; // on garde le premier objet, l'utilisateur recommence
            case detail::FilletError::NoIntersection:
                emit statusMessage(tr("Raccord : objets parallèles ou sans intersection."));
                break;
            case detail::FilletError::Radius:
                emit statusMessage(tr("Raccord : rayon %1 impossible entre ces objets.").arg(filletRadiusMemo_));
                break;
            default:
                emit statusMessage(tr("Raccord : type d'objet non pris en charge."));
                break;
        }
        filletFirstId_ = -1;
        toolPoints_.clear();
        return;
    }
    if (!plan->joint && !plan->n1) {
        emit statusMessage(tr("Raccord : rayon 0 sans ajustement, rien à faire."));
    } else {
        EditBatch batch(doc_, undoStack_, tr("Raccord"));
        auto place = [&](std::unique_ptr<geom::Entity> e, const geom::Entity* from) {
            e->setLayer(from->layer());
            if (from->colorOverride()) e->setColorOverride(from->colorOverride());
            e->selected = false;
            batch.add(std::move(e));
        };
        if (plan->joint) place(std::move(plan->joint), first);
        if (plan->n1) {
            place(std::move(plan->n1), first);
            place(std::move(plan->n2), hit);
            batch.remove(first);
            batch.remove(hit);
        }
    }
    filletFirstId_ = -1;
    toolPoints_.clear();
    if (!filletMultiple_) endCommand();
}

bool Viewport::submitFilletOption(const QString& text) {
    if (tool_ != ToolMode::Fillet) return false;
    QString option = text.trimmed().toUpper();
    if (option.isEmpty()) return false;
    if (option.startsWith(QLatin1Char('R')) && option != QLatin1String("R")) {
        option = option.mid(option.startsWith(QLatin1String("RAYON")) ? 5 : 1).trimmed();
        if (option.isEmpty()) {
            filletAwaitRadius_ = true;
            notifyPrompt();
            return true;
        }
    }
    bool ok = false;
    const double value = option.toDouble(&ok);
    if (ok) {
        if (value < 0.0) {
            emit statusMessage(tr("Raccord : le rayon doit être positif ou nul."));
            return true;
        }
        filletRadiusMemo_ = std::min(value, 1.0e6);
        filletAwaitRadius_ = false;
    } else if (option == QLatin1String("R")) {
        filletAwaitRadius_ = true;
    } else if (option == QLatin1String("A") || option == QLatin1String("AJUSTER")) {
        filletTrim_ = !filletTrim_;
    } else if (option == QLatin1String("M") || option == QLatin1String("MULTIPLE")) {
        filletMultiple_ = !filletMultiple_;
    } else {
        return false;
    }
    notifyPrompt();
    update();
    return true;
}

QString Viewport::filletPrompt() const {
    if (filletAwaitRadius_) return tr("Raccord — tapez le rayon (actuel : %1, 0 = coin vif)").arg(filletRadiusMemo_);
    const QString state = tr("Raccord R=%1%2%3")
                              .arg(filletRadiusMemo_)
                              .arg(filletTrim_ ? QString() : tr(", sans ajuster"))
                              .arg(filletMultiple_ ? tr(", multiple") : QString());
    if (filletFirstId_ < 0)
        return tr("%1 — désignez le premier objet [R rayon, A ajuster, M multiple]").arg(state);
    return tr("%1 — désignez le second objet").arg(state);
}

bool Viewport::drawFilletPreview(QPainter& painter) {
    // Apres le premier objet : le raccord qui sera pose sur l'objet survole.
    if (filletFirstId_ < 0 || toolPoints_.empty() || !hoverWorld_ || !doc_) return true;
    geom::Entity* first = doc_->findEntity(filletFirstId_);
    geom::Entity* hover = pickEntityAt(*hoverWorld_);
    if (!first || !hover) return true;
    auto plan = detail::planFillet(first, toolPoints_[0], hover, *hoverWorld_, filletRadiusMemo_, filletTrim_);
    if (!plan) return true;
    for (const geom::Entity* e : {plan->n1.get(), plan->n2.get(), plan->joint.get()})
        if (e) drawGhost(painter, *e);
    return true;
}

} // namespace bcad::app
