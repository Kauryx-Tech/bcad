// Les opérations qui portent sur la sélection déjà constituée, sans outil
// interactif : elles arrivent par le menu, le ruban ou le clavier et
// s'annulent en une seule macro chacune.

#include "Viewport.h"

#include "Commands.h"
#include "ViewportTolerances.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include <QMessageBox>
#include <QUndoStack>
#include <memory>

namespace bcad::app {

using geom::Point2;

void Viewport::deleteSelected() {
    if (!doc_) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) return;

    if (undoStack_) {
        undoStack_->beginMacro(tr("Delete"));
        for (geom::Entity* e : selected) undoStack_->push(new RemoveEntityCommand(doc_, e, tr("Delete")));
        undoStack_->endMacro();
    } else {
        for (geom::Entity* e : selected) doc_->removeEntity(e->id());
    }
}

void Viewport::explodeSelected() {
    if (!doc_) return;
    std::vector<geom::Entity*> selected = selectedEntities();

    std::vector<geom::Entity*> toRemove;
    std::vector<std::unique_ptr<geom::Entity>> toAdd;
    for (geom::Entity* e : selected) {
        if (e->typeId() != geom::TypeId_Polyline) continue;
        const auto& poly = static_cast<const geom::PolylineEntity&>(*e);
        const auto& verts = poly.vertices();
        std::size_t n = verts.size();
        std::size_t segCount = poly.closed() ? n : (n == 0 ? 0 : n - 1);
        if (segCount == 0) continue;
        for (std::size_t i = 0; i < segCount; ++i) {
            auto line = std::make_unique<geom::LineEntity>(verts[i], verts[(i + 1) % n]);
            line->setLayer(poly.layer());
            if (poly.colorOverride()) line->setColorOverride(poly.colorOverride());
            toAdd.push_back(std::move(line));
        }
        toRemove.push_back(e);
    }
    if (toRemove.empty()) return;

    if (undoStack_) {
        undoStack_->beginMacro(tr("Explode"));
        for (geom::Entity* e : toRemove) undoStack_->push(new RemoveEntityCommand(doc_, e, tr("Explode")));
        for (auto& e : toAdd) undoStack_->push(new AddEntityCommand(doc_, std::move(e), tr("Explode")));
        undoStack_->endMacro();
    } else {
        for (geom::Entity* e : toRemove) doc_->removeEntity(e->id());
        for (auto& e : toAdd) doc_->addEntity(std::move(e));
    }
}

void Viewport::selectAll() {
    if (!doc_) return;
    for (const auto& e : doc_->entities()) e->selected = true;
    emit selectionChanged();
    update();
}

void Viewport::selectLast() {
    if (!doc_) return;
    geom::Entity* last = nullptr;
    int maxId = -1;
    for (const auto& e : doc_->entities()) {
        e->selected = false;
        if (e->id() > maxId) {
            maxId = e->id();
            last = e.get();
        }
    }
    if (last) last->selected = true;
    emit selectionChanged();
    update();
}

