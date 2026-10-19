// Les outils qui transforment la sélection existante : déplacement, copie,
// rotation, mise à l'échelle, symétrie, étirement. Comme les outils de tracé
// (ViewportDrawTools), chacun accumule ses points dans `toolPoints_` et ne
// commite qu'une forme complète ; la différence est qu'ils partent de ce qui
// est déjà dessiné, et que chaque opération passe par une macro d'annulation
// unique.

#include "viewport/Viewport.h"

#include "editing/Commands.h"
#include "editing/EditBatch.h"
#include "detail/ModifyDetail.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Transform2D.h"
#include <QUndoStack>
#include <algorithm>
#include <memory>
#include <numbers>

namespace bcad::app {

using geom::Point2;


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
        {
            EditBatch batch(doc_, undoStack_, tr("Déplacer"));
            for (geom::Entity* e : selected) batch.transform(e, transform);
        }
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
            EditBatch batch(doc_, undoStack_, tr("Copier"));
            for (geom::Entity* e : selected) {
                auto clone = e->clone();
                clone->applyTransform(transform);
                clone->selected = false;
                batch.add(std::move(clone));   // la copie garde le calque de l'original
            }
        }
        endCommand();
    }
}

void Viewport::transformSelection(const geom::Transform2D& transform, const QString& label) {
    // Commune a Tourner et Echelle : en place, ou (option C) sur des copies
    // posees a cote des originaux, comme l'option Copie d'AutoCAD.
    EditBatch batch(doc_, undoStack_, label);
    for (geom::Entity* e : selectedEntities()) {
        if (!transformCopy_) {
            batch.transform(e, transform);
            continue;
        }
        std::unique_ptr<geom::Entity> copy = e->clone();
        copy->applyTransform(transform);
        copy->selected = false;
        batch.add(std::move(copy));
    }
}

void Viewport::applyRotate(const Point2& world) {
    // Pivot, puis un point de reference, puis un point cible : la rotation
    // est l'angle entre pivot->reference et pivot->cible (option Reference
    // d'AutoCAD, pointee). Les valeurs tapees (submitTransformValue) prennent
    // la place du deuxieme ou du troisieme point.
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 3) {
        if (selectedEntities().empty()) {
            emit statusMessage(tr("Sélectionnez d'abord les objets à tourner."));
        } else {
            const Point2& pivot = toolPoints_[0];
            double refAngle = geom::angleOf(pivot, toolPoints_[1]);
            double targetAngle = geom::angleOf(pivot, toolPoints_[2]);
            transformSelection(geom::Transform2D::rotation(targetAngle - refAngle, pivot), tr("Tourner"));
        }
        endCommand();
    }
}

void Viewport::applyScale(const Point2& world) {
    // Point de base, point de reference, point cible : le facteur est le
    // rapport des deux distances depuis la base (option Reference d'AutoCAD,
    // pointee). Les valeurs tapees remplacent le deuxieme ou le troisieme.
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 3) {
        const Point2& base = toolPoints_[0];
        double refDist = geom::distance(base, toolPoints_[1]);
        if (selectedEntities().empty()) {
            emit statusMessage(tr("Sélectionnez d'abord les objets à mettre à l'échelle."));
        } else if (refDist < geom::Tolerance::kDegenerateLength) {
            emit statusMessage(tr("Longueur de référence trop petite."));
        } else {
            double factor = geom::distance(base, toolPoints_[2]) / refDist;
            transformSelection(geom::Transform2D::scaling(factor, base), tr("Échelle"));
        }
        endCommand();
    }
}

