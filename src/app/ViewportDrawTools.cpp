// Les outils qui créent de la géométrie : ils accumulent des points jusqu'à
// ce que la forme soit complète, puis la commettent. `placePoint` est le seul
// point d'entrée — un clic de souris après accrochage (ViewportInput) comme
// une coordonnée tapée (submitTypedPoint) y aboutissent, ce qui garantit que
// les deux chemins produisent la même entité.

#include "Viewport.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include <QString>
#include <QUndoStack>
#include <cmath>
#include <memory>

namespace bcad::app {

using geom::Point2;

void Viewport::cancelActiveTool() {
    toolPoints_.clear();
    pickingObjects_ = false;
    textStage_ = 0;
    dimOrientation_ = 0;
    dimRadius_ = 0.0;
    dimTextHeight_ = 0.0;
    notifyPrompt();
}

// Entree (ou clic droit, ou ligne de commande vide) termine une polyligne
// ouverte ; C la ferme, comme l'option Clore d'AutoCAD. Fermer demande trois
// sommets : avec deux, ce serait un aller-retour sur le meme segment.
void Viewport::finishPolyline(bool closed) {
    if (tool_ == ToolMode::CapturePolygon) {
        // Un contour est toujours ferme ; il faut trois sommets pour qu'il
        // delimite quelque chose. La commande du module recoit les sommets
        // une fois le canevas revenu au repos.
        if (toolPoints_.size() < 3) {
            emit statusMessage(tr("Un contour demande au moins trois sommets."));
            return;
        }
        auto done = std::move(captureDone_);
        std::vector<Point2> vertices = toolPoints_;
        endCommand();
        if (done) done(std::move(vertices));
        return;
    }
    if (closed && toolPoints_.size() >= 3) {
        commitEntity(std::make_unique<geom::PolylineEntity>(toolPoints_, true), tr("Polyligne"));
    } else if (!closed && toolPoints_.size() >= 2) {
        commitEntity(std::make_unique<geom::PolylineEntity>(toolPoints_, false), tr("Polyligne"));
    }
    endCommand();
}

void Viewport::placePoint(const Point2& world) {
    if (!doc_) return;

    switch (tool_) {
        case ToolMode::Select:
            break; // aucune signification de placement de point pour Sélection
        case ToolMode::Move:
            applyMove(world);
            break;
        case ToolMode::Copy:
            applyCopy(world);
            break;
        case ToolMode::Rotate:
            applyRotate(world);
            break;
        case ToolMode::Scale:
            applyScale(world);
            break;
        case ToolMode::Mirror:
            applyMirror(world);
            break;
        case ToolMode::Trim:
            applyTrim(world);
            break;
        case ToolMode::Extend:
            applyExtend(world);
            break;
        case ToolMode::Break:
            applyBreak(world);
            break;
        case ToolMode::Line:
            placeLine(world);
            break;
        case ToolMode::Circle:
            placeCircle(world);
            break;
        case ToolMode::Arc:
            placeArc(world);
            break;
        case ToolMode::Polyline:
        case ToolMode::CapturePolygon:
            toolPoints_.push_back(world);
            break;
        case ToolMode::Rectangle:
            placeRectangle(world);
            break;
        case ToolMode::Point:
            placePointEntity(world);
            break;
        case ToolMode::Text:
            placeText(world);
            break;
        case ToolMode::DimensionLinear:
        case ToolMode::DimensionAligned:
        case ToolMode::DimensionAngular:
        case ToolMode::DimensionRadius:
        case ToolMode::DimensionDiameter:
            placeDimension(world);
            break;
    }
    notifyPrompt();
    update();
}

void Viewport::placeLine(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 2) {
        commitEntity(std::make_unique<geom::LineEntity>(toolPoints_[0], toolPoints_[1]), tr("Ligne"));
        // Les segments s'enchainent depuis le dernier point jusqu'a Entree ou
        // Echap, comme la commande LIGNE d'AutoCAD.
        toolPoints_.erase(toolPoints_.begin());
    }
}

void Viewport::placeCircle(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 2) {
        double r = geom::distance(toolPoints_[0], toolPoints_[1]);
        commitEntity(std::make_unique<geom::CircleEntity>(toolPoints_[0], r), tr("Cercle"));
        endCommand();
    }
}

void Viewport::placeArc(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 3) {
        double r = geom::distance(toolPoints_[0], toolPoints_[1]);
        double startAngle = geom::angleOf(toolPoints_[0], toolPoints_[1]);
        double endAngle = geom::angleOf(toolPoints_[0], toolPoints_[2]);
        commitEntity(std::make_unique<geom::ArcEntity>(toolPoints_[0], r, startAngle, endAngle), tr("Arc"));
        endCommand();
    }
}

void Viewport::placeRectangle(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 2) {
        const Point2& p0 = toolPoints_[0];
        const Point2& p1 = toolPoints_[1];
        std::vector<Point2> corners{
            p0, Point2(p1.x(), p0.y()), p1, Point2(p0.x(), p1.y()),
        };
        commitEntity(std::make_unique<geom::PolylineEntity>(std::move(corners), true), tr("Rectangle"));
        endCommand();
    }
}

void Viewport::placePointEntity(const Point2& world) {
    commitEntity(std::make_unique<geom::PointEntity>(world), tr("Point"));
    endCommand();
}

} // namespace bcad::app
