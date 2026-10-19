// Les périphériques d'entrée de la surface de dessin : ce qu'un clic, un
// glissement, une molette ou une touche déclenchent. La décision de *ce que
// fait l'outil* n'est pas ici — elle est dans ViewportDrawTools.cpp,
// ViewportTransformTools.cpp et ViewportCutTools.cpp — mais bien la
// distinction entre pointer une entité existante (position brute) et poser un
// point (position accrochée).

#include "Viewport.h"
#include "ToolAliases.h"

#include "CoordinateInput.h"

#include "ViewportTolerances.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace bcad::app {

using geom::Point2;

void Viewport::mousePressEvent(QMouseEvent* event) {
    // Bouton du milieu, ou bouton gauche avec la main (V-03) : panoramique.
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && tool_ == ToolMode::Pan)) {
        rememberView(false);
        panning_ = true;
        lastMousePos_ = event->pos();
        if (tool_ == ToolMode::Pan) setCursor(Qt::ClosedHandCursor);
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
    // Raccord, et objet a decaler : le clic designe un objet (et le cote a
    // garder) ; l'accrochage le ramenerait sur un sommet commun.
    if (tool_ == ToolMode::Fillet || (tool_ == ToolMode::Offset && offsetObjectId_ < 0)) {
        activeSnap_ = {};
        placePoint(toWorld(event->pos()));
        return;
    }
    placePoint(snappedWorld(event->pos()));
}

void Viewport::selectAt(QMouseEvent* event, bool addByDefault) {
    const Point2 rawWorld = toWorld(event->pos());
    const double pickTol = pickTolerance();
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
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && tool_ == ToolMode::Pan)) {
        panning_ = false;
        if (tool_ == ToolMode::Pan) setCursor(Qt::OpenHandCursor);
    }
    // Zoom fenetre en glissant : le relachement donne le coin oppose.
    if (event->button() == Qt::LeftButton && tool_ == ToolMode::ZoomWindow && toolPoints_.size() == 1) {
        const auto first = camera_.worldToScreen(toolPoints_[0]);
        if ((QPointF(first.x, first.y) - event->position()).manhattanLength() > 3) {
            placePoint(toWorld(event->pos()));
            return;
        }
    }

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
    rememberView(true);
    double factor = std::pow(1.0015, event->angleDelta().y());
    QPointF pos = event->position();
    camera_.zoomAt(factor, { pos.x(), pos.y() });
    requestTessellation();
    update();
}

