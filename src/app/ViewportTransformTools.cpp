// Les outils qui transforment la sélection existante : déplacement, copie,
// rotation, mise à l'échelle, symétrie, étirement. Comme les outils de tracé
// (ViewportDrawTools), chacun accumule ses points dans `toolPoints_` et ne
// commite qu'une forme complète ; la différence est qu'ils partent de ce qui
// est déjà dessiné, et que chaque opération passe par une macro d'annulation
// unique.

#include "Viewport.h"

#include "Commands.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Transform2D.h"
#include <QUndoStack>
#include <algorithm>
#include <memory>

namespace bcad::app {

using geom::Point2;

namespace {

// Sommet dans la fenetre (coins w1, w2), tolerance comprise.
bool inRect(const Point2& p, double x0, double y0, double x1, double y1) {
    double tol = geom::Tolerance::kDegenerateLength;
    return p.x_ >= std::min(x0, x1) - tol && p.x_ <= std::max(x0, x1) + tol &&
           p.y_ >= std::min(y0, y1) - tol && p.y_ <= std::max(y0, y1) + tol;
}

} // namespace


void Viewport::applyMove(const Point2& world) {
    // Selection : point de base puis destination, saisis au clic ou au clavier.
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets à déplacer."));
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
        endCommand();
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
        endCommand();
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
        endCommand();
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
        endCommand();
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
        endCommand();
    }
}

void Viewport::applyStretch(const Point2& world) {
    // Trois points : deux coins de fenetre, puis la cible du deplacement
    // (vecteur = p3-p2). Les sommets des lignes et polylignes selectionnees
    // qui tombent dans la fenetre suivent le vecteur ; les centres de
    // cercles/arcs dans la fenetre deplacent l'objet entier,
    // comme tout autre type dont le centre de boite y figure (point, texte, cotations).
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 3) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets à étirer."));
        endCommand();
        return;
    }
    const Point2& w1 = toolPoints_[0];
    const Point2& w2 = toolPoints_[1];
    double dx = toolPoints_[2].x_ - w2.x_, dy = toolPoints_[2].y_ - w2.y_;
    if (std::hypot(dx, dy) < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Vecteur d'étirement trop petit."));
        endCommand();
        return;
    }
    bool done = false;
    if (undoStack_) undoStack_->beginMacro(tr("Étirer"));
    for (geom::Entity* e : selected) {
        std::unique_ptr<geom::Entity> out;
        if (e->typeId() == geom::TypeId_Line) {
            const auto& l = static_cast<const geom::LineEntity&>(*e);
            Point2 a = l.start(), b = l.end();
            bool ma = inRect(a, w1.x_, w1.y_, w2.x_, w2.y_);
            bool mb = inRect(b, w1.x_, w1.y_, w2.x_, w2.y_);
            if (!ma && !mb) continue;
            if (ma) { a.x_ += dx; a.y_ += dy; }
            if (mb) { b.x_ += dx; b.y_ += dy; }
            out = std::make_unique<geom::LineEntity>(a, b);
        } else if (e->typeId() == geom::TypeId_Polyline) {
            const auto& p = static_cast<const geom::PolylineEntity&>(*e);
            std::vector<Point2> nv = p.vertices();
            bool any = false;
            for (auto& v : nv) {
                if (inRect(v, w1.x_, w1.y_, w2.x_, w2.y_)) {
                    v.x_ += dx;
                    v.y_ += dy;
                    any = true;
                }
            }
            if (!any) continue;
            out = std::make_unique<geom::PolylineEntity>(nv, p.closed());
        } else if (e->typeId() == geom::TypeId_Circle || e->typeId() == geom::TypeId_Arc) {
            Point2 c = (e->typeId() == geom::TypeId_Circle)
                           ? static_cast<const geom::CircleEntity&>(*e).center()
                           : static_cast<const geom::ArcEntity&>(*e).center();
            if (!inRect(c, w1.x_, w1.y_, w2.x_, w2.y_)) continue;
            out = e->clone();
            out->applyTransform(geom::Transform2D::translation(dx, dy));
        } else {
            // Autres types (point, texte, cotations…) : l'objet suit le
            // vecteur si le centre de sa boite est dans la fenetre.
            geom::BoundingBox bb = e->boundingBox();
            Point2 center((bb.minX + bb.maxX) / 2.0, (bb.minY + bb.maxY) / 2.0);
            if (!inRect(center, w1.x_, w1.y_, w2.x_, w2.y_)) continue;
            out = e->clone();
            out->applyTransform(geom::Transform2D::translation(dx, dy));
        }
        out->setLayer(e->layer());
        out->selected = true;
        if (undoStack_) {
            undoStack_->push(new RemoveEntityCommand(doc_, e, tr("Étirer")));
            undoStack_->push(new AddEntityCommand(doc_, std::move(out), tr("Étirer")));
        } else {
            doc_->removeEntity(e->id());
            doc_->addEntity(std::move(out));
        }
        done = true;
    }
    if (undoStack_) undoStack_->endMacro();
    if (!done) emit statusMessage(tr("Étirer : aucun sommet dans la fenêtre."));
    endCommand();
}

} // namespace bcad::app
