// Les outils de repetition : reseau rectangulaire et hachures. Meme
// contrat que les outils de modification (ViewportModifyTools) : selection
// prealable, points dans `toolPoints_`, une macro d'annulation par
// operation, aucun dialogue.

#include "Viewport.h"

#include "Commands.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include <QUndoStack>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace bcad::app {

using geom::Point2;

void Viewport::applyArray(const Point2& world) {
    // Reseau rectangulaire 3x2 : p1 base, p2 donne le pas des colonnes
    // (p2-p1), p3 le pas des rangees (p3-p2). La selection est dupliquee sur
    // la grille, l'original en (0,0) — 5 clones par objet.
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 3) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets du réseau."));
        endCommand();
        return;
    }
    double sx = toolPoints_[1].x_ - toolPoints_[0].x_;
    double sy = toolPoints_[1].y_ - toolPoints_[0].y_;
    double rx = toolPoints_[2].x_ - toolPoints_[1].x_;
    double ry = toolPoints_[2].y_ - toolPoints_[1].y_;
    if (std::hypot(sx, sy) < geom::Tolerance::kDegenerateLength ||
        std::hypot(rx, ry) < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Réseau : pas trop petit."));
        endCommand();
        return;
    }
    if (undoStack_) undoStack_->beginMacro(tr("Réseau"));
    for (geom::Entity* e : selected) {
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 3; ++i) {
                if (i == 0 && j == 0) continue;
                auto clone = e->clone();
                clone->applyTransform(
                    geom::Transform2D::translation(i * sx + j * rx, i * sy + j * ry));
                clone->selected = false;
                if (undoStack_) {
                    undoStack_->push(new AddEntityCommand(doc_, std::move(clone), tr("Réseau")));
                } else {
                    doc_->addEntity(std::move(clone));
                }
            }
        }
    }
    if (undoStack_) undoStack_->endMacro();
    endCommand();
}

void Viewport::applyHatch(const Point2& /*world*/) {
    // Hachures a 45° des polylignes fermees selectionnees : balayage
    // horizontal dans le repere tourne, intersections avec les aretes
    // (regle pair-impair), segments retransformes. Pas = min(largeur,
    // hauteur)/25, borne pour rester lisible hors-ligne (ADR-016).
    std::vector<const geom::PolylineEntity*> bounds;
    for (geom::Entity* e : selectedEntities()) {
        if (e->typeId() != geom::TypeId_Polyline) continue;
        const auto& p = static_cast<const geom::PolylineEntity&>(*e);
        if (p.closed() && p.vertices().size() >= 3) bounds.push_back(&p);
    }
    if (bounds.empty()) {
        emit statusMessage(tr("Hachures : sélectionnez d'abord un contour fermé."));
        endCommand();
        return;
    }
    double angle = std::numbers::pi / 4.0;
    double ca = std::cos(-angle), sa = std::sin(-angle);
    auto rot = [&](const Point2& p) {
        return Point2(p.x_ * ca - p.y_ * sa, p.x_ * sa + p.y_ * ca);
    };
    auto unrot = [&](const Point2& p) {
        return Point2(p.x_ * ca + p.y_ * sa, -p.x_ * sa + p.y_ * ca);
    };
    if (undoStack_) undoStack_->beginMacro(tr("Hachures"));
    for (const auto* poly : bounds) {
        const auto& v = poly->vertices();
        std::vector<Point2> q;
        q.reserve(v.size());
        for (const auto& p : v) q.push_back(rot(p));
        double yMin = q[0].y_, yMax = q[0].y_, xMin = q[0].x_, xMax = q[0].x_;
        for (const auto& p : q) {
            yMin = std::min(yMin, p.y_);
            yMax = std::max(yMax, p.y_);
            xMin = std::min(xMin, p.x_);
            xMax = std::max(xMax, p.x_);
        }
        double step = std::min(xMax - xMin, yMax - yMin) / 25.0;
        step = std::clamp(step, 0.5, 50.0);
        std::size_t n = q.size();
        for (double y = yMin + step; y < yMax; y += step) {
            std::vector<double> xs;
            for (std::size_t i = 0; i < n; ++i) {
                const Point2& a = q[i];
                const Point2& b = q[(i + 1) % n];
                if ((a.y_ <= y && b.y_ > y) || (b.y_ <= y && a.y_ > y)) {
                    double t = (y - a.y_) / (b.y_ - a.y_);
                    xs.push_back(a.x_ + t * (b.x_ - a.x_));
                }
            }
            std::sort(xs.begin(), xs.end());
            for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
                if (xs[k + 1] - xs[k] < geom::Tolerance::kDegenerateLength) continue;
                auto seg = std::make_unique<geom::LineEntity>(unrot(Point2(xs[k], y)),
                                                             unrot(Point2(xs[k + 1], y)));
                seg->setLayer(poly->layer());
                seg->selected = false;
                if (undoStack_) {
                    undoStack_->push(new AddEntityCommand(doc_, std::move(seg), tr("Hachures")));
                } else {
                    doc_->addEntity(std::move(seg));
                }
            }
        }
    }
    if (undoStack_) undoStack_->endMacro();
    endCommand();
}

} // namespace bcad::app
