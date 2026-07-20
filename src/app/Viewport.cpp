#include "bcad/app/Viewport.h"

#include "bcad/app/Commands.h"
#include "bcad/app/CoordinateInput.h"
#include "bcad/app/TessellationWorker.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/SnapGeometry.h"
#include "bcad/geometry/Transform2D.h"
#include "bcad/render/Grid.h"
#include "bcad/render/LevelOfDetail.h"
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QUndoStack>
#include <QWheelEvent>
#include <limits>

namespace bcad::app {

using geom::Point2;

namespace {
constexpr double kPickToleranceScreenPx = 6.0;
constexpr double kSnapToleranceScreenPx = 10.0;
constexpr int kTessDebounceMs = 16;
} // namespace

Viewport::Viewport(QWidget* parent) : QOpenGLWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    worker_ = new TessellationWorker();
    worker_->moveToThread(&tessThread_);
    connect(&tessThread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &TessellationWorker::finished, this, &Viewport::onTessellationFinished,
            Qt::QueuedConnection);
    tessThread_.start();

    tessDebounce_.setSingleShot(true);
    connect(&tessDebounce_, &QTimer::timeout, this, &Viewport::requestTessellationNow);
}

Viewport::~Viewport() {
    tessThread_.quit();
    tessThread_.wait();
}

void Viewport::setDocument(core::Document* doc) {
    doc_ = doc;
    if (doc_) {
        doc_->onChanged = [this] { requestTessellation(); update(); };
    }
    requestTessellation();
    update();
}

void Viewport::setTool(ToolMode mode) {
    cancelActiveTool();
    tool_ = mode;
    emit toolChanged(tool_);
    update();
}

void Viewport::zoomToFit() {
    if (!doc_) return;
    geom::BoundingBox bb = doc_->extents();
    if (bb.isValid()) {
        bb.expand(std::max(bb.width(), bb.height()) * 0.05 + 1.0);
        camera_.zoomToFit(bb);
    } else {
        camera_.zoomToFit(geom::BoundingBox{ -50, -50, 50, 50 });
    }
    requestTessellation();
    update();
}

void Viewport::initializeGL() {
    renderer_.initialize();
}

void Viewport::resizeGL(int w, int h) {
    camera_.setViewportSize(w, h);
    requestTessellation();
}

void Viewport::paintGL() {
    // Qt's documented way to mix a QPainter overlay with raw GL calls inside
    // paintGL(): construct the painter first (it sets up the FBO-backed paint
    // device), wrap the native GL drawing in begin/endNativePainting, then
    // keep using the same painter for 2D overlay drawing. The background fill
    // and grid are painted *before* entering native painting (they must sit
    // under the geometry); GlRenderer itself no longer clears the framebuffer,
    // since that would wipe out what QPainter just drew here.
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 32, 36));
    if (gridVisible_) drawGrid(painter);

    painter.beginNativePainting();
    renderer_.render(camera_);
    painter.endNativePainting();

    drawToolPreview(painter);
    drawSnapMarker(painter);
    drawRubberBand(painter);
}

Point2 Viewport::toWorld(QPoint screenPos) const {
    return camera_.screenToWorld({ static_cast<double>(screenPos.x()), static_cast<double>(screenPos.y()) });
}

std::optional<Point2> Viewport::activeReferencePoint() const {
    if (!toolPoints_.empty()) return toolPoints_.back();
    if (tool_ == ToolMode::Move && moveAnchor_) return moveAnchor_;
    return std::nullopt;
}

Point2 Viewport::snappedWorld(QPoint screenPos) {
    Point2 world = toWorld(screenPos);
    activeSnap_ = {};

    if (doc_ && snapEnabled_) {
        double tol = kSnapToleranceScreenPx / camera_.pixelsPerUnit();
        activeSnap_ = snapEngine_.findSnap(*doc_, world, tol, activeReferencePoint());
        if (activeSnap_) return activeSnap_.point;
    }

    // Ortho constrains to horizontal/vertical from the reference point —
    // checked before grid snap since it's the more specific constraint
    // when both are active, but after object snap (which always wins).
    if (orthoEnabled_) {
        if (auto ref = activeReferencePoint()) {
            double dx = CGAL::to_double(world.x() - ref->x());
            double dy = CGAL::to_double(world.y() - ref->y());
            return std::abs(dx) >= std::abs(dy) ? Point2(CGAL::to_double(ref->x()) + dx, CGAL::to_double(ref->y()))
                                                  : Point2(CGAL::to_double(ref->x()), CGAL::to_double(ref->y()) + dy);
        }
    }

    // Grid snap only kicks in when object snap found nothing — object
    // geometry is always the more precise target when both are in reach.
    if (gridSnapEnabled_) {
        double spacing = render::adaptiveGridSpacing(camera_.pixelsPerUnit());
        Point2 gridPoint(render::snapToGrid(CGAL::to_double(world.x()), spacing),
                          render::snapToGrid(CGAL::to_double(world.y()), spacing));
        activeSnap_ = { SnapType::Grid, gridPoint };
        return gridPoint;
    }

    return world;
}

