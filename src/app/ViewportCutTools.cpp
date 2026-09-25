// Les outils qui coupent une entité désignée par un clic : rognage jusqu'à
// l'intersection la plus proche, prolongement jusqu'à l'obstacle le plus
// proche, rupture en un point. Les trois partent d'une entité pointée et la
// remplacent par sa version modifiée — toujours une seule macro d'annulation,
// pour qu'un Ctrl+Z remise la ligne d'origine telle quelle.

#include "Viewport.h"

#include "Commands.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/SnapGeometry.h"
#include <QMessageBox>
#include <QUndoStack>
#include <cmath>
#include <limits>
#include <memory>

namespace bcad::app {

using geom::Point2;

void Viewport::applyTrim(const Point2& world) {
    // Lignes uniquement pour l'instant (§2.18) : cliquez près de
    // l'extrémité que vous voulez couper jusqu'à la plus proche
    // intersection réelle avec autre chose dans le document — le
    // flux de travail "couper contre tout" qu'offre AutoCAD quand
    // on ne pointe pas d'arêtes de coupe explicites.
    geom::Entity* hit = pickModifiableEntity(world);
    if (!hit) return;
    if (hit->typeId() != geom::TypeId_Line) {
        QMessageBox::information(this, tr("Trim"), tr("Trim currently supports lines only."));
        return;
    }
    auto* line = static_cast<geom::LineEntity*>(hit);
    Point2 a = line->start(), b = line->end();
    geom::Vector2 dir = b - a;
    double lenSq = geom::squaredLength(dir);
    if (lenSq < geom::Tolerance::kDegenerateLength) return;

    std::vector<double> ts;
    for (const auto& e : doc_->entities()) {
        if (e.get() == hit) continue;
        for (const Point2& p : geom::entityIntersections(*line, *e)) {
            double t = geom::dot(p - a, dir) / lenSq;
            if (t > 1e-9 && t < 1.0 - 1e-9) ts.push_back(t);
        }
    }
    if (ts.empty()) {
        QMessageBox::information(this, tr("Trim"), tr("No cutting edge found."));
        return;
    }
    double clickT = geom::dot(world - a, dir) / lenSq;
    double bestT = ts.front();
    for (double t : ts) {
        if (std::abs(t - clickT) < std::abs(bestT - clickT)) bestT = t;
    }
    Point2 cutPoint(a.x_ + bestT * dir.x_, a.y_ + bestT * dir.y_);
    // Conserve le côté sur lequel l'utilisateur n'a PAS cliqué.
    Point2 newStart = clickT < bestT ? cutPoint : a;
    Point2 newEnd = clickT < bestT ? b : cutPoint;
    auto trimmed = std::make_unique<geom::LineEntity>(newStart, newEnd);
    trimmed->setLayer(line->layer());
    if (line->colorOverride()) trimmed->setColorOverride(line->colorOverride());
    if (undoStack_) {
        undoStack_->beginMacro(tr("Trim"));
        undoStack_->push(new RemoveEntityCommand(doc_, hit, tr("Trim")));
        undoStack_->push(new AddEntityCommand(doc_, std::move(trimmed), tr("Trim")));
        undoStack_->endMacro();
    } else {
        doc_->removeEntity(hit->id());
        doc_->addEntity(std::move(trimmed));
    }
}

void Viewport::applyExtend(const Point2& world) {
    // Lignes uniquement pour l'instant (§2.18) : prolonge
    // l'extrémité la plus proche du clic, jusqu'à la plus proche
    // entité que croiserait le prolongement infini de la ligne.
    // Implémenté en sondant les intersections avec un très long
    // segment de substitution plutôt qu'en ajoutant une routine
    // d'intersection de droite infinie dédiée.
    geom::Entity* hit = pickModifiableEntity(world);
    if (!hit) return;
    if (hit->typeId() != geom::TypeId_Line) {
        QMessageBox::information(this, tr("Extend"), tr("Extend currently supports lines only."));
        return;
    }
    auto* line = static_cast<geom::LineEntity*>(hit);
    Point2 a = line->start(), b = line->end();
    geom::Vector2 dir = b - a;
    double lenSq = geom::squaredLength(dir);
    if (lenSq < geom::Tolerance::kDegenerateLength) return;

    bool extendFromB = geom::distance(world, b) <= geom::distance(world, a);
    constexpr double kExtendFactor = 1e5; // pratiquement illimité pour tout dessin réaliste
    Point2 farA = extendFromB ? a
                               : Point2(a.x_ - kExtendFactor * dir.x_, a.y_ - kExtendFactor * dir.y_);
    Point2 farB = extendFromB
                      ? Point2(b.x_ + kExtendFactor * dir.x_, b.y_ + kExtendFactor * dir.y_)
                      : b;
    geom::LineEntity probe(farA, farB);

    std::optional<double> bestT;
    for (const auto& e : doc_->entities()) {
        if (e.get() == hit) continue;
        for (const Point2& p : geom::entityIntersections(probe, *e)) {
            double t = geom::dot(p - a, dir) / lenSq;
            bool beyond = extendFromB ? (t > 1.0 + 1e-9) : (t < -1e-9);
            if (!beyond) continue;
            if (!bestT || (extendFromB ? t < *bestT : t > *bestT)) bestT = t;
        }
    }
    if (!bestT) {
        QMessageBox::information(this, tr("Extend"), tr("Nothing found to extend to."));
        return;
    }
    Point2 newPoint(a.x_ + (*bestT) * dir.x_, a.y_ + (*bestT) * dir.y_);
    Point2 newStart = extendFromB ? a : newPoint;
    Point2 newEnd = extendFromB ? newPoint : b;
    auto extended = std::make_unique<geom::LineEntity>(newStart, newEnd);
    extended->setLayer(line->layer());
    if (line->colorOverride()) extended->setColorOverride(line->colorOverride());
    if (undoStack_) {
        undoStack_->beginMacro(tr("Extend"));
        undoStack_->push(new RemoveEntityCommand(doc_, hit, tr("Extend")));
        undoStack_->push(new AddEntityCommand(doc_, std::move(extended), tr("Extend")));
        undoStack_->endMacro();
    } else {
        doc_->removeEntity(hit->id());
        doc_->addEntity(std::move(extended));
    }
}

void Viewport::applyBreak(const Point2& world) {
    // Lignes et polylignes ouvertes (§2.18) — les polylignes
    // fermées nécessitent deux points de coupure dans les vrais
    // outils CAO (la boucle reste une boucle), ce que ce MVP à
    // clic unique ne tente pas encore.
    geom::Entity* hit = pickModifiableEntity(world);
    if (!hit) return;

    if (hit->typeId() == geom::TypeId_Line) {
        auto* line = static_cast<geom::LineEntity*>(hit);
        Point2 breakPoint = geom::closestPointOnSegment(world, line->start(), line->end());
        auto part1 = std::make_unique<geom::LineEntity>(line->start(), breakPoint);
        auto part2 = std::make_unique<geom::LineEntity>(breakPoint, line->end());
        part1->setLayer(line->layer());
        part2->setLayer(line->layer());
        if (line->colorOverride()) {
            part1->setColorOverride(line->colorOverride());
            part2->setColorOverride(line->colorOverride());
        }
        if (undoStack_) {
            undoStack_->beginMacro(tr("Break"));
            undoStack_->push(new RemoveEntityCommand(doc_, hit, tr("Break")));
            undoStack_->push(new AddEntityCommand(doc_, std::move(part1), tr("Break")));
            undoStack_->push(new AddEntityCommand(doc_, std::move(part2), tr("Break")));
            undoStack_->endMacro();
        } else {
            doc_->removeEntity(hit->id());
            doc_->addEntity(std::move(part1));
            doc_->addEntity(std::move(part2));
        }
    } else if (hit->typeId() == geom::TypeId_Polyline) {
        auto* poly = static_cast<geom::PolylineEntity*>(hit);
        if (poly->closed()) {
            QMessageBox::information(this, tr("Break"), tr("Breaking closed polylines is not supported yet."));
            return;
        }
        const auto& verts = poly->vertices();
        std::size_t n = verts.size();
        if (n < 2) return;
        std::size_t bestSeg = 0;
        double bestDist = std::numeric_limits<double>::infinity();
        Point2 bestPoint = world;
        for (std::size_t i = 0; i + 1 < n; ++i) {
            Point2 cp = geom::closestPointOnSegment(world, verts[i], verts[i + 1]);
            double d = geom::distance(world, cp);
            if (d < bestDist) {
                bestDist = d;
                bestSeg = i;
                bestPoint = cp;
            }
        }
        std::vector<Point2> part1(verts.begin(), verts.begin() + static_cast<long>(bestSeg) + 1);
        part1.push_back(bestPoint);
        std::vector<Point2> part2{ bestPoint };
        part2.insert(part2.end(), verts.begin() + static_cast<long>(bestSeg) + 1, verts.end());

        std::vector<std::unique_ptr<geom::Entity>> newEntities;
        if (part1.size() >= 2) {
            auto p1 = std::make_unique<geom::PolylineEntity>(part1, false);
            p1->setLayer(poly->layer());
            newEntities.push_back(std::move(p1));
        }
        if (part2.size() >= 2) {
            auto p2 = std::make_unique<geom::PolylineEntity>(part2, false);
            p2->setLayer(poly->layer());
            newEntities.push_back(std::move(p2));
        }
        if (undoStack_) undoStack_->beginMacro(tr("Break"));
        if (undoStack_) {
            undoStack_->push(new RemoveEntityCommand(doc_, hit, tr("Break")));
        } else {
            doc_->removeEntity(hit->id());
        }
        for (auto& e : newEntities) commitEntity(std::move(e), tr("Break"));
        if (undoStack_) undoStack_->endMacro();
    }
}

} // namespace bcad::app
