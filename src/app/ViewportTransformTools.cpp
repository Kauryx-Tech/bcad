// Les outils qui transforment la sélection existante : déplacement, copie,
// rotation, mise à l'échelle, symétrie. Comme les outils de tracé
// (ViewportDrawTools), chacun accumule ses points dans `toolPoints_` et ne
// commite qu'une forme complète ; la différence est qu'ils partent de ce qui
// est déjà dessiné, et que chaque opération passe par une macro d'annulation
// unique.

#include "Viewport.h"

#include "Commands.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Transform2D.h"
#include <QUndoStack>
#include <memory>

namespace bcad::app {

using geom::Point2;

void Viewport::applyMove(const Point2& world) {
    // Objet designe au clic (sans selection) : son point d'ancrage est le clic.
    if (moveTarget_ && moveAnchor_) {
        double dx = world.x_ - moveAnchor_->x_;
        double dy = world.y_ - moveAnchor_->y_;
        auto transform = geom::Transform2D::translation(dx, dy);
        if (undoStack_) {
            undoStack_->push(new TransformEntityCommand(doc_, moveTarget_, transform, tr("Déplacer")));
        } else {
            moveTarget_->applyTransform(transform);
            doc_->notifyEntityChanged(moveTarget_);
        }
        moveTarget_ = nullptr;
        moveAnchor_.reset();
        return;
    }
    // Selection : point de base puis destination, saisis au clic ou au clavier.
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets à déplacer, ou désignez-en un."));
        return;
    }
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 2) {
        auto transform = geom::Transform2D::translation(toolPoints_[1].x_ - toolPoints_[0].x_,
                                                        toolPoints_[1].y_ - toolPoints_[0].y_);
        if (undoStack_) undoStack_->beginMacro(tr("Déplacer"));
        for (geom::Entity* e : selected) {
            if (undoStack_) {
                undoStack_->push(new TransformEntityCommand(doc_, e, transform, tr("Déplacer")));
            } else {
                e->applyTransform(transform);
                doc_->notifyEntityChanged(e);
            }
        }
        if (undoStack_) undoStack_->endMacro();
        toolPoints_.clear();
    }
}

void Viewport::applyCopy(const Point2& world) {
    // Point de base, puis point de destination — opère sur la
    // sélection actuelle (faite au préalable avec Sélection), en
    // clonant plutôt qu'en déplaçant sur place : la commande COPY
    // d'AutoCAD crée toujours une nouvelle géométrie.
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 2) {
        std::vector<geom::Entity*> selected = selectedEntities();
        if (selected.empty()) {
            emit statusMessage(tr("Sélectionnez d'abord les objets à copier."));
        } else {
            double dx = toolPoints_[1].x_ - toolPoints_[0].x_;
            double dy = toolPoints_[1].y_ - toolPoints_[0].y_;
            auto transform = geom::Transform2D::translation(dx, dy);
            if (undoStack_) undoStack_->beginMacro(tr("Copier"));
            for (geom::Entity* e : selected) {
                auto clone = e->clone();
                clone->applyTransform(transform);
                clone->selected = false;
                commitEntity(std::move(clone), tr("Copier"));
            }
            if (undoStack_) undoStack_->endMacro();
        }
        toolPoints_.clear();
    }
}

void Viewport::applyRotate(const Point2& world) {
    // Pivot, puis un point de référence, puis un point cible —
    // la rotation appliquée est l'angle *entre* pivot->référence
    // et pivot->cible, pas un angle absolu, donc elle tourne par
    // rapport à l'orientation actuelle de la sélection (correspond
    // à la commande ROTATE d'AutoCAD avec un angle de référence
    // pointé).
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 3) {
        std::vector<geom::Entity*> selected = selectedEntities();
        if (selected.empty()) {
            emit statusMessage(tr("Sélectionnez d'abord les objets à tourner."));
        } else {
            const Point2& pivot = toolPoints_[0];
            double refAngle = geom::angleOf(pivot, toolPoints_[1]);
            double targetAngle = geom::angleOf(pivot, toolPoints_[2]);
            auto transform = geom::Transform2D::rotation(targetAngle - refAngle, pivot);
            if (undoStack_) undoStack_->beginMacro(tr("Tourner"));
            for (geom::Entity* e : selected) {
                if (undoStack_) {
                    undoStack_->push(new TransformEntityCommand(doc_, e, transform, tr("Tourner")));
                } else {
                    e->applyTransform(transform);
                    doc_->notifyEntityChanged(e);
                }
            }
            if (undoStack_) undoStack_->endMacro();
        }
        toolPoints_.clear();
    }
}

void Viewport::applyScale(const Point2& world) {
    // Point de base, point de référence, point cible — le facteur
    // d'échelle est le rapport des deux distances depuis le point
    // de base, donc relatif à la taille actuelle de la sélection
    // (correspond à la commande SCALE d'AutoCAD avec une longueur
    // de référence pointée).
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 3) {
        std::vector<geom::Entity*> selected = selectedEntities();
        const Point2& base = toolPoints_[0];
        double refDist = geom::distance(base, toolPoints_[1]);
        if (selected.empty()) {
            emit statusMessage(tr("Sélectionnez d'abord les objets à mettre à l'échelle."));
        } else if (refDist < geom::Tolerance::kDegenerateLength) {
            emit statusMessage(tr("Longueur de référence trop petite."));
        } else {
            double targetDist = geom::distance(base, toolPoints_[2]);
            double factor = targetDist / refDist;
            auto transform = geom::Transform2D::scaling(factor, base);
            if (undoStack_) undoStack_->beginMacro(tr("Échelle"));
            for (geom::Entity* e : selected) {
                if (undoStack_) {
                    undoStack_->push(new TransformEntityCommand(doc_, e, transform, tr("Échelle")));
                } else {
                    e->applyTransform(transform);
                    doc_->notifyEntityChanged(e);
                }
            }
            if (undoStack_) undoStack_->endMacro();
        }
        toolPoints_.clear();
    }
}

void Viewport::applyMirror(const Point2& world) {
    // Deux points définissent la ligne de symétrie. Non destructif
    // par défaut (clone + transforme, garde les originaux) —
    // correspond au comportement par défaut "effacer les objets
    // source ? Non" de la commande MIRROR d'AutoCAD.
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 2) {
        std::vector<geom::Entity*> selected = selectedEntities();
        if (selected.empty()) {
            emit statusMessage(tr("Sélectionnez d'abord les objets à symétriser."));
        } else {
            auto transform = geom::Transform2D::mirrorAcrossLine(toolPoints_[0], toolPoints_[1]);
            if (undoStack_) undoStack_->beginMacro(tr("Symétrie"));
            for (geom::Entity* e : selected) {
                auto clone = e->clone();
                clone->applyTransform(transform);
                clone->selected = false;
                commitEntity(std::move(clone), tr("Symétrie"));
            }
            if (undoStack_) undoStack_->endMacro();
        }
        toolPoints_.clear();
    }
}

} // namespace bcad::app
