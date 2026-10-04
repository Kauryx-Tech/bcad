// Les outils qui créent de la géométrie : ils accumulent des points jusqu'à
// ce que la forme soit complète, puis la commettent. `placePoint` est le seul
// point d'entrée — un clic de souris après accrochage (ViewportInput) comme
// une coordonnée tapée (submitTypedPoint) y aboutissent, ce qui garantit que
// les deux chemins produisent la même entité.

#include "Viewport.h"

#include "CoordinateInput.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/layout/Dimension.h"
#include <QString>
#include <QUndoStack>
#include <cmath>
#include <memory>
#include <numbers>

namespace bcad::app {

using geom::Point2;

void Viewport::cancelActiveTool() {
    toolPoints_.clear();
    pickingObjects_ = false;
    notifyPrompt();
}

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
            finishPolyline(false);
            return;
        default:
            endCommand();
            return;
    }
}

// Entree (ou clic droit, ou ligne de commande vide) termine une polyligne
// ouverte ; C la ferme, comme l'option Clore d'AutoCAD. Fermer demande trois
// sommets : avec deux, ce serait un aller-retour sur le meme segment.
void Viewport::finishPolyline(bool closed) {
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
            toolPoints_.push_back(world);
            break;
        case ToolMode::Rectangle:
            placeRectangle(world);
            break;
        case ToolMode::Point:
            placePointEntity(world);
            break;
        case ToolMode::DimensionLinear:
        case ToolMode::DimensionAligned:
            placeDimensionLinearOrAligned(world);
            break;
        case ToolMode::DimensionAngular:
            placeDimensionAngular(world);
            break;
        case ToolMode::DimensionRadius:
        case ToolMode::DimensionDiameter:
            placeDimensionRadial(world);
            break;
    }
    notifyPrompt();
    update();
}

void Viewport::submitTypedPoint(const QString& text) {
    // Ligne vide = Entree (valider, terminer, ou relancer la derniere commande) ;
    // « C » clot la polyligne, comme dans AutoCAD.
    const QString option = text.trimmed();
    if (option.isEmpty()) {
        pressEnter(/*fromRightClick=*/false);
        update();
        return;
    }
    if (tool_ == ToolMode::Polyline &&
        option.compare(QStringLiteral("C"), Qt::CaseInsensitive) == 0) {
        finishPolyline(true);
        return;
    }
    if (pickingObjects_) return;   // les objets se designent a la souris
    const std::string raw = text.toStdString();

    std::optional<Point2> parsed = parseCoordinateInput(raw, activeReferencePoint());
    if (!parsed) return;
    activeSnap_ = {};
    placePoint(*parsed);
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

void Viewport::placeDimensionLinearOrAligned(const Point2& world) {
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
            return;
        }
        ensureDimensionLayer();
        const geom::Vector2 normal{-base.y_ / length, base.x_ / length};
        const double distance = tool_ == ToolMode::DimensionAligned
            ? 0.0 : geom::dot(toolPoints_[2] - a, normal);
        const Point2 da{a.x_ + normal.x_ * distance, a.y_ + normal.y_ * distance};
        const Point2 db{b.x_ + normal.x_ * distance, b.y_ + normal.y_ * distance};
        auto extensionA = std::make_unique<geom::LineEntity>(a, da);
        auto extensionB = std::make_unique<geom::LineEntity>(b, db);
        auto dimensionLine = std::make_unique<geom::LineEntity>(da, db);
        extensionA->setLayer("Cotations");
        extensionB->setLayer("Cotations");
        dimensionLine->setLayer("Cotations");
        const Point2 labelPoint{
            (da.x_ + db.x_) * 0.5 + normal.x_ * 0.15,
            (da.y_ + db.y_) * 0.5 + normal.y_ * 0.15};
        const std::string label = layout::Dimension{
            a, b, length, Point2{(a.x_ + b.x_) * 0.5, (a.y_ + b.y_) * 0.5}}.text();
        auto text = std::make_unique<geom::TextEntity>(labelPoint, label, 0.12);
        text->setLayer("Cotations");
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
        endCommand();
    }
}

void Viewport::placeDimensionAngular(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 3) {
        const Point2& vertex = toolPoints_[0];
        const Point2& start = toolPoints_[1];
        const Point2& end = toolPoints_[2];
        const double radius = geom::distance(vertex, start);
        if (radius < geom::Tolerance::kDegenerateLength) {
            toolPoints_.clear();
            return;
        }
        ensureDimensionLayer();
        const double startAngle = geom::angleOf(vertex, start);
        const double endAngle = geom::angleOf(vertex, end);
        auto arc = std::make_unique<geom::ArcEntity>(
            vertex, radius, startAngle, endAngle);
        auto rayA = std::make_unique<geom::LineEntity>(vertex, start);
        auto rayB = std::make_unique<geom::LineEntity>(vertex, end);
        rayA->setLayer("Cotations");
        rayB->setLayer("Cotations");
        arc->setLayer("Cotations");
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
        text->setLayer("Cotations");
        commitEntity(std::move(text), tr("Texte de cotation"));
        if (undoStack_) undoStack_->endMacro();
        endCommand();
    }
}

void Viewport::placeDimensionRadial(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() == 2) {
        const Point2& center = toolPoints_[0];
        const Point2& edge = toolPoints_[1];
        const double radius = geom::distance(center, edge);
        if (radius < geom::Tolerance::kDegenerateLength) {
            toolPoints_.clear();
            return;
        }
        ensureDimensionLayer();
        const Point2 opposite{center.x_ - (edge.x_ - center.x_),
                              center.y_ - (edge.y_ - center.y_)};
        auto line = std::make_unique<geom::LineEntity>(
            center, tool_ == ToolMode::DimensionDiameter ? opposite : edge);
        line->setLayer("Cotations");
        const Point2 labelPoint{
            (center.x_ + (tool_ == ToolMode::DimensionDiameter ? opposite.x_ : edge.x_)) * 0.5,
            (center.y_ + (tool_ == ToolMode::DimensionDiameter ? opposite.y_ : edge.y_)) * 0.5};
        const QString prefix = tool_ == ToolMode::DimensionRadius ? QStringLiteral("R ") :
                                                          QString::fromUtf8("\xC3\x98 ");
        auto text = std::make_unique<geom::TextEntity>(
            labelPoint, (prefix + QString::number(
                tool_ == ToolMode::DimensionDiameter ? radius * 2.0 : radius,
                'f', 3)).toStdString(), 0.12);
        text->setLayer("Cotations");
        const QString label = tool_ == ToolMode::DimensionRadius
                                  ? tr("Cotation de rayon") : tr("Cotation de diamètre");
        if (undoStack_) undoStack_->beginMacro(label);
        commitEntity(std::move(line), label);
        commitEntity(std::move(text), tr("Texte de cotation"));
        if (undoStack_) undoStack_->endMacro();
        endCommand();
    }
}

} // namespace bcad::app
