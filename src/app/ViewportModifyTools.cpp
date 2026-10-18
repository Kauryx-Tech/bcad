// Les outils de modification geometrique : decalage et chanfrein (lignes
// et arcs). L'etirement vit avec les transformations (ViewportTransformTools),
// le reseau et les hachures avec la repetition (ViewportPatternTools).
// Meme contrat : selection prealable, points dans `toolPoints_`, une macro
// d'annulation par operation, aucun dialogue.

#include "Viewport.h"

#include "Commands.h"
#include "ModifyDetail.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include <QUndoStack>
#include <algorithm>
#include <memory>
#include <memory>
#include <numbers>

namespace bcad::app {

using geom::Point2;

void Viewport::applyOffset(const Point2& world) {
    // Deux points definissent la distance de decalage (p2-p1) ; le cote vient
    // de la position de p2 par rapport a chaque entite (detail::offsetClone).
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 2) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets à décaler."));
        endCommand();
        return;
    }
    double d = geom::distance(toolPoints_[0], toolPoints_[1]);
    if (d < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Distance de décalage trop petite."));
        endCommand();
        return;
    }
    const Point2& side = toolPoints_[1];
    bool done = false;
    if (undoStack_) undoStack_->beginMacro(tr("Décaler"));
    for (geom::Entity* e : selected) {
        std::unique_ptr<geom::Entity> out = detail::offsetClone(e, d, side);
        if (!out) continue;
        out->setLayer(e->layer());
        out->selected = false;
        if (undoStack_) undoStack_->push(new AddEntityCommand(doc_, std::move(out), tr("Décaler")));
        else doc_->addEntity(std::move(out));
        done = true;
    }
    if (undoStack_) undoStack_->endMacro();
    if (!done) emit statusMessage(tr("Décaler : type d'objet non pris en charge."));
    endCommand();
}

void Viewport::applyChamfer(const Point2& world) {
    // Deux points designent les distances de chanfrein : d1 = distance du
    // premier point a l'intersection des supports des deux premieres
    // entites selectionnees (lignes, arcs ou polylignes — le segment le
    // plus proche du point designe), d2 de meme pour le second. Les
    // entites sont raccourcies et un segment de chanfrein les relie —
    // comportement de CHAMFER sans dialogue. Les cercles complets sont
    // refuses : on ne raccourcit pas un contour ferme.
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 2) return;
    std::vector<geom::Entity*> prims;
    for (geom::Entity* e : selectedEntities()) {
        if (e->typeId() == geom::TypeId_Line || e->typeId() == geom::TypeId_Arc ||
            e->typeId() == geom::TypeId_Polyline) {
            prims.push_back(e);
        }
    }
    if (prims.size() < 2) {
        emit statusMessage(tr("Chanfrein : sélectionnez d'abord deux lignes, arcs ou polylignes."));
        endCommand();
        return;
    }
    detail::ChamferSupport s1 = detail::chamferSupport(prims[0], toolPoints_[0]);
    detail::ChamferSupport s2 = detail::chamferSupport(prims[1], toolPoints_[1]);
    std::vector<Point2> candidates = detail::supportIntersections(s1, s2);
    if (candidates.empty()) {
        emit statusMessage(tr("Chanfrein : supports sans intersection."));
        endCommand();
        return;
    }
    Point2 q1 = detail::projectOnSupport(s1, toolPoints_[0]);
    Point2 q2 = detail::projectOnSupport(s2, toolPoints_[1]);
    Point2 mid((q1.x_ + q2.x_) / 2.0, (q1.y_ + q2.y_) / 2.0);
    Point2 inter = *std::min_element(candidates.begin(), candidates.end(),
                                     [&](const Point2& a, const Point2& b) {
                                         return geom::distance(a, mid) < geom::distance(b, mid);
                                     });
    double d1 = geom::distance(q1, inter), d2 = geom::distance(q2, inter);
    if (!chamferFillet_ && chamferDist_ > 0.0) d1 = d2 = chamferDist_;
    if (d1 < geom::Tolerance::kDegenerateLength || d2 < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Chanfrein : distances trop petites."));
        endCommand();
        return;
    }
    Point2 t1 = detail::trimOnSupport(s1, inter, q1, d1);
    Point2 t2 = detail::trimOnSupport(s2, inter, q2, d2);
    // Objet de raccord : segment droit, ou arc de conge de rayon tape
    // (lignes, segments de polyligne, arcs).
    std::unique_ptr<geom::Entity> joint;
    QString label = tr("Chanfrein");
    if (chamferFillet_) {
        auto fillet = detail::filletBetween(s1, s2, inter, q1, q2, filletRadius_);
        if (!fillet) {
            emit statusMessage(tr("Congé : tapez un rayon compatible avec les deux objets."));
            endCommand();
            return;
        }
        t1 = fillet->t1;
        t2 = fillet->t2;
        if (fillet->ccw) {
            joint = std::make_unique<geom::ArcEntity>(fillet->c, filletRadius_, fillet->a1, fillet->a2);
        } else {
            joint = std::make_unique<geom::ArcEntity>(fillet->c, filletRadius_, fillet->a2, fillet->a1);
        }
        label = tr("Congé");
    } else {
        joint = std::make_unique<geom::LineEntity>(t1, t2);
    }
    std::unique_ptr<geom::Entity> n1 = detail::trimKeeping(prims[0], s1, t1, q1);
    std::unique_ptr<geom::Entity> n2 = detail::trimKeeping(prims[1], s2, t2, q2);
    n1->setLayer(prims[0]->layer());
    n2->setLayer(prims[1]->layer());
    n1->selected = n2->selected = false;
    joint->setLayer(prims[0]->layer());
    if (undoStack_) {
        undoStack_->beginMacro(label);
        undoStack_->push(new RemoveEntityCommand(doc_, prims[0], label));
        undoStack_->push(new RemoveEntityCommand(doc_, prims[1], label));
        undoStack_->push(new AddEntityCommand(doc_, std::move(n1), label));
        undoStack_->push(new AddEntityCommand(doc_, std::move(n2), label));
        undoStack_->push(new AddEntityCommand(doc_, std::move(joint), label));
        undoStack_->endMacro();
    } else {
        doc_->removeEntity(prims[0]->id());
        doc_->removeEntity(prims[1]->id());
        doc_->addEntity(std::move(n1));
        doc_->addEntity(std::move(n2));
        doc_->addEntity(std::move(joint));
    }
    endCommand();
}

} // namespace bcad::app