void Viewport::joinSelected() {
    if (!doc_) return;
    std::vector<geom::LineEntity*> lines;
    for (geom::Entity* e : selectedEntities()) {
        if (e->typeId() == geom::TypeId_Line) lines.push_back(static_cast<geom::LineEntity*>(e));
    }
    if (lines.size() < 2) {
        QMessageBox::information(this, tr("Join"), tr("Select at least two lines to join."));
        return;
    }

    double tol = kPickToleranceScreenPx / camera_.pixelsPerUnit();
    std::size_t n = lines.size();
    std::vector<bool> used(n, false);

    struct Chain {
        std::vector<Point2> points;
        std::vector<std::size_t> lineIndices;
    };
    std::vector<Chain> chains;

    for (std::size_t i = 0; i < n; ++i) {
        if (used[i]) continue;
        used[i] = true;
        Chain chain;
        chain.points = { lines[i]->start(), lines[i]->end() };
        chain.lineIndices = { i };
        bool extended = true;
        while (extended) {
            extended = false;
            for (std::size_t j = 0; j < n; ++j) {
                if (used[j]) continue;
                const Point2& a = lines[j]->start();
                const Point2& b = lines[j]->end();
                if (geom::distance(chain.points.back(), a) < tol) {
                    chain.points.push_back(b);
                } else if (geom::distance(chain.points.back(), b) < tol) {
                    chain.points.push_back(a);
                } else if (geom::distance(chain.points.front(), a) < tol) {
                    chain.points.insert(chain.points.begin(), b);
                } else if (geom::distance(chain.points.front(), b) < tol) {
                    chain.points.insert(chain.points.begin(), a);
                } else {
                    continue;
                }
                used[j] = true;
                chain.lineIndices.push_back(j);
                extended = true;
            }
        }
        chains.push_back(std::move(chain));
    }

    bool anyMerged = false;
    for (const auto& chain : chains) {
        if (chain.lineIndices.size() >= 2) anyMerged = true;
    }
    if (!anyMerged) {
        QMessageBox::information(this, tr("Join"), tr("No selected lines share an endpoint."));
        return;
    }

    if (undoStack_) undoStack_->beginMacro(tr("Join"));
    for (auto& chain : chains) {
        if (chain.lineIndices.size() < 2) continue;
        // Capture le calque avant la suppression — les pointeurs de ligne
        // source ne survivent pas à leur propre RemoveEntityCommand.
        std::string chainLayer = lines[chain.lineIndices[0]]->layer();
        for (std::size_t idx : chain.lineIndices) {
            if (undoStack_) {
                undoStack_->push(new RemoveEntityCommand(doc_, lines[idx], tr("Join")));
            } else {
                doc_->removeEntity(lines[idx]->id());
            }
        }
        auto poly = std::make_unique<geom::PolylineEntity>(chain.points, false);
        poly->setLayer(chainLayer);
        commitEntity(std::move(poly), tr("Join"));
    }
    if (undoStack_) undoStack_->endMacro();
}

geom::Entity* Viewport::pickModifiableEntity(const Point2& world) const {
    if (!doc_) return nullptr;
    double pickTol = kPickToleranceScreenPx / camera_.pixelsPerUnit();
    return doc_->pickEntity(world, pickTol);
}

void Viewport::booleanOperation(geom::BooleanOp op) {
    if (!doc_) return;

    std::vector<geom::PolylineEntity*> selected;
    for (const auto& e : doc_->entities()) {
        if (e->selected && e->typeId() == geom::TypeId_Polyline) {
            selected.push_back(static_cast<geom::PolylineEntity*>(e.get()));
        }
    }
    if (selected.size() != 2 || !selected[0]->closed() || !selected[1]->closed()) {
        QMessageBox::information(this, tr("Boolean Operation"),
                                  tr("Select exactly two closed polylines first."));
        return;
    }

    std::vector<geom::PolylineEntity> results = geom::booleanOp(*selected[0], *selected[1], op);
    if (results.empty()) {
        QMessageBox::information(this, tr("Boolean Operation"), tr("The operation produced no geometry."));
        return;
    }

    std::string layer = selected[0]->layer();
    geom::Entity* first = selected[0];
    geom::Entity* second = selected[1];

    if (undoStack_) {
        undoStack_->beginMacro(tr("Boolean Operation"));
        undoStack_->push(new RemoveEntityCommand(doc_, first, tr("Boolean")));
        undoStack_->push(new RemoveEntityCommand(doc_, second, tr("Boolean")));
        for (auto& poly : results) {
            poly.setLayer(layer);
            undoStack_->push(
                new AddEntityCommand(doc_, std::make_unique<geom::PolylineEntity>(std::move(poly)), tr("Boolean")));
        }
        undoStack_->endMacro();
    } else {
        doc_->removeEntity(first->id());
        doc_->removeEntity(second->id());
        for (auto& poly : results) {
            poly.setLayer(layer);
            doc_->addEntity(std::make_unique<geom::PolylineEntity>(std::move(poly)));
        }
    }
    update();
}

} // namespace bcad::app
