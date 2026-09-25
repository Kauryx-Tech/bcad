#include "Viewport.h"

#include "Commands.h"
#include "CoordinateInput.h"
#include "TessellationWorker.h"
#include "bcad/events/EventBus.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/SnapGeometry.h"
#include "bcad/geometry/Transform2D.h"
#include "bcad/render/Grid.h"
#include "bcad/render/LevelOfDetail.h"
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include "bcad/layout/Dimension.h"
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
    // Unsubscribe from previous document
    docSub_.reset();

    doc_ = doc;
    if (doc_) {
        events::EventBus::SubscriptionId id = events::EventBus::instance().subscribe<events::DocumentChanged>(
            [this](const events::DocumentChanged&) {
                requestTessellation();
                update();
            });
        docSub_ = std::make_unique<events::SubscriptionGuard>(id);
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
    // La façon documentée par Qt de mélanger une surimpression QPainter
    // avec des appels GL bruts dans paintGL() : construire le painter en
    // premier (il met en place le périphérique de peinture adossé au FBO),
    // envelopper le dessin GL natif dans begin/endNativePainting, puis
    // continuer à utiliser le même painter pour le dessin de surimpression
    // 2D. Le remplissage du fond et la grille sont peints *avant* d'entrer
    // dans le dessin natif (ils doivent se trouver sous la géométrie) ;
    // GlRenderer lui-même n'efface plus le framebuffer, puisque cela
    // effacerait ce que QPainter vient de dessiner ici.
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 32, 36));
    if (gridVisible_) drawGrid(painter);

    painter.beginNativePainting();
    renderer_.render(camera_);
    painter.endNativePainting();

    painter.save();
    if (doc_) {
        for (const auto& entity : doc_->entities()) {
            const auto* text = dynamic_cast<const geom::TextEntity*>(entity.get());
            if (!text) continue;
            const auto screen = camera_.worldToScreen(text->position());
            painter.setPen(QColor(235, 235, 235));
            QFont font;
            font.setPointSizeF(std::max(7.0, text->height() * camera_.pixelsPerUnit() * 0.75));
            painter.setFont(font);
            painter.drawText(QPointF(screen.x, screen.y), QString::fromStdString(text->text()));
        }
    }
    painter.restore();

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

    // Ortho contraint à l'horizontale/verticale depuis le point de
    // référence — vérifié avant l'accrochage à la grille car c'est la
    // contrainte la plus spécifique quand les deux sont actives, mais
    // après l'accrochage aux objets (qui l'emporte toujours).
    if (orthoEnabled_) {
        if (auto ref = activeReferencePoint()) {
            double dx = world.x_ - ref->x_;
            double dy = world.y_ - ref->y_;
            return std::abs(dx) >= std::abs(dy) ? Point2(ref->x_ + dx, ref->y_)
                                                  : Point2(ref->x_, ref->y_ + dy);
        }
    }

    // L'accrochage à la grille n'intervient que lorsque l'accrochage aux
    // objets n'a rien trouvé — la géométrie des objets est toujours la
    // cible la plus précise quand les deux sont à portée.
    if (gridSnapEnabled_) {
        double spacing = render::adaptiveGridSpacing(camera_.pixelsPerUnit());
        Point2 gridPoint(render::snapToGrid(world.x_, spacing),
                          render::snapToGrid(world.y_, spacing));
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

void Viewport::onTessellationFinished(core::TessellationResult result) {
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
            break; // aucune signification de placement de point pour Sélection
        case ToolMode::Move: {
            if (moveTarget_ && moveAnchor_) {
                double dx = world.x_ - moveAnchor_->x_;
                double dy = world.y_ - moveAnchor_->y_;
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
            // Point de base, puis point de destination — opère sur la
            // sélection actuelle (faite au préalable avec Sélection), en
            // clonant plutôt qu'en déplaçant sur place : la commande COPY
            // d'AutoCAD crée toujours une nouvelle géométrie.
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 2) {
                std::vector<geom::Entity*> selected = selectedEntities();
                if (selected.empty()) {
                    QMessageBox::information(this, tr("Copy"), tr("Select entities to copy first."));
                } else {
                    double dx = toolPoints_[1].x_ - toolPoints_[0].x_;
                    double dy = toolPoints_[1].y_ - toolPoints_[0].y_;
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
            // Deux points définissent la ligne de symétrie. Non destructif
            // par défaut (clone + transforme, garde les originaux) —
            // correspond au comportement par défaut "effacer les objets
            // source ? Non" de la commande MIRROR d'AutoCAD.
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
            // Lignes uniquement pour l'instant (§2.18) : cliquez près de
            // l'extrémité que vous voulez couper jusqu'à la plus proche
            // intersection réelle avec autre chose dans le document — le
            // flux de travail "couper contre tout" qu'offre AutoCAD quand
            // on ne pointe pas d'arêtes de coupe explicites.
            geom::Entity* hit = pickModifiableEntity(world);
            if (!hit) break;
            if (hit->typeId() != geom::TypeId_Line) {
                QMessageBox::information(this, tr("Trim"), tr("Trim currently supports lines only."));
                break;
            }
            auto* line = static_cast<geom::LineEntity*>(hit);
            Point2 a = line->start(), b = line->end();
            geom::Vector2 dir = b - a;
            double lenSq = geom::squaredLength(dir);
            if (lenSq < geom::Tolerance::kDegenerateLength) break;

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
                break;
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
            break;
        }
        case ToolMode::Extend: {
            // Lignes uniquement pour l'instant (§2.18) : prolonge
            // l'extrémité la plus proche du clic, jusqu'à la plus proche
            // entité que croiserait le prolongement infini de la ligne.
            // Implémenté en sondant les intersections avec un très long
            // segment de substitution plutôt qu'en ajoutant une routine
            // d'intersection de droite infinie dédiée.
            geom::Entity* hit = pickModifiableEntity(world);
            if (!hit) break;
            if (hit->typeId() != geom::TypeId_Line) {
                QMessageBox::information(this, tr("Extend"), tr("Extend currently supports lines only."));
                break;
            }
            auto* line = static_cast<geom::LineEntity*>(hit);
            Point2 a = line->start(), b = line->end();
            geom::Vector2 dir = b - a;
            double lenSq = geom::squaredLength(dir);
            if (lenSq < geom::Tolerance::kDegenerateLength) break;

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
                break;
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
            break;
        }
        case ToolMode::Break: {
            // Lignes et polylignes ouvertes (§2.18) — les polylignes
            // fermées nécessitent deux points de coupure dans les vrais
            // outils CAO (la boucle reste une boucle), ce que ce MVP à
            // clic unique ne tente pas encore.
            geom::Entity* hit = pickModifiableEntity(world);
            if (!hit) break;

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
        case ToolMode::Rectangle: {
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 2) {
                const Point2& p0 = toolPoints_[0];
                const Point2& p1 = toolPoints_[1];
                std::vector<Point2> corners{
                    p0, Point2(p1.x(), p0.y()), p1, Point2(p0.x(), p1.y()),
                };
                commitEntity(std::make_unique<geom::PolylineEntity>(std::move(corners), true), tr("Rectangle"));
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::Point: {
            commitEntity(std::make_unique<geom::PointEntity>(world), tr("Point"));
            break;
        }
        case ToolMode::DimensionLinear:
        case ToolMode::DimensionAligned: {
            toolPoints_.push_back(world);
            const std::size_t requiredPoints =
                tool_ == ToolMode::DimensionAligned ? 2 : 3;
            if (toolPoints_.size() == requiredPoints) {
                const Point2& a = toolPoints_[0];
                const Point2& b = toolPoints_[1];
                const geom::Vector2 base = b - a;
                const double length = geom::length(base);
                if (length < geom::Tolerance::kDegenerateLength) {
                    toolPoints_.clear();
                    break;
                }
                const geom::Vector2 normal{-base.y_ / length, base.x_ / length};
                const double distance = tool_ == ToolMode::DimensionAligned
                    ? 0.0 : geom::dot(toolPoints_[2] - a, normal);
                const Point2 da{a.x_ + normal.x_ * distance, a.y_ + normal.y_ * distance};
                const Point2 db{b.x_ + normal.x_ * distance, b.y_ + normal.y_ * distance};
                auto extensionA = std::make_unique<geom::LineEntity>(a, da);
                auto extensionB = std::make_unique<geom::LineEntity>(b, db);
                auto dimensionLine = std::make_unique<geom::LineEntity>(da, db);
                extensionA->setLayer("Dimensions");
                extensionB->setLayer("Dimensions");
                dimensionLine->setLayer("Dimensions");
                const Point2 labelPoint{
                    (da.x_ + db.x_) * 0.5 + normal.x_ * 0.15,
                    (da.y_ + db.y_) * 0.5 + normal.y_ * 0.15};
                const std::string label = layout::Dimension{
                    a, b, length, Point2{(a.x_ + b.x_) * 0.5, (a.y_ + b.y_) * 0.5}}.text();
                auto text = std::make_unique<geom::TextEntity>(labelPoint, label, 0.12);
                text->setLayer("Dimensions");
                if (undoStack_) {
                    undoStack_->beginMacro(tool_ == ToolMode::DimensionAligned
                        ? tr("Cotation alignée") : tr("Cotation linéaire"));
                    commitEntity(std::move(extensionA), tr("Cotation"));
                    commitEntity(std::move(extensionB), tr("Cotation"));
                    commitEntity(std::move(dimensionLine), tr("Cotation"));
                    commitEntity(std::move(text), tr("Texte de cotation"));
                    undoStack_->endMacro();
                } else {
                    doc_->addEntity(std::move(extensionA));
                    doc_->addEntity(std::move(extensionB));
                    doc_->addEntity(std::move(dimensionLine));
                    doc_->addEntity(std::move(text));
                }
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::DimensionAngular: {
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 3) {
                const Point2& vertex = toolPoints_[0];
                const Point2& start = toolPoints_[1];
                const Point2& end = toolPoints_[2];
                const double radius = geom::distance(vertex, start);
                if (radius < geom::Tolerance::kDegenerateLength) {
                    toolPoints_.clear();
                    break;
                }
                const double startAngle = geom::angleOf(vertex, start);
                const double endAngle = geom::angleOf(vertex, end);
                auto arc = std::make_unique<geom::ArcEntity>(
                    vertex, radius, startAngle, endAngle);
                auto rayA = std::make_unique<geom::LineEntity>(vertex, start);
                auto rayB = std::make_unique<geom::LineEntity>(vertex, end);
                rayA->setLayer("Dimensions");
                rayB->setLayer("Dimensions");
                arc->setLayer("Dimensions");
                if (undoStack_) undoStack_->beginMacro(tr("Cotation angulaire"));
                commitEntity(std::move(rayA), tr("Cotation angulaire"));
                commitEntity(std::move(rayB), tr("Cotation angulaire"));
                commitEntity(std::move(arc), tr("Cotation angulaire"));
                const double middle = startAngle + (endAngle - startAngle) * 0.5;
                const Point2 labelPoint{
                    vertex.x_ + std::cos(middle) * radius * 1.15,
                    vertex.y_ + std::sin(middle) * radius * 1.15};
                auto text = std::make_unique<geom::TextEntity>(
                    labelPoint, QString::number((endAngle - startAngle) * 180.0 /
                                                std::numbers::pi, 'f', 1).append(QChar(0x00B0)).toStdString(),
                    0.12);
                text->setLayer("Dimensions");
                commitEntity(std::move(text), tr("Texte de cotation"));
                if (undoStack_) undoStack_->endMacro();
                toolPoints_.clear();
            }
            break;
        }
        case ToolMode::DimensionRadius:
        case ToolMode::DimensionDiameter: {
            toolPoints_.push_back(world);
            if (toolPoints_.size() == 2) {
                const Point2& center = toolPoints_[0];
                const Point2& edge = toolPoints_[1];
                const double radius = geom::distance(center, edge);
                if (radius < geom::Tolerance::kDegenerateLength) {
                    toolPoints_.clear();
                    break;
                }
                const Point2 opposite{center.x_ - (edge.x_ - center.x_),
                                      center.y_ - (edge.y_ - center.y_)};
                auto line = std::make_unique<geom::LineEntity>(
                    center, tool_ == ToolMode::DimensionDiameter ? opposite : edge);
                line->setLayer("Dimensions");
                const Point2 labelPoint{
                    (center.x_ + (tool_ == ToolMode::DimensionDiameter ? opposite.x_ : edge.x_)) * 0.5,
                    (center.y_ + (tool_ == ToolMode::DimensionDiameter ? opposite.y_ : edge.y_)) * 0.5};
                const QString prefix = tool_ == ToolMode::DimensionRadius ? QStringLiteral("R ") :
                                                                  QString::fromUtf8("\xC3\x98 ");
                auto text = std::make_unique<geom::TextEntity>(
                    labelPoint, (prefix + QString::number(
                        tool_ == ToolMode::DimensionDiameter ? radius * 2.0 : radius,
                        'f', 3)).toStdString(), 0.12);
                text->setLayer("Dimensions");
                commitEntity(std::move(line),
                             tool_ == ToolMode::DimensionRadius
                                 ? tr("Cotation de rayon") : tr("Cotation de diamètre"));
                commitEntity(std::move(text), tr("Texte de cotation"));
                toolPoints_.clear();
            }
            break;
        }
    }
    update();
}

void Viewport::submitTypedPoint(const QString& text) {
    const std::string raw = text.toStdString();

    std::optional<Point2> parsed = parseCoordinateInput(raw, activeReferencePoint());
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

    // Pointer une entité existante (Sélection, premier clic de Déplacer)
    // utilise la position brute du curseur ; placer un nouveau point
    // (outils de dessin, destination de Déplacer) utilise la position
    // accrochée pour que la géométrie puisse être ancrée précisément.
    switch (tool_) {
        case ToolMode::Select: {
            bool additive = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);
            geom::Entity* hit = doc_->pickEntity(rawWorld, pickTol);
            if (hit) {
                if (!additive) {
                    for (const auto& e : doc_->entities()) e->selected = false;
                    hit->selected = true;
                } else {
                    hit->selected = !hit->selected; // le clic avec modificateur bascule l'appartenance
                }
            } else {
                // Espace vide : désélectionne maintenant (sauf en mode
                // additif) et démarre un glissement de fenêtre de
                // sélection ; la direction (gauche-à-droite ou
                // droite-à-gauche) est décidée au relâchement, une fois le
                // point final connu — voir mouseReleaseEvent.
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
    emit cursorWorldPositionChanged(world.x_, world.y_);

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
        // Ignore les micro-glissements accidentels — traité comme le clic
        // sur espace vide qu'il était visuellement (sélection déjà
        // effacée au moment de l'appui).
        if (doc_ && (endScreen - rubberBandStartScreen_).manhattanLength() > 3) {
            Point2 p1 = toWorld(rubberBandStartScreen_);
            Point2 p2 = toWorld(endScreen);
            double x1 = p1.x_, y1 = p1.y_;
            double x2 = p2.x_, y2 = p2.y_;
            geom::BoundingBox worldRect{ std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2) };

            // Glissement de gauche à droite = fenêtre (entièrement englobé
            // uniquement) ; de droite à gauche = capture (tout ce qui est
            // touché) — la convention AutoCAD standard.
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
                              tool_ == ToolMode::Polyline || tool_ == ToolMode::Rectangle ||
                              tool_ == ToolMode::Point ||
                              tool_ == ToolMode::DimensionLinear ||
                              tool_ == ToolMode::DimensionAligned ||
                              tool_ == ToolMode::DimensionAngular ||
                              tool_ == ToolMode::DimensionRadius ||
                              tool_ == ToolMode::DimensionDiameter;

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
        // Taper une coordonnée directement dans le viewport active la
        // ligne de commande, initialisée avec ce qui vient d'être tapé —
        // reproduit la saisie dynamique d'AutoCAD plutôt que d'exiger un
        // clic préalable dans le champ.
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
        } else if (tool_ == ToolMode::Rectangle && toolPoints_.size() == 1) {
            painter.drawRect(QRectF(last, cur));
        } else if (tool_ == ToolMode::Polyline) {
            for (std::size_t i = 1; i < toolPoints_.size(); ++i) {
                painter.drawLine(toScreen(toolPoints_[i - 1]), toScreen(toolPoints_[i]));
            }
            painter.drawLine(last, cur);
        } else if ((tool_ == ToolMode::DimensionLinear ||
                    tool_ == ToolMode::DimensionAligned) &&
                   toolPoints_.size() >= 2) {
            const Point2& a = toolPoints_[0];
            const Point2& b = toolPoints_[1];
            const geom::Vector2 base = b - a;
            const double length = geom::length(base);
            if (length >= geom::Tolerance::kDegenerateLength) {
                const geom::Vector2 normal{-base.y_ / length, base.x_ / length};
                const double distance = tool_ == ToolMode::DimensionAligned
                    ? 0.0 : geom::dot(*hoverWorld_ - a, normal);
                const Point2 da{a.x_ + normal.x_ * distance, a.y_ + normal.y_ * distance};
                const Point2 db{b.x_ + normal.x_ * distance, b.y_ + normal.y_ * distance};
                painter.drawLine(toScreen(a), toScreen(da));
                painter.drawLine(toScreen(b), toScreen(db));
                painter.drawLine(toScreen(da), toScreen(db));
                painter.setPen(QColor(255, 230, 80));
                painter.drawText(toScreen(Point2{(da.x_ + db.x_) * 0.5,
                                                 (da.y_ + db.y_) * 0.5}),
                                 QString::fromStdString(layout::Dimension{a, b, length,
                                     Point2{(a.x_ + b.x_) * 0.5, (a.y_ + b.y_) * 0.5}}.text()));
            }
        } else if (tool_ == ToolMode::DimensionAngular && toolPoints_.size() >= 2) {
            const Point2& vertex = toolPoints_[0];
            const double radius = geom::distance(vertex, toolPoints_[1]);
            const double start = geom::angleOf(vertex, toolPoints_[1]);
            const double end = geom::angleOf(vertex, *hoverWorld_);
            painter.drawLine(toScreen(vertex), toScreen(toolPoints_[1]));
            painter.drawLine(toScreen(vertex), toScreen(*hoverWorld_));
            painter.drawArc(QRectF(toScreen(Point2{vertex.x_ - radius, vertex.y_ - radius}),
                                   toScreen(Point2{vertex.x_ + radius, vertex.y_ + radius})),
                            static_cast<int>(-start * 180.0 / std::numbers::pi * 16),
                            static_cast<int>((end - start) * 180.0 / std::numbers::pi * 16));
        } else if ((tool_ == ToolMode::DimensionRadius ||
                    tool_ == ToolMode::DimensionDiameter) &&
                   toolPoints_.size() == 1) {
            const Point2& center = toolPoints_[0];
            const Point2& edge = *hoverWorld_;
            const Point2 other{center.x_ - (edge.x_ - center.x_),
                               center.y_ - (edge.y_ - center.y_)};
            painter.drawLine(toScreen(center),
                             toScreen(tool_ == ToolMode::DimensionRadius ? edge : other));
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

    // La forme indique le type d'accrochage, selon la convention utilisée
    // par la plupart des outils CAO : carré = extrémité, cercle = centre,
    // triangle = milieu, losange = intersection, équerre = perpendiculaire,
    // X = grille.
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
            // Sablier, le glyphe conventionnel NEA (point le plus proche).
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

    // Axes d'origine, plus lumineux, quand ils sont dans le champ de vue
    // — un repère visuel fixe.
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

    // Fenêtre (de gauche à droite) se lit comme un rectangle bleu plein ;
    // capture (de droite à gauche) comme un rectangle vert en pointillés
    // — la convention AutoCAD.
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
