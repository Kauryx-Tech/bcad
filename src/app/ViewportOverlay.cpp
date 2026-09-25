// Tout ce que le viewport peint par-dessus le dessin : la grille avant la
// géométrie, puis les textes d'entités, l'aperçu de l'outil en cours, le
// marqueur d'accrochage et le rectangle de capture. Ces surimpressions sont
// construites directement sur le thread GL/UI avec QPainter, puisqu'il
// s'agit de quelques points — le document lui-même arrive tessellé depuis
// le thread d'arrière-plan (Viewport.cpp).

#include "Viewport.h"

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/layout/Dimension.h"
#include "bcad/render/Grid.h"
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace bcad::app {

using geom::Point2;

void Viewport::drawEntityTexts(QPainter& painter) {
    if (!doc_) return;
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