bool Viewport::submitTransformValue(const QString& text) {
    // Tourner / Echelle par valeur (M-05) : apres le point de base, un angle
    // en degres (ou un facteur) s'applique tout de suite ; apres un point de
    // reference, il donne le nouvel angle absolu (ou la nouvelle longueur).
    // « C » bascule la copie a tout moment de la commande.
    if ((tool_ != ToolMode::Rotate && tool_ != ToolMode::Scale) || pickingObjects_) return false;
    const QString option = text.trimmed().toUpper();
    if (option == QLatin1String("C") || option == QLatin1String("COPIE")) {
        transformCopy_ = !transformCopy_;
        notifyPrompt();
        return true;
    }
    if (toolPoints_.empty()) return false;
    bool ok = false;
    const double value = QString(option).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    if (!ok || option.contains(QLatin1Char(','))) return false; // « x,y » reste une coordonnee
    if (selectedEntities().empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets."));
        endCommand();
        return true;
    }
    const Point2 base = toolPoints_[0];
    if (tool_ == ToolMode::Rotate) {
        const double rad = value * std::numbers::pi / 180.0;
        const double angle = toolPoints_.size() >= 2 ? rad - geom::angleOf(base, toolPoints_[1]) : rad;
        transformSelection(geom::Transform2D::rotation(angle, base), tr("Tourner"));
    } else {
        double factor = value;
        if (toolPoints_.size() >= 2) {
            const double refDist = geom::distance(base, toolPoints_[1]);
            if (refDist < geom::Tolerance::kDegenerateLength) {
                emit statusMessage(tr("Longueur de référence trop petite."));
                return true;
            }
            factor = value / refDist;
        }
        if (factor <= 0.0) {
            emit statusMessage(tr("Échelle : le facteur doit être positif."));
            return true;
        }
        transformSelection(geom::Transform2D::scaling(factor, base), tr("Échelle"));
    }
    endCommand();
    update();
    return true;
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
            EditBatch batch(doc_, undoStack_, tr("Miroir"));
            for (geom::Entity* e : selected) {
                auto clone = e->clone();
                clone->applyTransform(transform);
                clone->selected = false;
                batch.add(std::move(clone));   // la copie garde le calque de l'original
            }
        }
        endCommand();
    }
}

void Viewport::applyStretch(const Point2& world) {
    // Comme ETIRER d'AutoCAD : deux coins de fenetre de capture, puis point
    // de base et destination. La fenetre designe aussi les objets (tout ce
    // qu'elle touche) ; une selection prealable restreint l'etirement a
    // elle. Ce qui bouge : detail::stretchClone.
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 4) return;
    const Point2 w1 = toolPoints_[0], w2 = toolPoints_[1];
    const double dx = toolPoints_[3].x_ - toolPoints_[2].x_;
    const double dy = toolPoints_[3].y_ - toolPoints_[2].y_;
    if (std::hypot(dx, dy) < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Étirer : déplacement nul."));
        endCommand();
        return;
    }
    std::vector<geom::Entity*> targets = stretchTargets();
    bool done = false;
    {
        EditBatch batch(doc_, undoStack_, tr("Étirer"));
        for (geom::Entity* e : targets) {
            std::unique_ptr<geom::Entity> out = detail::stretchClone(e, w1, w2, dx, dy);
            if (!out) continue;
            out->selected = false;
            batch.replace(e, std::move(out));
            done = true;
        }
    }
    if (!done) emit statusMessage(tr("Étirer : aucun sommet dans la fenêtre."));
    endCommand();
}

std::vector<geom::Entity*> Viewport::stretchTargets() const {
    // Selection prealable, sinon tout ce que la fenetre touche (capture).
    std::vector<geom::Entity*> selected = selectedEntities();
    if (!selected.empty() || !doc_ || toolPoints_.size() < 2) return selected;
    const Point2& w1 = toolPoints_[0];
    const Point2& w2 = toolPoints_[1];
    geom::BoundingBox box{std::min(w1.x_, w2.x_), std::min(w1.y_, w2.y_), std::max(w1.x_, w2.x_),
                          std::max(w1.y_, w2.y_)};
    return doc_->entitiesInRegion(box);
}

} // namespace bcad::app
