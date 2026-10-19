// Variantes de Cercle ▾ et Arc ▾, comme AutoCAD : cercle centre-diametre,
// 2 points (extremites d'un diametre), 3 points ; arc 3 points (depart, point
// de passage, fin). Les options se tapent aussi pendant la commande : « 2P »,
// « 3P », « D » dans CERCLE ; « C » (centre) dans ARC 3 points. Geometrie :
// CircleDetail.h, partagee avec l'apercu.

#include "Viewport.h"

#include "CircleDetail.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"

#include <QPainter>

#include <memory>

namespace bcad::app {

using geom::Point2;

namespace {

// Cercle ou arc defini par les points saisis et, pour l'apercu, le curseur.
std::unique_ptr<geom::Entity> variantEntity(ToolMode tool, const std::vector<Point2>& p) {
    switch (tool) {
        case ToolMode::CircleDiameter:
            if (p.size() < 2) return nullptr;
            if (geom::distance(p[0], p[1]) < geom::Tolerance::kDegenerateLength) return nullptr;
            return std::make_unique<geom::CircleEntity>(p[0], geom::distance(p[0], p[1]) / 2.0);
        case ToolMode::Circle2P:
            if (p.size() < 2) return nullptr;
            if (auto c = detail::circleFrom2P(p[0], p[1])) return std::make_unique<geom::CircleEntity>(c->center, c->radius);
            return nullptr;
        case ToolMode::Circle3P:
            if (p.size() < 3) return nullptr;
            if (auto c = detail::circleFrom3P(p[0], p[1], p[2]))
                return std::make_unique<geom::CircleEntity>(c->center, c->radius);
            return nullptr;
        case ToolMode::Arc3P:
            if (p.size() < 3) return nullptr;
            if (auto a = detail::arcFrom3P(p[0], p[1], p[2]))
                return std::make_unique<geom::ArcEntity>(a->center, a->radius, a->startAngle, a->endAngle);
            return nullptr;
        default:
            return nullptr;
    }
}

std::size_t pointsNeeded(ToolMode tool) {
    return (tool == ToolMode::Circle3P || tool == ToolMode::Arc3P) ? 3 : 2;
}

} // namespace

void Viewport::placeCircleVariant(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() < pointsNeeded(tool_)) return;
    std::unique_ptr<geom::Entity> entity = variantEntity(tool_, toolPoints_);
    if (!entity) {
        emit statusMessage(tr("Points alignés ou confondus : aucun cercle ne passe par eux."));
        toolPoints_.pop_back();
        return;
    }
    const bool isArc = tool_ == ToolMode::Arc3P;
    commitEntity(std::move(entity), isArc ? tr("Arc") : tr("Cercle"));
    endCommand();
}

bool Viewport::submitCircleOption(const QString& text) {
    const QString option = text.trimmed().toUpper();
    if (tool_ == ToolMode::Circle) {
        if (toolPoints_.empty() && option == QLatin1String("2P")) { setTool(ToolMode::Circle2P); return true; }
        if (toolPoints_.empty() && option == QLatin1String("3P")) { setTool(ToolMode::Circle3P); return true; }
        if (toolPoints_.size() == 1 && (option == QLatin1String("D") || option == QLatin1String("DIAMETRE") ||
                                        option == QStringLiteral("DIAMÈTRE"))) {
            // Le centre deja saisi est garde : seul le sens du point suivant change.
            tool_ = ToolMode::CircleDiameter;
            emit toolChanged(tool_);
            notifyPrompt();
            update();
            return true;
        }
    }
    if (tool_ == ToolMode::Arc3P && toolPoints_.empty() &&
        (option == QLatin1String("C") || option == QLatin1String("CENTRE"))) {
        setTool(ToolMode::Arc);
        return true;
    }
    return false;
}

bool Viewport::drawCircleVariantPreview(QPainter& painter) {
    if (!hoverWorld_ || toolPoints_.empty()) return false;
    if (tool_ != ToolMode::CircleDiameter && tool_ != ToolMode::Circle2P && tool_ != ToolMode::Circle3P &&
        tool_ != ToolMode::Arc3P) {
        return false;
    }
    std::vector<Point2> pts = toolPoints_;
    pts.push_back(*hoverWorld_);
    auto toScreen = [this](const Point2& p) {
        render::ScreenPoint s = camera_.worldToScreen(p);
        return QPointF(s.x, s.y);
    };
    if (pts.size() < pointsNeeded(tool_)) {
        painter.drawLine(toScreen(pts[pts.size() - 2]), toScreen(pts.back()));
        return true;
    }
    if (auto ghost = variantEntity(tool_, pts)) {
        QPolygonF path;
        for (const auto& p : ghost->tessellate(0.5 / camera_.pixelsPerUnit())) path << toScreen(p);
        painter.drawPolyline(path);
    }
    return true;
}

} // namespace bcad::app
