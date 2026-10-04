// Tout ce que le viewport peint par-dessus le dessin : la grille avant la
// géométrie, puis les textes d'entités, l'aperçu de l'outil en cours, le
// marqueur d'accrochage et le rectangle de capture. Ces surimpressions sont
// construites directement sur le thread GL/UI avec QPainter, puisqu'il
// s'agit de quelques points — le document lui-même arrive tessellé depuis
// le thread d'arrière-plan (Viewport.cpp).

#include "Viewport.h"

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/DimensionGraphics.h"
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
        // Un calque masque cache aussi ses textes et ses cotes.
        const layers::Layer* layer = doc_->layerManager().find(entity->layer());
        if (layer && !layer->visible) continue;
        if (const auto* dim = dynamic_cast<const geom::DimensionEntity*>(entity.get())) {
            const geom::Color c = entity->colorOverride().value_or(layer ? layer->color : geom::Color{});
            drawDimensionLabel(painter, geom::dimensionGraphics(*dim).label, QColor::fromRgbF(c.r, c.g, c.b));
            continue;
        }
        const auto* text = dynamic_cast<const geom::TextEntity*>(entity.get());
        if (!text) continue;
        const auto screen = camera_.worldToScreen(text->position());
        painter.setPen(QColor(235, 235, 235));
        QFont font;
        font.setPointSizeF(std::max(7.0, text->height() * camera_.pixelsPerUnit() * 0.75));
        painter.setFont(font);
        // L'angle du texte : rotation autour du point d'insertion (l'ecran a
        // l'axe Y vers le bas, d'ou le signe).
        painter.save();
        painter.translate(screen.x, screen.y);
        painter.rotate(-text->rotation() * 180.0 / std::numbers::pi);
        painter.drawText(QPointF(0, 0), QString::fromStdString(text->text()));
        painter.restore();
    }
}

// Texte d'une cotation : centre sur sa ligne de base, a l'angle de la cote.
void Viewport::drawDimensionLabel(QPainter& painter, const geom::DimensionLabel& label,
                                  const QColor& color) {
    const auto screen = camera_.worldToScreen(label.position);
    QFont font;
    font.setPointSizeF(std::max(7.0, label.height * camera_.pixelsPerUnit() * 0.75));
    painter.save();
    painter.setFont(font);
    painter.setPen(color);
    painter.translate(screen.x, screen.y);
    painter.rotate(-label.angle * 180.0 / std::numbers::pi);
    const QString text = QString::fromStdString(label.text);
    painter.drawText(QPointF(-painter.fontMetrics().horizontalAdvance(text) / 2.0, 0), text);
    painter.restore();
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

    // Cotation : l'objet qui sera pose, dessine sous le curseur.
    if (isDimensionTool() && hoverWorld_) {
        std::vector<Point2> pts = toolPoints_;
        pts.push_back(*hoverWorld_);
        if (const auto preview = dimensionFromPoints(pts)) {
            const auto graphics = geom::dimensionGraphics(*preview);
            QPolygonF path;
            for (const auto& p : graphics.path) path << toScreen(p);
            painter.drawPolyline(path);
            drawDimensionLabel(painter, graphics.label, QColor(255, 230, 80));
            return;
        }
    }

    if (!toolPoints_.empty() && hoverWorld_) {
        QPointF last = toScreen(toolPoints_.back());
        QPointF cur = toScreen(*hoverWorld_);

        if (tool_ == ToolMode::Circle && toolPoints_.size() == 1) {
            double r = geom::distance(toolPoints_[0], *hoverWorld_) * camera_.pixelsPerUnit();
            painter.drawEllipse(last, r, r);
        } else if (tool_ == ToolMode::Rectangle && toolPoints_.size() == 1) {
            painter.drawRect(QRectF(last, cur));
        } else if (tool_ == ToolMode::Polyline || tool_ == ToolMode::CapturePolygon) {
            for (std::size_t i = 1; i < toolPoints_.size(); ++i) {
                painter.drawLine(toScreen(toolPoints_[i - 1]), toScreen(toolPoints_[i]));
            }
            painter.drawLine(last, cur);
            // Un contour se ferme toujours : le cote de fermeture est montre.
            if (tool_ == ToolMode::CapturePolygon && toolPoints_.size() >= 2)
                painter.drawLine(cur, toScreen(toolPoints_.front()));
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
    // Loin de l'origine, le pas peut etre plus petit que la precision des
    // coordonnees (x + pas == x) : la boucle ne finirait jamais.
    if (startX + spacing == startX || startY + spacing == startY ||
        (region.maxX - startX) / spacing > 4000.0 || (region.maxY - startY) / spacing > 4000.0)
        return;

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
