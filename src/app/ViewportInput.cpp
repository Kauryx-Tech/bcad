// Les périphériques d'entrée de la surface de dessin : ce qu'un clic, un
// glissement, une molette ou une touche déclenchent. La décision de *ce que
// fait l'outil* n'est pas ici — elle est dans ViewportDrawTools.cpp,
// ViewportTransformTools.cpp et ViewportCutTools.cpp — mais bien la
// distinction entre pointer une entité existante (position brute) et poser un
// point (position accrochée).

#include "Viewport.h"

#include "ViewportTolerances.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace bcad::app {

using geom::Point2;

void Viewport::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastMousePos_ = event->pos();
        return;
    }

    // Clic droit = Entree, comme AutoCAD sans menu contextuel : valide la
    // designation des objets, termine la polyligne ou la ligne, ou la commande.
    if (event->button() == Qt::RightButton) {
        pressEnter(/*fromRightClick=*/true);
        update();
        return;
    }

    if (event->button() != Qt::LeftButton || !doc_) return;

    // Au repos (aucune commande) comme pendant la designation des objets d'une
    // commande, la souris selectionne : clic sur un objet ou fenetre glissee.
    // Sinon le clic pose un point, accroche, pour l'outil en cours.
    if (tool_ == ToolMode::Select || pickingObjects_) {
        selectAt(event, /*addByDefault=*/pickingObjects_);
        return;
    }
    placePoint(snappedWorld(event->pos()));
}

void Viewport::selectAt(QMouseEvent* event, bool addByDefault) {
    const Point2 rawWorld = toWorld(event->pos());
    const double pickTol = kPickToleranceScreenPx / camera_.pixelsPerUnit();
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool toggle = event->modifiers() & Qt::ControlModifier;
    geom::Entity* hit = doc_->pickEntity(rawWorld, pickTol);
    if (hit) {
        if (addByDefault) {
            hit->selected = !shift;            // Maj retire de la selection
        } else if (shift || toggle) {
            hit->selected = !hit->selected;    // le clic avec modificateur bascule l'appartenance
        } else {
            for (const auto& e : doc_->entities()) e->selected = false;
            hit->selected = true;
        }
    } else {
        // Espace vide : demarre une fenetre de selection ; sa direction (gauche
        // vers droite ou l'inverse) est decidee au relachement — voir
        // mouseReleaseEvent. Au repos, sans modificateur, la selection
        // precedente est d'abord effacee.
        if (!addByDefault && !shift && !toggle) {
            for (const auto& e : doc_->entities()) e->selected = false;
        }
        rubberBandActive_ = true;
        rubberBandStartScreen_ = event->pos();
    }
    emit selectionChanged();
    notifyPrompt();
    update();
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
            notifyPrompt();
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
                              tool_ == ToolMode::CapturePolygon ||
                              tool_ == ToolMode::Point ||
                              tool_ == ToolMode::DimensionLinear ||
                              tool_ == ToolMode::DimensionAligned ||
                              tool_ == ToolMode::DimensionAngular ||
                              tool_ == ToolMode::DimensionRadius ||
                              tool_ == ToolMode::DimensionDiameter;

    if (event->key() == Qt::Key_Escape) {
        // Echap termine la commande en cours ; au repos, il vide la selection.
        if (tool_ != ToolMode::Select) {
            endCommand();
        } else if (doc_) {
            for (const auto& e : doc_->entities()) e->selected = false;
            emit selectionChanged();
            notifyPrompt();
        }
        update();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
               (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier)) {
        pressEnter(/*fromRightClick=*/false);
        update();
    } else if (event->key() == Qt::Key_C && event->modifiers() == Qt::NoModifier &&
               (tool_ == ToolMode::Polyline || tool_ == ToolMode::CapturePolygon) &&
               toolPoints_.size() >= 3) {
        finishPolyline(true);
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

} // namespace bcad::app
