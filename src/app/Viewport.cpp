// La surface de dessin elle-meme : ce qu'elle possede et comment elle rend.
//
// Classe repartie sur dix unites de traduction par responsabilite, sans
// changement de comportement. Cette liste est la seule copie : l'en-tete
// src/app/Viewport.h n'en reprend que le principe, une table recopiee a deux
// endroits derive.
//   Viewport.cpp             cycle de vie, document, camera, rendu GL,
//                            tessellation d'arriere-plan, accrochage
//   ViewportInput.cpp        souris, molette, clavier, Entree et ligne de
//                            commande
//   ViewportOverlay.cpp      toute la surimpression QPainter : grille, textes
//                            d'entites, apercu d'outil, marqueur d'accrochage,
//                            rectangle de capture
//   ViewportEditing.cpp      operations sur la selection : supprimer, eclater,
//                            joindre, tout selectionner, operation booleanne
//   ViewportDrawTools.cpp    machine a etats des outils qui creent de la
//                            geometrie (trait, cercle, arc, rectangle, point)
//   ViewportTransformTools.cpp  outils qui deplacent, copient, tournent,
//                            dimensionnent ou symetrisent la selection
//   ViewportCutTools.cpp     outils qui rognent, prolongent ou rompent une
//                            entite designee au clic
//   ViewportPrompts.cpp      consigne de chaque etape d'outil (ligne de
//                            commande)
//   ViewportTextTools.cpp    outil Texte et modification d'un texte
//   ViewportDimensionTools.cpp  outils de cotation (un objet par cote)
//   ViewportMeasureTools.cpp    mesures : distance, angle, surface (rien n'est cree)
//   ViewportFilletTools.cpp     raccord : deux objets designes, arc ou coin vif
//   ViewportNavigation.cpp      zoom fenetre, zoom precedent / suivant, main
//   ViewportOffsetTools.cpp     decaler : distance ou point, objet, cote
//   ViewportCircleTools.cpp     variantes de cercle et d'arc (2P, 3P, diametre)
//   ViewportDynamicInput.cpp    saisie dynamique pres du curseur (F12)
// Seuls les corps changent de fichier : l'en-tete porte Q_OBJECT et reste
// unique, le moc d'une classe ne se decoupant pas. La tolerance de pointage,
// seule valeur commune aux unites, vient de ViewportTolerances.h.

#include "theme/Theme.h"
#include "Viewport.h"

#include "TrackingDetail.h"

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
    // L'historique des vues appartient au dessin quitte.
    viewBack_.clear();
    viewForward_.clear();
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
    // Une saisie de contour n'appartient qu'a la commande qui l'a demandee :
    // elle ne se relance pas par Entree, et changer d'outil l'abandonne.
    if (mode != ToolMode::Select && mode != ToolMode::CapturePolygon) {
        lastCommand_ = mode;
        std::erase(recentCommands_, mode);
        recentCommands_.insert(recentCommands_.begin(), mode);
        if (recentCommands_.size() > 5) recentCommands_.pop_back();
    }
    if (mode != ToolMode::CapturePolygon) captureDone_ = nullptr;
    // Une modification lancee sans selection commence par designer ses objets,
    // a la souris ; avec une selection prealable, elle part directement.
    const bool actsOnSelection = mode == ToolMode::Move || mode == ToolMode::Copy ||
                                 mode == ToolMode::Rotate || mode == ToolMode::Scale ||
                                 mode == ToolMode::Mirror || mode == ToolMode::Erase ||
                                 mode == ToolMode::Explode || mode == ToolMode::Join;
    pickingObjects_ = actsOnSelection && selectedEntities().empty();
    if (mode == ToolMode::Pan) setCursor(Qt::OpenHandCursor);
    else unsetCursor();
    emit toolChanged(tool_);
    notifyPrompt();
    update();
}

void Viewport::endCommand() {
    setTool(ToolMode::Select);
}

void Viewport::capturePolygon(const QString& prompt,
                              std::function<void(std::vector<geom::Point2>)> done) {
    capturePrompt_ = prompt;
    setTool(ToolMode::CapturePolygon);
    captureDone_ = std::move(done);
    notifyPrompt();
}

void Viewport::setCamera(const render::Camera2D& camera) {
    const int width = camera_.viewportWidth();
    const int height = camera_.viewportHeight();
    camera_ = camera;
    camera_.setViewportSize(width, height);
    requestTessellation();
    update();
}

void Viewport::zoomToFit() {
    if (!doc_) return;
    rememberView(false);
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
    painter.fillRect(rect(), theme::color(theme::Token::Canvas)); // fond de l'espace objet AutoCAD sombre
    if (gridVisible_) drawGrid(painter);

    painter.beginNativePainting();
    renderer_.render(camera_, devicePixelRatioF());
    painter.endNativePainting();

    painter.save();
    drawEntityTexts(painter);
    painter.restore();

    drawToolPreview(painter);
    drawPolarTracking(painter);
    drawDynamicInput(painter);
    drawSnapMarker(painter);
    drawRubberBand(painter);
}

double Viewport::pickTolerance() const {
    return kPickToleranceScreenPx / camera_.pixelsPerUnit();
}

geom::Entity* Viewport::pickEntityAt(const Point2& world) const {
    return doc_ ? doc_->pickEntity(world, pickTolerance()) : nullptr;
}

QPointF Viewport::toScreenF(const Point2& world) const {
    const render::ScreenPoint s = camera_.worldToScreen(world);
    return QPointF(s.x, s.y);
}

void Viewport::drawGhost(QPainter& painter, const geom::Entity& entity) const {
    QPolygonF path;
    for (const auto& p : entity.tessellate(0.5 / camera_.pixelsPerUnit())) path << toScreenF(p);
    if (path.size() == 2) painter.drawLine(path.at(0), path.at(1));
    else if (!path.isEmpty()) painter.drawPolyline(path);
}

Point2 Viewport::toWorld(QPoint screenPos) const {
    return camera_.screenToWorld({ static_cast<double>(screenPos.x()), static_cast<double>(screenPos.y()) });
}

std::optional<Point2> Viewport::activeReferencePoint() const {
    if (!toolPoints_.empty()) return toolPoints_.back();
    // Coordonnees (I-06) : « @dx,dy » part du dernier point identifie.
    if (tool_ == ToolMode::Coordinates && idLast_) return idLast_;
    return std::nullopt;
}

Point2 Viewport::snappedWorld(QPoint screenPos) {
    Point2 world = toWorld(screenPos);
    activeSnap_ = {};
    polarPoint_.reset();

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

    // Reperage polaire (AutoCAD : apres l'accrochage aux objets, exclusif
    // d'Ortho) : direction multiple de l'increment, a ~10 px du curseur.
    if (polarEnabled_ && !orthoEnabled_ && tool_ != ToolMode::Select) {
        if (auto ref = activeReferencePoint()) {
            if (auto hit = detail::polarTrack(*ref, world, polarIncrementDeg_, 10.0 / camera_.pixelsPerUnit())) {
                polarRef_ = *ref;
                polarPoint_ = hit->point;
                polarDistance_ = hit->distance;
                polarAngleDeg_ = hit->angleDeg;
                return hit->point;
            }
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
    // Un objet dessine va sur le calque courant, comme dans AutoCAD. Une
    // entite nait sur « 0 » : sans cette ligne, tout le dessin restait sur
    // « 0 » quel que soit le calque choisi. Un calque fixe par l'outil
    // (Cotations) est garde.
    if (entity->layer() == "0") entity->setLayer(doc_->layerManager().currentLayerName());
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
