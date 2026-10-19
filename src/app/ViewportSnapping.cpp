// Accrochage du curseur, dans l'ordre d'AutoCAD : objets, reperage d'objet
// (F11), Ortho (F8) ou reperage polaire (F10), grille (F9) ; et le dessin des
// reperages (croix des points acquis, lignes d'alignement, rayon polaire).
// Calculs purs : TrackingDetail.h.

#include "Viewport.h"

#include "TrackingDetail.h"
#include "ViewportTolerances.h"
#include "theme/Theme.h"

#include "bcad/render/Grid.h"

#include <QFontMetrics>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace bcad::app {

using geom::Point2;

Point2 Viewport::snappedWorld(QPoint screenPos) {
    Point2 world = toWorld(screenPos);
    activeSnap_ = {};
    polarPoint_.reset();
    trackPoint_.reset();

    if (doc_ && snapEnabled_) {
        double tol = kSnapToleranceScreenPx / camera_.pixelsPerUnit();
        activeSnap_ = snapEngine_.findSnap(*doc_, world, tol, activeReferencePoint());
        if (activeSnap_) {
            // Reperage d'objet : un point d'accrochage survole pendant une
            // commande est acquis (les 7 derniers).
            if (objectTracking_ && tool_ != ToolMode::Select &&
                std::none_of(acquired_.begin(), acquired_.end(), [&](const Point2& p) {
                    return geom::distance(p, activeSnap_.point) < geom::Tolerance::kDegenerateLength;
                })) {
                acquired_.push_back(activeSnap_.point);
                if (acquired_.size() > 7) acquired_.erase(acquired_.begin());
            }
            return activeSnap_.point;
        }
    }

    // Reperage d'objet : horizontale / verticale d'un point acquis.
    if (objectTracking_ && tool_ != ToolMode::Select && !acquired_.empty()) {
        if (auto hit = detail::trackAlign(acquired_, world, kSnapToleranceScreenPx / camera_.pixelsPerUnit())) {
            trackPoint_ = hit->point;
            trackHorizontalFrom_ = hit->horizontalFrom;
            trackVerticalFrom_ = hit->verticalFrom;
            return hit->point;
        }
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
    // d'Ortho) : direction multiple de l'increment, pres du curseur.
    if (polarEnabled_ && !orthoEnabled_ && tool_ != ToolMode::Select) {
        if (auto ref = activeReferencePoint()) {
            if (auto hit = detail::polarTrack(*ref, world, polarIncrementDeg_, kSnapToleranceScreenPx / camera_.pixelsPerUnit())) {
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

namespace {
// Bulle de repere pres du curseur (« Polaire : 12.000 < 90° »), comme AutoCAD.
void drawTrackingTip(QPainter& painter, const QPointF& at, const QString& label) {
    const QFontMetrics fm(painter.font());
    const QRectF box(at + QPointF(16, -fm.height() - 18),
                     QSizeF(fm.horizontalAdvance(label) + 2 * theme::kSpaceS, fm.height() + theme::kSpaceS));
    painter.setPen(QPen(theme::color(theme::Token::Border), 1));
    painter.setBrush(theme::color(theme::Token::Panel));
    painter.drawRoundedRect(box, theme::kRadius, theme::kRadius);
    painter.setPen(theme::color(theme::Token::TextPrimary));
    painter.drawText(box, Qt::AlignCenter, label);
}

// Ligne de repere : tirets bien visibles (un pointille de 1 px se perd sur
// le fond du canevas).
QPen trackingPen() {
    QPen pen(theme::color(theme::Token::Snap), 1);
    pen.setDashPattern({6, 4});
    return pen;
}
} // namespace

void Viewport::drawTracking(QPainter& painter) {
    if (tool_ == ToolMode::Select) return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // Reperage d'objet : croix sur les points acquis (taille du marqueur
    // d'accrochage), lignes d'alignement et bulle « Repérage : d < angle ».
    constexpr double kHalf = 6.0;
    painter.setPen(QPen(theme::color(theme::Token::Snap), 2));
    for (const geom::Point2& p : acquired_) {
        const QPointF s = toScreenF(p);
        painter.drawLine(s - QPointF(kHalf, 0), s + QPointF(kHalf, 0));
        painter.drawLine(s - QPointF(0, kHalf), s + QPointF(0, kHalf));
    }
    if (trackPoint_) {
        painter.setPen(trackingPen());
        const QPointF at = toScreenF(*trackPoint_);
        if (trackHorizontalFrom_) painter.drawLine(QPointF(0, at.y()), QPointF(width(), at.y()));
        if (trackVerticalFrom_) painter.drawLine(QPointF(at.x(), 0), QPointF(at.x(), height()));
        QString label;
        if (trackHorizontalFrom_ && trackVerticalFrom_) {
            label = tr("Repérage : intersection");
        } else if (trackHorizontalFrom_) {
            const double d = trackPoint_->x_ - trackHorizontalFrom_->x_;
            label = tr("Repérage : %1 < %2°").arg(std::abs(d), 0, 'f', 3).arg(d >= 0 ? 0 : 180);
        } else {
            const double d = trackPoint_->y_ - trackVerticalFrom_->y_;
            label = tr("Repérage : %1 < %2°").arg(std::abs(d), 0, 'f', 3).arg(d >= 0 ? 90 : 270);
        }
        drawTrackingTip(painter, at, label);
    }
    // Reperage polaire : rayon de la direction reperee, jusqu'au bord du
    // canevas, et bulle « Polaire : longueur < angle ».
    if (polarPoint_ && polarRef_) {
        const QPointF from = toScreenF(*polarRef_);
        const QPointF to = toScreenF(*polarPoint_);
        const QPointF dir = to - from;
        if (const double len = std::hypot(dir.x(), dir.y()); len >= 1.0) {
            painter.setPen(trackingPen());
            painter.drawLine(from, from + dir / len * std::hypot(width(), height()));
            drawTrackingTip(painter, to,
                            tr("Polaire : %1 < %2°").arg(polarDistance_, 0, 'f', 3).arg(polarAngleDeg_, 0, 'f', 0));
        }
    }
    painter.restore();
}

} // namespace bcad::app