void Viewport::keyPressEvent(QKeyEvent* event) {
    bool drawingToolActive = tool_ == ToolMode::Line || tool_ == ToolMode::Circle || tool_ == ToolMode::Arc ||
                              tool_ == ToolMode::CircleDiameter || tool_ == ToolMode::Circle2P ||
                              tool_ == ToolMode::Circle3P || tool_ == ToolMode::Arc3P ||
                              tool_ == ToolMode::Polyline || tool_ == ToolMode::Polygon ||
                              tool_ == ToolMode::Rectangle ||
                              tool_ == ToolMode::CapturePolygon || tool_ == ToolMode::Text ||
                              tool_ == ToolMode::Point ||
                              tool_ == ToolMode::DimensionLinear ||
                              tool_ == ToolMode::DimensionAligned ||
                              tool_ == ToolMode::DimensionAngular ||
                              tool_ == ToolMode::DimensionRadius ||
                              tool_ == ToolMode::DimensionDiameter ||
                              tool_ == ToolMode::Measure || tool_ == ToolMode::Area ||
                              tool_ == ToolMode::Coordinates || tool_ == ToolMode::Fillet ||
                              tool_ == ToolMode::Offset || tool_ == ToolMode::Rotate ||
                              tool_ == ToolMode::Scale ||
                              tool_ == ToolMode::ZoomWindow;

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
    } else if (tool_ == ToolMode::Text && textStage_ == 3 && event->key() == Qt::Key_Space) {
        // Pendant la saisie du texte, l'espace est un caractere, pas Entree.
        emit typedInputRequested(event->text());
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
               (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier)) {
        pressEnter(/*fromRightClick=*/false);
        update();
    } else if (event->key() == Qt::Key_C && event->modifiers() == Qt::NoModifier &&
               (tool_ == ToolMode::Polyline || tool_ == ToolMode::Polygon ||
                tool_ == ToolMode::CapturePolygon) &&
               toolPoints_.size() >= 3) {
        finishPolyline(true);
    } else if (tool_ == ToolMode::Text && textStage_ == 3 && !event->text().isEmpty() &&
               event->text().at(0).isPrint()) {
        // Le contenu du texte se tape dans la ligne de commande : la premiere
        // touche y est reportee.
        emit typedInputRequested(event->text());
    } else if (event->modifiers() == Qt::NoModifier && tool_ == ToolMode::Select && !pickingObjects_ &&
               !event->text().isEmpty() && event->text().at(0).isLetter()) {
        // Taper une commande au repos ouvre la ligne de commande avec la
        // lettre (C-01) ; la validation lancera l'outil via submitTypedPoint.
        // Comme AutoCAD : F lance RACCORD ; le zoom etendu est au double-clic
        // de la molette ou ZE. Les raccourcis a modificateur ne passent pas ici.
        emit typedInputRequested(event->text());
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

// Double-clic sur un texte au repos : le modifier (D-01b), comme AutoCAD.
void Viewport::mouseDoubleClickEvent(QMouseEvent* event) {
    // Double-clic de la molette : zoom etendu, comme AutoCAD.
    if (event->button() == Qt::MiddleButton) {
        zoomToFit();
        return;
    }
    if (event->button() != Qt::LeftButton || !doc_ || tool_ != ToolMode::Select) return;
    const double pickTol = pickTolerance();
    if (geom::Entity* hit = doc_->pickEntity(toWorld(event->pos()), pickTol);
        hit && hit->typeId() == geom::TypeId_Text) {
        editText(hit->id());
    }
}

// Entree, Espace, clic droit ou ligne de commande vide.
void Viewport::pressEnter(bool fromRightClick) {
    if (pickingObjects_) {
        // Les objets designes sont valides : la commande passe a ses points.
        if (!selectedEntities().empty()) pickingObjects_ = false;
        notifyPrompt();
        return;
    }
    switch (tool_) {
        case ToolMode::Select:
            // Au repos, Entree relance la derniere commande (AutoCAD) ; un clic
            // droit au repos ne declenche rien.
            if (!fromRightClick && lastCommand_ != ToolMode::Select) setTool(lastCommand_);
            return;
        case ToolMode::Polyline:
        case ToolMode::Polygon:
        case ToolMode::CapturePolygon:
            finishPolyline(false);
            return;
        case ToolMode::Area:
            // Entree calcule le contour saisi ; sans sommet en cours, elle
            // quitte la commande.
            if (toolPoints_.empty()) endCommand();
            else finishAreaPolygon();
            return;
        case ToolMode::Text:
            // Entree accepte la hauteur ou l'angle proposes ; sur le contenu,
            // elle termine (ligne vide).
            if (textStage_ == 1 || textStage_ == 2) submitTextValue(QString());
            else endCommand();
            return;
        default:
            endCommand();
            return;
    }
}

// Coordonnee ou option tapee dans la ligne de commande.
void Viewport::submitTypedPoint(const QString& text) {
    // Hauteur, angle et lignes de l'outil Texte se tapent dans la ligne de
    // commande : ce ne sont pas des coordonnees.
    if (tool_ == ToolMode::Text && textStage_ >= 1) {
        submitTextValue(text);
        return;
    }
    if (submitDimensionOption(text)) return;
    if (submitMeasureOption(text)) return;
    if (submitFilletOption(text)) return;
    if (submitOffsetOption(text)) return;
    if (submitTransformValue(text)) return;
    if (submitCircleOption(text)) return;
    if (submitModifyOption(text)) return;
    // Ligne vide = Entree (valider, terminer, ou relancer la derniere commande) ;
    // « C » clot la polyligne, comme dans AutoCAD.
    const QString option = text.trimmed();
    if (option.isEmpty()) {
        pressEnter(/*fromRightClick=*/false);
        update();
        return;
    }
    if ((tool_ == ToolMode::Polyline || tool_ == ToolMode::Polygon ||
         tool_ == ToolMode::CapturePolygon) &&
        option.compare(QStringLiteral("C"), Qt::CaseInsensitive) == 0) {
        finishPolyline(true);
        return;
    }
    if (pickingObjects_) return;   // les objets se designent a la souris
    // Au repos, un alias de commande lance l'outil (C-01) : « L », « PL »,
    // « DLI »… Seuls les outils livres ont un alias ; le reste tombe comme
    // avant dans l'analyse de coordonnees, qui l'ignore. Pendant une commande
    // (outil actif, polyligne, cote, texte), les lettres gardent leur sens
    // d'option (C clot, H/V oriente) et ne changent pas d'outil.
    if (tool_ == ToolMode::Select) {
            if (std::optional<ToolMode> alias = toolModeForAlias(option)) {
            setTool(*alias);
            update();
            return;
        }
        // Zoom precedent / suivant (V-02) : immediats, pas d'outil.
        if (option.compare(QStringLiteral("ZE"), Qt::CaseInsensitive) == 0) {
            zoomToFit();
            return;
        }
        if (option.compare(QStringLiteral("ZP"), Qt::CaseInsensitive) == 0) {
            zoomPrevious();
            return;
        }
        if (option.compare(QStringLiteral("ZS"), Qt::CaseInsensitive) == 0) {
            zoomNext();
            return;
        }
        // Alias d'un outil pas encore livré : le dire, avec sa tâche, plutôt
        // que d'ignorer la saisie sans un mot.
        {
            if (const char* missing = futureToolForAlias(option)) {
                emit statusMessage(tr("%1 : outil pas encore livré").arg(QString::fromLatin1(missing)));
                return;
            }
        }
    }
    const std::string raw = text.toStdString();

    std::optional<Point2> parsed = parseCoordinateInput(raw, activeReferencePoint());
    if (!parsed) return;
    activeSnap_ = {};
    placePoint(*parsed);
}

} // namespace bcad::app
