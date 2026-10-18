// Apercus des outils de modification et de repetition : fantomes de ce
// que l'outil va poser (decalage, chanfrein, reseau), fenetre d'etirement,
// point chaud (hachures, effacer). Lecture seule : rien n'est commis, les
// calculs sont ceux de ModifyDetail.h, partages avec l'execution.

#include "Viewport.h"

#include "ModifyDetail.h"
#include "bcad/geometry/GeometryUtils.h"
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace bcad::app {

using geom::Point2;

bool Viewport::drawModifyPreview(QPainter& painter) {
    auto toScreen = [this](const Point2& p) {
        render::ScreenPoint s = camera_.worldToScreen(p);
        return QPointF(s.x, s.y);
    };

    // Apercu sans points : point chaud (hachures : carre au pas courant ;
    // effacer : croix).
    if (toolPoints_.empty() && hoverWorld_) {
        QPointF cur = toScreen(*hoverWorld_);
        if (tool_ == ToolMode::Hatch) {
            double h = ((hatchSpacing_ > 0.0) ? hatchSpacing_ : 5.0) * camera_.pixelsPerUnit() / 2.0;
            painter.drawRect(QRectF(cur - QPointF(h, h), cur + QPointF(h, h)));
            return true;
        }
        if (tool_ == ToolMode::Erase) {
            double k = 6.0;
            painter.drawLine(cur - QPointF(k, k), cur + QPointF(k, k));
            painter.drawLine(cur - QPointF(-k, k), cur + QPointF(-k, k));
            return true;
        }
    }

    if (tool_ == ToolMode::Fillet) return drawFilletPreview(painter);
    if (tool_ == ToolMode::Offset) return drawOffsetPreview(painter);

    if (!toolPoints_.empty() && hoverWorld_) {
        QPointF last = toScreen(toolPoints_.back());
        QPointF cur = toScreen(*hoverWorld_);

        auto drawCross = [&](const Point2& p) {
            QPointF sp = toScreen(p);
            double k = 5.0;
            painter.drawLine(sp - QPointF(k, 0), sp + QPointF(k, 0));
            painter.drawLine(sp - QPointF(0, k), sp + QPointF(0, k));
        };
        auto drawGhost = [&](const geom::Entity* ghost) {
            QPolygonF path;
            for (const auto& p : ghost->tessellate(0.5)) path << toScreen(p);
            if (path.size() == 2) painter.drawLine(path.at(0), path.at(1));
            else if (!path.isEmpty()) painter.drawPolyline(path);
        };

        if (tool_ == ToolMode::Stretch) {
            // Fenetre, puis vecteur de base et objets etires sous le curseur.
            if (toolPoints_.size() == 1) {
                painter.drawRect(QRectF(last, cur));
                return true;
            }
            painter.drawRect(QRectF(toScreen(toolPoints_[0]), toScreen(toolPoints_[1])));
            if (toolPoints_.size() >= 3) {
                painter.drawLine(last, cur);
                const double dx = hoverWorld_->x_ - toolPoints_[2].x_;
                const double dy = hoverWorld_->y_ - toolPoints_[2].y_;
                int shown = 0;
                for (geom::Entity* e : stretchTargets()) {
                    if (shown >= 50) break;
                    if (auto ghost = detail::stretchClone(e, toolPoints_[0], toolPoints_[1], dx, dy)) {
                        drawGhost(ghost.get());
                        ++shown;
                    }
                }
            }
            return true;
        }
        if (tool_ == ToolMode::Chamfer && !toolPoints_.empty()) {
            // Points de raccord et segment de chanfrein qui seront poses.
            std::vector<geom::Entity*> prims;
            for (geom::Entity* e : selectedEntities()) {
                if (e->typeId() == geom::TypeId_Line || e->typeId() == geom::TypeId_Arc ||
                    e->typeId() == geom::TypeId_Polyline) {
                    prims.push_back(e);
                }
            }
            if (prims.size() >= 2) {
                detail::ChamferSupport s1 = detail::chamferSupport(prims[0], toolPoints_[0]);
                detail::ChamferSupport s2 = detail::chamferSupport(prims[1], *hoverWorld_);
                std::vector<Point2> cands = detail::supportIntersections(s1, s2);
                if (!cands.empty()) {
                    Point2 q1 = detail::projectOnSupport(s1, toolPoints_[0]);
                    Point2 q2 = detail::projectOnSupport(s2, *hoverWorld_);
                    Point2 mid((q1.x_ + q2.x_) / 2.0, (q1.y_ + q2.y_) / 2.0);
                    Point2 inter = *std::min_element(
                        cands.begin(), cands.end(), [&](const Point2& a, const Point2& b) {
                            return geom::distance(a, mid) < geom::distance(b, mid);
                        });
                    double d1 = geom::distance(q1, inter), d2 = geom::distance(q2, inter);
                    if (!chamferFillet_ && chamferDist_ > 0.0) d1 = d2 = chamferDist_;
                    if (chamferFillet_ && filletRadius_ > 0.0) {
                        // Arc de conge qui sera pose.
                        if (auto fillet = detail::filletBetween(s1, s2, inter, q1, q2,
                                                               filletRadius_)) {
                            drawCross(fillet->t1);
                            drawCross(fillet->t2);
                            geom::ArcEntity arc(fillet->c, filletRadius_,
                                                fillet->ccw ? fillet->a1 : fillet->a2,
                                                fillet->ccw ? fillet->a2 : fillet->a1);
                            drawGhost(&arc);
                            return true;
                        }
                    } else if (!chamferFillet_ && d1 >= geom::Tolerance::kDegenerateLength &&
                               d2 >= geom::Tolerance::kDegenerateLength) {
                        Point2 t1 = detail::trimOnSupport(s1, inter, q1, d1);
                        Point2 t2 = detail::trimOnSupport(s2, inter, q2, d2);
                        drawCross(t1);
                        drawCross(t2);
                        painter.drawLine(toScreen(t1), toScreen(t2));
                        return true;
                    }
                }
            }
            painter.drawLine(last, cur);
            return true;
        }
        if (tool_ == ToolMode::Array) {
            // Fantomes des clones qui seront poses (3 premiers objets, 24
            // positions max), puis reperes de pas.
            if (toolPoints_.size() >= 2) {
                Point2 p2 = toolPoints_.size() >= 3 ? toolPoints_[2] : *hoverWorld_;
                std::vector<geom::Transform2D> trs = detail::arrayTransforms(
                    arrayPolar_, toolPoints_[0], toolPoints_[1], p2, arrayCount_);
                int shown = 0;
                for (geom::Entity* e : selectedEntities()) {
                    if (shown++ >= 3) break;
                    int n = 0;
                    for (const auto& t : trs) {
                        if (n++ >= 24) break;
                        auto ghost = e->clone();
                        ghost->applyTransform(t);
                        drawGhost(ghost.get());
                    }
                }
            }
            if (arrayPolar_) {
                // Arc du pas angulaire + positions des objets.
                const Point2& center = toolPoints_[0];
                double rad = toolPoints_.size() >= 2 ? geom::distance(center, toolPoints_[1])
                                                     : geom::distance(center, *hoverWorld_);
                if (rad >= geom::Tolerance::kDegenerateLength) {
                    double a0 = geom::angleOf(center, toolPoints_.size() >= 2 ? toolPoints_[1]
                                                                             : *hoverWorld_);
                    double a1 = geom::angleOf(center, *hoverWorld_);
                    double sweep = toolPoints_.size() >= 2 ? geom::normalizeAngle(a1 - a0)
                                                           : 2.0 * std::numbers::pi;
                    QPolygonF arc;
                    for (int i = 0; i <= 32; ++i) {
                        double a = a0 + sweep * i / 32.0;
                        arc << toScreen(Point2(center.x_ + rad * std::cos(a),
                                               center.y_ + rad * std::sin(a)));
                    }
                    painter.drawPolyline(arc);
                    if (toolPoints_.size() >= 2 && arrayCount_ > 1) {
                        double st = geom::normalizeAngle(a1 - a0);
                        for (int i = 1; i < arrayCount_; ++i) {
                            double a = a0 + st * i;
                            drawCross(Point2(center.x_ + rad * std::cos(a),
                                             center.y_ + rad * std::sin(a)));
                        }
                    }
                }
            } else if (toolPoints_.size() == 1) {
                painter.drawLine(last, cur);
                for (int i = 1; i < 3; ++i) {
                    double f = i / 3.0;
                    drawCross(Point2(toolPoints_[0].x_ + (hoverWorld_->x_ - toolPoints_[0].x_) * f,
                                     toolPoints_[0].y_ + (hoverWorld_->y_ - toolPoints_[0].y_) * f));
                }
            } else {
                // Noeuds de la grille qui sera posee.
                double sx = toolPoints_[1].x_ - toolPoints_[0].x_;
                double sy = toolPoints_[1].y_ - toolPoints_[0].y_;
                double rx = hoverWorld_->x_ - toolPoints_[1].x_;
                double ry = hoverWorld_->y_ - toolPoints_[1].y_;
                int cols = std::max(2, arrayCount_);
                for (int j = 0; j < 2; ++j)
                    for (int i = 0; i < cols; ++i)
                        drawCross(Point2(toolPoints_[0].x_ + i * sx + j * rx,
                                         toolPoints_[0].y_ + i * sy + j * ry));
            }
            return true;
        }
    }

    return false;
}

} // namespace bcad::app
