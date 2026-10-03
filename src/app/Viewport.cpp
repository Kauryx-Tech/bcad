// La surface de dessin elle-meme : ce qu'elle possede et comment elle rend.
//
// Classe repartie sur sept unites de traduction par responsabilite, sans
// changement de comportement. Cette liste est la seule copie : l'en-tete
// src/app/Viewport.h n'en reprend que le principe, une table recopiee a deux
// endroits derive.
//   Viewport.cpp             cycle de vie, document, camera, rendu GL,
//                            tessellation d'arriere-plan, accrochage
//   ViewportInput.cpp        souris, molette, clavier
//   ViewportOverlay.cpp      toute la surimpression QPainter : grille, textes
//                            d'entites, apercu d'outil, marqueur d'accrochage,
//                            rectangle de capture
//   ViewportEditing.cpp      operations sur la selection : supprimer, eclater,
//                            joindre, tout selectionner, operation booleanne
//   ViewportDrawTools.cpp    machine a etats des outils qui creent de la
//                            geometrie (trait, cercle, arc, rectangle, point,
//                            cotations)
//   ViewportTransformTools.cpp  outils qui deplacent, copient, tournent,
//                            dimensionnent ou symetrisent la selection
//   ViewportCutTools.cpp     outils qui rognent, prolongent ou rompent une
//                            entite designee au clic
// Seuls les corps changent de fichier : l'en-tete porte Q_OBJECT et reste
// unique, le moc d'une classe ne se decoupant pas. La tolerance de pointage,
// seule valeur commune aux unites, vient de ViewportTolerances.h.

#include "Viewport.h"

#include "Commands.h"
#include "TessellationWorker.h"
#include "ViewportTolerances.h"
#include "bcad/events/EventBus.h"
#include "bcad/render/Grid.h"
#include "bcad/render/LevelOfDetail.h"
#include <QPainter>
#include <QUndoStack>
#include <algorithm>

namespace bcad::app {

using geom::Point2;

namespace {
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
    painter.fillRect(rect(), QColor(0x21, 0x28, 0x30)); // fond de l'espace objet AutoCAD sombre
    if (gridVisible_) drawGrid(painter);

    painter.beginNativePainting();
    renderer_.render(camera_);
    painter.endNativePainting();

    painter.save();
    drawEntityTexts(painter);
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

} // namespace bcad::app