void Viewport::requestTessellation() {
    tessDebounce_.start(kTessDebounceMs);
}

void Viewport::requestTessellationNow() {
    if (!doc_ || !worker_) return;
    geom::BoundingBox region = camera_.visibleWorldRegion();
    double margin = std::max(region.width(), region.height()) * 0.1;
    region.expand(margin);
    double tolerance = render::worldToleranceForZoom(camera_.pixelsPerUnit());
    QMetaObject::invokeMethod(worker_, "build", Qt::QueuedConnection,
                               Q_ARG(const bcad::core::Document*, doc_),
                               Q_ARG(bcad::geom::BoundingBox, region),
                               Q_ARG(double, tolerance));
}

void Viewport::onTessellationFinished(render::TessellationResult result) {
    renderer_.setTessellation(std::move(result));
    update();
}

void Viewport::commitEntity(std::unique_ptr<geom::Entity> entity, const QString& label) {
    if (!doc_) return;
    if (undoStack_) {
        undoStack_->push(new AddEntityCommand(doc_, std::move(entity), label));
    } else {
        doc_->addEntity(std::move(entity));
    }
}

std::vector<geom::Entity*> Viewport::selectedEntities() const {
    std::vector<geom::Entity*> selected;
    if (!doc_) return selected;
    for (const auto& e : doc_->entities()) {
        if (e->selected) selected.push_back(e.get());
    }
    return selected;
}

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
        if (e->type() != geom::EntityType::Polyline) continue;
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
        if (e->type() == geom::EntityType::Line) lines.push_back(static_cast<geom::LineEntity*>(e));
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
        // Capture the layer before removing — the source line pointers
        // don't survive their own RemoveEntityCommand.
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
        if (e->selected && e->type() == geom::EntityType::Polyline) {
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

void Viewport::cancelActiveTool() {
    toolPoints_.clear();
    moveTarget_ = nullptr;
    moveAnchor_.reset();
}

void Viewport::finishPolyline() {
    if (toolPoints_.size() >= 2) {
        commitEntity(std::make_unique<geom::PolylineEntity>(toolPoints_, false), tr("Polyline"));
    }
    toolPoints_.clear();
    update();
}

void Viewport::placePoint(const Point2& world) {
    if (!doc_) return;

    switch (tool_) {
        case ToolMode::Select:
            break; // no point-placement meaning for Select
        case ToolMode::Move: {
            if (moveTarget_ && moveAnchor_) {
                double dx = CGAL::to_double(world.x() - moveAnchor_->x());
                double dy = CGAL::to_double(world.y() - moveAnchor_->y());
                auto transform = geom::Transform2D::translation(dx, dy);
                if (undoStack_) {
                    undoStack_->push(new TransformEntityCommand(doc_, moveTarget_, transform, tr("Move")));
                } else {
                    moveTarget_->applyTransform(transform);
                    doc_->notifyEntityChanged(moveTarget_);
                }
                moveTarget_ = nullptr;
                moveAnchor_.reset();
            }
            break;
        }
        case ToolMode::Copy: {
            // Base point, then destination point — operates on the current
            // selection (made beforehand with Select), cloning rather than
            // moving in place: AutoCAD's COPY always creates new geometry.
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 2) {
                std::vector<geom::Entity*> selected = selectedEntities();
                if (selected.empty()) {
                    QMessageBox::information(this, tr("Copy"), tr("Select entities to copy first."));
                } else {
                    double dx = CGAL::to_double(toolPoints_[1].x() - toolPoints_[0].x());
                    double dy = CGAL::to_double(toolPoints_[1].y() - toolPoints_[0].y());
                    auto transform = geom::Transform2D::translation(dx, dy);
                    if (undoStack_) undoStack_->beginMacro(tr("Copy"));
                    for (geom::Entity* e : selected) {
                        auto clone = e->clone();
                        clone->applyTransform(transform);
                        clone->selected = false;
                        commitEntity(std::move(clone), tr("Copy"));
                    }
                    if (undoStack_) undoStack_->endMacro();
                }
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Rotate: {
            // Pivot, then a reference point, then a target point — the
            // rotation applied is the angle *between* pivot->reference and
            // pivot->target, not an absolute angle, so it rotates relative
            // to however the selection is already oriented (matches
            // AutoCAD's ROTATE with a picked reference angle).
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 3) {
                std::vector<geom::Entity*> selected = selectedEntities();
                if (selected.empty()) {
                    QMessageBox::information(this, tr("Rotate"), tr("Select entities to rotate first."));
                } else {
                    const Point2& pivot = toolPoints_[0];
                    double refAngle = geom::angleOf(pivot, toolPoints_[1]);
                    double targetAngle = geom::angleOf(pivot, toolPoints_[2]);
                    auto transform = geom::Transform2D::rotation(targetAngle - refAngle, pivot);
                    if (undoStack_) undoStack_->beginMacro(tr("Rotate"));
                    for (geom::Entity* e : selected) {
                        if (undoStack_) {
                            undoStack_->push(new TransformEntityCommand(doc_, e, transform, tr("Rotate")));
                        } else {
                            e->applyTransform(transform);
                            doc_->notifyEntityChanged(e);
                        }
                    }
                    if (undoStack_) undoStack_->endMacro();
                }
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Scale: {
            // Base point, reference point, target point — scale factor is
            // the ratio of the two distances from the base point, so it's
            // relative to the selection's current size (matches AutoCAD's
            // SCALE with a picked reference length).
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 3) {
                std::vector<geom::Entity*> selected = selectedEntities();
                const Point2& base = toolPoints_[0];
                double refDist = geom::distance(base, toolPoints_[1]);
                if (selected.empty()) {
                    QMessageBox::information(this, tr("Scale"), tr("Select entities to scale first."));
                } else if (refDist < geom::Tolerance::kDegenerateLength) {
                    QMessageBox::information(this, tr("Scale"), tr("Reference distance is too small."));
                } else {
                    double targetDist = geom::distance(base, toolPoints_[2]);
                    double factor = targetDist / refDist;
                    auto transform = geom::Transform2D::scaling(factor, base);
                    if (undoStack_) undoStack_->beginMacro(tr("Scale"));
                    for (geom::Entity* e : selected) {
                        if (undoStack_) {
                            undoStack_->push(new TransformEntityCommand(doc_, e, transform, tr("Scale")));
                        } else {
                            e->applyTransform(transform);
                            doc_->notifyEntityChanged(e);
                        }
                    }
                    if (undoStack_) undoStack_->endMacro();
                }
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Mirror: {
            // Two points define the mirror line. Non-destructive by
            // default (clones + transforms, keeps the originals) — matches
            // AutoCAD MIRROR's default "erase source objects? No".
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 2) {
                std::vector<geom::Entity*> selected = selectedEntities();
                if (selected.empty()) {
                    QMessageBox::information(this, tr("Mirror"), tr("Select entities to mirror first."));
                } else {
                    auto transform = geom::Transform2D::mirrorAcrossLine(toolPoints_[0], toolPoints_[1]);
                    if (undoStack_) undoStack_->beginMacro(tr("Mirror"));
                    for (geom::Entity* e : selected) {
                        auto clone = e->clone();
                        clone->applyTransform(transform);
                        clone->selected = false;
                        commitEntity(std::move(clone), tr("Mirror"));
                    }
                    if (undoStack_) undoStack_->endMacro();
                }
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Trim: {
            // Lines only for now (§2.18): click near the end you want cut
            // back to the nearest real intersection with anything else in
            // the document — the "trim against everything" workflow
            // AutoCAD offers when you skip picking explicit cutting edges.
            geom::Entity* hit = pickModifiableEntity(world);
            if (!hit) break;
            if (hit->type() != geom::EntityType::Line) {
                QMessageBox::information(this, tr("Trim"), tr("Trim currently supports lines only."));
                break;
            }
            auto* line = static_cast<geom::LineEntity*>(hit);
            Point2 a = line->start(), b = line->end();
            geom::Vector2 dir = b - a;
            double lenSq = CGAL::to_double(dir.squared_length());
            if (lenSq < geom::Tolerance::kDegenerateLength) break;

            std::vector<double> ts;
            for (const auto& e : doc_->entities()) {
                if (e.get() == hit) continue;
                for (const Point2& p : geom::entityIntersections(*line, *e)) {
                    double t = CGAL::to_double((p - a) * dir) / lenSq;
                    if (t > 1e-9 && t < 1.0 - 1e-9) ts.push_back(t);
                }
            }
            if (ts.empty()) {
                QMessageBox::information(this, tr("Trim"), tr("No cutting edge found."));
                break;
            }
            double clickT = CGAL::to_double((world - a) * dir) / lenSq;
            double bestT = ts.front();
            for (double t : ts) {
                if (std::abs(t - clickT) < std::abs(bestT - clickT)) bestT = t;
            }
            Point2 cutPoint(CGAL::to_double(a.x()) + bestT * CGAL::to_double(dir.x()),
                             CGAL::to_double(a.y()) + bestT * CGAL::to_double(dir.y()));
            // Keep the side the user did NOT click on.
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
            break;
        }
        case ToolMode::Extend: {
            // Lines only for now (§2.18): extends whichever endpoint is
            // nearer the click, out to the nearest entity that the line's
            // infinite extension would cross. Implemented by probing
            // intersections against a very long stand-in segment rather
            // than adding a dedicated infinite-line intersection routine.
            geom::Entity* hit = pickModifiableEntity(world);
            if (!hit) break;
            if (hit->type() != geom::EntityType::Line) {
                QMessageBox::information(this, tr("Extend"), tr("Extend currently supports lines only."));
                break;
            }
            auto* line = static_cast<geom::LineEntity*>(hit);
            Point2 a = line->start(), b = line->end();
            geom::Vector2 dir = b - a;
            double lenSq = CGAL::to_double(dir.squared_length());
            if (lenSq < geom::Tolerance::kDegenerateLength) break;

            bool extendFromB = geom::distance(world, b) <= geom::distance(world, a);
            constexpr double kExtendFactor = 1e5; // effectively unbounded for any realistic drawing
            Point2 farA = extendFromB ? a
                                       : Point2(CGAL::to_double(a.x()) - kExtendFactor * CGAL::to_double(dir.x()),
                                                CGAL::to_double(a.y()) - kExtendFactor * CGAL::to_double(dir.y()));
            Point2 farB = extendFromB
                              ? Point2(CGAL::to_double(b.x()) + kExtendFactor * CGAL::to_double(dir.x()),
                                       CGAL::to_double(b.y()) + kExtendFactor * CGAL::to_double(dir.y()))
                              : b;
            geom::LineEntity probe(farA, farB);

            std::optional<double> bestT;
            for (const auto& e : doc_->entities()) {
                if (e.get() == hit) continue;
                for (const Point2& p : geom::entityIntersections(probe, *e)) {
                    double t = CGAL::to_double((p - a) * dir) / lenSq;
                    bool beyond = extendFromB ? (t > 1.0 + 1e-9) : (t < -1e-9);
                    if (!beyond) continue;
                    if (!bestT || (extendFromB ? t < *bestT : t > *bestT)) bestT = t;
                }
            }
            if (!bestT) {
                QMessageBox::information(this, tr("Extend"), tr("Nothing found to extend to."));
                break;
            }
            Point2 newPoint(CGAL::to_double(a.x()) + (*bestT) * CGAL::to_double(dir.x()),
                             CGAL::to_double(a.y()) + (*bestT) * CGAL::to_double(dir.y()));
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
            break;
        }
        case ToolMode::Break: {
            // Lines and open polylines (§2.18) — closed polylines need
            // two break points in real CAD tools (the loop stays a loop),
            // which this single-click MVP doesn't attempt yet.
            geom::Entity* hit = pickModifiableEntity(world);
            if (!hit) break;

            if (hit->type() == geom::EntityType::Line) {
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
            } else if (hit->type() == geom::EntityType::Polyline) {
                auto* poly = static_cast<geom::PolylineEntity*>(hit);
                if (poly->closed()) {
                    QMessageBox::information(this, tr("Break"), tr("Breaking closed polylines is not supported yet."));
                    break;
                }
                const auto& verts = poly->vertices();
                std::size_t n = verts.size();
                if (n < 2) break;
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
            break;
        }
        case ToolMode::Line: {
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 2) {
                commitEntity(std::make_unique<geom::LineEntity>(toolPoints_[0], toolPoints_[1]), tr("Line"));
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Circle: {
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 2) {
                double r = geom::distance(toolPoints_[0], toolPoints_[1]);
                commitEntity(std::make_unique<geom::CircleEntity>(toolPoints_[0], r), tr("Circle"));
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Arc: {
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 3) {
                double r = geom::distance(toolPoints_[0], toolPoints_[1]);
                double startAngle = geom::angleOf(toolPoints_[0], toolPoints_[1]);
                double endAngle = geom::angleOf(toolPoints_[0], toolPoints_[2]);
                commitEntity(std::make_unique<geom::ArcEntity>(toolPoints_[0], r, startAngle, endAngle), tr("Arc"));
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Polyline: {
            toolPoints_.push_back(world);
            break;
        }
    }
    update();
}

void Viewport::submitTypedPoint(const QString& text) {
    if (!doc_) return;
    std::optional<Point2> parsed = parseCoordinateInput(text.toStdString(), activeReferencePoint());
    if (!parsed) return;
    activeSnap_ = {};
    placePoint(*parsed);
}

void Viewport::mousePressEvent(QMouseEvent* event) {
    Point2 rawWorld = toWorld(event->pos());
    double pickTol = kPickToleranceScreenPx / camera_.pixelsPerUnit();

    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastMousePos_ = event->pos();
        return;
    }

    if (event->button() == Qt::RightButton) {
        if (tool_ == ToolMode::Polyline && toolPoints_.size() >= 2) {
            finishPolyline();
        } else {
            cancelActiveTool();
            update();
        }
        return;
    }

    if (event->button() != Qt::LeftButton || !doc_) return;

    // Picking an existing entity (Select, first Move click) uses the raw
    // cursor position; placing a new point (drawing tools, Move destination)
    // uses the snapped position so geometry can be anchored precisely.
    switch (tool_) {
        case ToolMode::Select: {
            bool additive = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);
            geom::Entity* hit = doc_->pickEntity(rawWorld, pickTol);
            if (hit) {
                if (!additive) {
                    for (const auto& e : doc_->entities()) e->selected = false;
                    hit->selected = true;
                } else {
                    hit->selected = !hit->selected; // modifier-click toggles membership
                }
            } else {
                // Empty space: clear now (unless additive) and start a
                // rubber-band drag; direction (left-to-right vs
                // right-to-left) decided on release, once we know the end
                // point — see mouseReleaseEvent.
                if (!additive) {
                    for (const auto& e : doc_->entities()) e->selected = false;
                }
                rubberBandActive_ = true;
                rubberBandStartScreen_ = event->pos();
            }
            emit selectionChanged();
            update();
            break;
        }
        case ToolMode::Move: {
            if (!moveTarget_) {
                moveTarget_ = doc_->pickEntity(rawWorld, pickTol);
                if (moveTarget_) moveAnchor_ = rawWorld;
                update();
            } else {
                placePoint(snappedWorld(event->pos()));
            }
            break;
        }
        default:
            placePoint(snappedWorld(event->pos()));
            break;
    }
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    Point2 world = snappedWorld(event->pos());
    hoverWorld_ = world;
    emit cursorWorldPositionChanged(CGAL::to_double(world.x()), CGAL::to_double(world.y()));

    if (panning_) {
        QPoint delta = event->pos() - lastMousePos_;
        camera_.panByScreenDelta(delta.x(), delta.y());
        lastMousePos_ = event->pos();
        requestTessellation();
    }
    update();
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) panning_ = false;

    if (event->button() == Qt::LeftButton && rubberBandActive_) {
        rubberBandActive_ = false;
        QPoint endScreen = event->pos();
        // Ignore accidental micro-drags — treat as the empty-space click
        // it visually was (selection already cleared at press time).
        if (doc_ && (endScreen - rubberBandStartScreen_).manhattanLength() > 3) {
            Point2 p1 = toWorld(rubberBandStartScreen_);
            Point2 p2 = toWorld(endScreen);
            double x1 = CGAL::to_double(p1.x()), y1 = CGAL::to_double(p1.y());
            double x2 = CGAL::to_double(p2.x()), y2 = CGAL::to_double(p2.y());
            geom::BoundingBox worldRect{ std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2) };

            // Left-to-right drag = window (fully-enclosed only); right-to-left
            // = crossing (anything touched) — the standard AutoCAD convention.
            bool windowMode = endScreen.x() >= rubberBandStartScreen_.x();
            for (geom::Entity* e : doc_->entitiesInRegion(worldRect)) {
                if (windowMode && !worldRect.contains(e->boundingBox())) continue;
                e->selected = true;
            }
            emit selectionChanged();
        }
        update();
    }
}

void Viewport::wheelEvent(QWheelEvent* event) {
    double factor = std::pow(1.0015, event->angleDelta().y());
    QPointF pos = event->position();
    camera_.zoomAt(factor, { pos.x(), pos.y() });
    requestTessellation();
    update();
}

void Viewport::keyPressEvent(QKeyEvent* event) {
    bool drawingToolActive = tool_ == ToolMode::Line || tool_ == ToolMode::Circle || tool_ == ToolMode::Arc ||
                              tool_ == ToolMode::Polyline;

    if (event->key() == Qt::Key_Escape) {
        cancelActiveTool();
        update();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (tool_ == ToolMode::Polyline) finishPolyline();
    } else if (event->key() == Qt::Key_F) {
        zoomToFit();
    } else if (event->modifiers() == Qt::NoModifier && drawingToolActive && !event->text().isEmpty() &&
               (event->text().at(0).isDigit() || event->text().at(0) == QChar('@') ||
                event->text().at(0) == QChar('-'))) {
        // Typing a coordinate directly into the viewport activates the
        // command line, seeded with what was just typed — mirrors AutoCAD's
        // dynamic input rather than requiring a click into the field first.
        emit typedInputRequested(event->text());
    } else {
        QOpenGLWidget::keyPressEvent(event);
    }
}

void Viewport::drawToolPreview(QPainter& painter) {
    if (toolPoints_.empty() && !hoverWorld_) return;

    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(255, 200, 0));
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);

    auto toScreen = [this](const Point2& p) {
        render::ScreenPoint s = camera_.worldToScreen(p);
        return QPointF(s.x, s.y);
    };

    for (const auto& p : toolPoints_) {
        QPointF sp = toScreen(p);
        painter.drawEllipse(sp, 3, 3);
    }

    if (!toolPoints_.empty() && hoverWorld_) {
        QPointF last = toScreen(toolPoints_.back());
        QPointF cur = toScreen(*hoverWorld_);

        if (tool_ == ToolMode::Circle && toolPoints_.size() == 1) {
            double r = geom::distance(toolPoints_[0], *hoverWorld_) * camera_.pixelsPerUnit();
            painter.drawEllipse(last, r, r);
        } else if (tool_ == ToolMode::Polyline) {
            for (std::size_t i = 1; i < toolPoints_.size(); ++i) {
                painter.drawLine(toScreen(toolPoints_[i - 1]), toScreen(toolPoints_[i]));
            }
            painter.drawLine(last, cur);
        } else {
            painter.drawLine(last, cur);
        }
    }
}

void Viewport::drawSnapMarker(QPainter& painter) {
    if (!activeSnap_) return;

    render::ScreenPoint s = camera_.worldToScreen(activeSnap_.point);
    QPointF p(s.x, s.y);

    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(80, 220, 120));
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Shape communicates snap type, matching the convention most CAD tools
    // use: square = endpoint, circle = center, triangle = midpoint,
    // diamond = intersection, corner bracket = perpendicular, X = grid.
    constexpr double kHalf = 6.0;
    switch (activeSnap_.type) {
        case SnapType::Endpoint:
            painter.drawRect(QRectF(p.x() - kHalf, p.y() - kHalf, kHalf * 2, kHalf * 2));
            break;
        case SnapType::Center:
            painter.drawEllipse(p, kHalf, kHalf);
            break;
        case SnapType::Midpoint: {
            QPolygonF tri;
            tri << QPointF(p.x(), p.y() - kHalf) << QPointF(p.x() - kHalf, p.y() + kHalf)
                << QPointF(p.x() + kHalf, p.y() + kHalf);
            painter.drawPolygon(tri);
            break;
        }
        case SnapType::Intersection: {
            QPolygonF diamond;
            diamond << QPointF(p.x(), p.y() - kHalf) << QPointF(p.x() + kHalf, p.y())
                    << QPointF(p.x(), p.y() + kHalf) << QPointF(p.x() - kHalf, p.y());
            painter.drawPolygon(diamond);
            break;
        }
        case SnapType::Perpendicular: {
            QPointF a(p.x() - kHalf, p.y() + kHalf);
            QPointF b(p.x() + kHalf, p.y() + kHalf);
            QPointF c(p.x() + kHalf, p.y() - kHalf);
            painter.drawLine(a, b);
            painter.drawLine(b, c);
            break;
        }
        case SnapType::Quadrant: {
            QPolygonF diamond;
            diamond << QPointF(p.x(), p.y() - kHalf) << QPointF(p.x() + kHalf, p.y())
                    << QPointF(p.x(), p.y() + kHalf) << QPointF(p.x() - kHalf, p.y());
            painter.setBrush(QColor(80, 220, 120));
            painter.drawPolygon(diamond);
            painter.setBrush(Qt::NoBrush);
            break;
        }
        case SnapType::Nearest: {
            // Hourglass, the conventional NEA glyph.
            QPolygonF hourglass;
            hourglass << QPointF(p.x() - kHalf, p.y() - kHalf) << QPointF(p.x() + kHalf, p.y() - kHalf)
                       << QPointF(p.x() - kHalf, p.y() + kHalf) << QPointF(p.x() + kHalf, p.y() + kHalf);
            painter.drawPolygon(hourglass);
            break;
        }
        case SnapType::Grid:
            painter.drawLine(QPointF(p.x() - kHalf, p.y() - kHalf), QPointF(p.x() + kHalf, p.y() + kHalf));
            painter.drawLine(QPointF(p.x() - kHalf, p.y() + kHalf), QPointF(p.x() + kHalf, p.y() - kHalf));
            break;
        case SnapType::None:
            break;
    }
}

void Viewport::drawGrid(QPainter& painter) {
    double spacing = render::adaptiveGridSpacing(camera_.pixelsPerUnit());
    geom::BoundingBox region = camera_.visibleWorldRegion();
    if (spacing <= 0.0 || !region.isValid()) return;

    double startX = std::floor(region.minX / spacing) * spacing;
    double startY = std::floor(region.minY / spacing) * spacing;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(68, 71, 76));

    for (double y = startY; y <= region.maxY; y += spacing) {
        for (double x = startX; x <= region.maxX; x += spacing) {
            render::ScreenPoint s = camera_.worldToScreen(geom::Point2(x, y));
            painter.drawEllipse(QPointF(s.x, s.y), 1.2, 1.2);
        }
    }

    // Origin axes, brighter, when in view — a fixed visual anchor.
    painter.setPen(QPen(QColor(95, 100, 106), 1));
    if (region.minX <= 0.0 && region.maxX >= 0.0) {
        render::ScreenPoint top = camera_.worldToScreen(geom::Point2(0, region.maxY));
        render::ScreenPoint bottom = camera_.worldToScreen(geom::Point2(0, region.minY));
        painter.drawLine(QPointF(top.x, top.y), QPointF(bottom.x, bottom.y));
    }
    if (region.minY <= 0.0 && region.maxY >= 0.0) {
        render::ScreenPoint left = camera_.worldToScreen(geom::Point2(region.minX, 0));
        render::ScreenPoint right = camera_.worldToScreen(geom::Point2(region.maxX, 0));
        painter.drawLine(QPointF(left.x, left.y), QPointF(right.x, right.y));
    }
}

void Viewport::drawRubberBand(QPainter& painter) {
    if (!rubberBandActive_ || !hoverWorld_) return;

    render::ScreenPoint startS = camera_.worldToScreen(toWorld(rubberBandStartScreen_));
    render::ScreenPoint endS = camera_.worldToScreen(*hoverWorld_);
    QRectF rect(QPointF(startS.x, startS.y), QPointF(endS.x, endS.y));

    // Window (left-to-right) reads as a solid blue box; crossing
    // (right-to-left) as a dashed green one — the AutoCAD convention.
    bool windowMode = endS.x >= startS.x;
    QColor color = windowMode ? QColor(80, 140, 220) : QColor(90, 200, 110);
    QPen pen(color);
    pen.setStyle(windowMode ? Qt::SolidLine : Qt::DashLine);
    painter.setPen(pen);
    painter.setBrush(QColor(color.red(), color.green(), color.blue(), 40));
    painter.drawRect(rect);
    painter.setBrush(Qt::NoBrush);
}

} // namespace bcad::app
