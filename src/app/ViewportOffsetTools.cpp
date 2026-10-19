// Decaler (M-01), comme DECALER / OFFSET d'AutoCAD :
//   1. la distance se tape (nombre), ou « P » : decaler par un point de
//      passage — la distance vient alors du point ;
//   2. on designe l'objet a decaler (au clic, sans selection prealable) ;
//   3. on clique du cote du decalage (ou le point de passage).
// Puis on recommence en 2 jusqu'a Entree. Options : « E » efface la source,
// « C » pose sur le calque courant (sinon celui de la source), « M » enchaine
// les decalages depuis le dernier objet cree. Distance et options sont
// gardees d'une commande a l'autre. Geometrie : detail::offsetClone.

#include "Viewport.h"

#include "Commands.h"
#include "EditBatch.h"
#include "ModifyDetail.h"
#include "ViewportTolerances.h"

#include "bcad/core/Document.h"

#include <QPainter>
#include <QUndoStack>

#include <algorithm>
#include <memory>

namespace bcad::app {

using geom::Point2;

namespace {

bool offsetable(const geom::Entity* e) {
    return e->typeId() == geom::TypeId_Line || e->typeId() == geom::TypeId_Circle ||
           e->typeId() == geom::TypeId_Arc || e->typeId() == geom::TypeId_Polyline;
}

} // namespace

void Viewport::applyOffset(const Point2& world) {
    if (!offsetThrough_ && offsetDistMemo_ <= 0.0) {
        emit statusMessage(tr("Décaler : tapez d'abord la distance (ou P pour un point de passage)."));
        return;
    }
    geom::Entity* source = offsetObjectId_ >= 0 ? doc_->findEntity(offsetObjectId_) : nullptr;
    if (!source) {
        geom::Entity* hit = pickEntityAt(world);
        if (!hit || !offsetable(hit)) {
            emit statusMessage(tr("Décaler : désignez une ligne, un cercle, un arc ou une polyligne."));
            return;
        }
        offsetObjectId_ = hit->id();
        return;
    }
    const double d = offsetThrough_ ? source->distanceTo(world) : offsetDistMemo_;
    if (d < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Décaler : distance nulle."));
        return;
    }
    std::unique_ptr<geom::Entity> out = detail::offsetClone(source, d, world);
    if (!out) {
        emit statusMessage(tr("Décaler : impossible de ce côté (rayon négatif ou objet dégénéré)."));
        return;
    }
    if (offsetCurrentLayer_) {
        out->setLayer(doc_->layerManager().currentLayerName());
    } else {
        out->setLayer(source->layer());
        if (source->colorOverride()) out->setColorOverride(source->colorOverride());
    }
    out->selected = false;
    {
        EditBatch batch(doc_, undoStack_, tr("Décaler"));
        batch.add(std::move(out));
        if (offsetErase_) batch.remove(source);
    }
    // Multiple : on repart de l'objet qui vient d'etre cree (le dernier ajoute).
    offsetObjectId_ = offsetMultiple_ && !doc_->entities().empty() ? doc_->entities().back()->id() : -1;
}

bool Viewport::submitOffsetOption(const QString& text) {
    if (tool_ != ToolMode::Offset) return false;
    const QString option = text.trimmed().toUpper();
    if (option.isEmpty()) return false;
    bool ok = false;
    const double value = option.toDouble(&ok);
    if (ok) {
        if (value <= 0.0) {
            emit statusMessage(tr("Décaler : la distance doit être positive."));
            return true;
        }
        offsetDistMemo_ = std::min(value, 1.0e7);
        offsetThrough_ = false;
    } else if (option == QLatin1String("P") || option == QLatin1String("PAR")) {
        offsetThrough_ = true;
    } else if (option == QLatin1String("E") || option == QLatin1String("EFFACER")) {
        offsetErase_ = !offsetErase_;
    } else if (option == QLatin1String("C") || option == QLatin1String("CALQUE")) {
        offsetCurrentLayer_ = !offsetCurrentLayer_;
    } else if (option == QLatin1String("M") || option == QLatin1String("MULTIPLE")) {
        offsetMultiple_ = !offsetMultiple_;
    } else {
        return false;
    }
    notifyPrompt();
    update();
    return true;
}

QString Viewport::offsetPrompt() const {
    if (!offsetThrough_ && offsetDistMemo_ <= 0.0)
        return tr("Décaler — tapez la distance [P par un point de passage]");
    QString state = offsetThrough_ ? tr("Décaler par un point") : tr("Décaler D=%1").arg(offsetDistMemo_);
    if (offsetErase_) state += tr(", source effacée");
    if (offsetCurrentLayer_) state += tr(", calque courant");
    if (offsetMultiple_) state += tr(", multiple");
    if (offsetObjectId_ < 0)
        return tr("%1 — désignez l'objet, Entrée pour terminer [nombre, P, E effacer, C calque, M multiple]")
            .arg(state);
    return offsetThrough_ ? tr("%1 — point de passage").arg(state)
                          : tr("%1 — cliquez du côté du décalage").arg(state);
}

bool Viewport::drawOffsetPreview(QPainter& painter) {
    // L'objet qui sera pose, sous le curseur, une fois la source designee.
    if (offsetObjectId_ < 0 || !hoverWorld_ || !doc_) return true;
    geom::Entity* source = doc_->findEntity(offsetObjectId_);
    if (!source) return true;
    const double d = offsetThrough_ ? source->distanceTo(*hoverWorld_) : offsetDistMemo_;
    if (d < geom::Tolerance::kDegenerateLength) return true;
    auto ghost = detail::offsetClone(source, d, *hoverWorld_);
    if (!ghost) return true;
    drawGhost(painter, *ghost);
    return true;
}

} // namespace bcad::app
