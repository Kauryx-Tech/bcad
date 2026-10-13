// Les outils de modification geometrique : decalage et chanfrein (lignes
// et arcs). L'etirement vit avec les transformations (ViewportTransformTools),
// le reseau et les hachures avec la repetition (ViewportPatternTools).
// Meme contrat : selection prealable, points dans `toolPoints_`, une macro
// d'annulation par operation, aucun dialogue.

#include "Viewport.h"

#include "Commands.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include <QUndoStack>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace bcad::app {

using geom::Point2;

namespace {

// Intersection des droites infinies (a,b) et (c,d). nullopt si paralleles.
std::optional<Point2> lineIntersection(const Point2& a, const Point2& b,
                                       const Point2& c, const Point2& d) {
    double dx1 = b.x_ - a.x_, dy1 = b.y_ - a.y_;
    double dx2 = d.x_ - c.x_, dy2 = d.y_ - c.y_;
    double denom = dx1 * dy2 - dy1 * dx2;
    if (std::abs(denom) < geom::Tolerance::kDegenerateLength) return std::nullopt;
    double t = ((c.x_ - a.x_) * dy2 - (c.y_ - a.y_) * dx2) / denom;
    return Point2(a.x_ + t * dx1, a.y_ + t * dy1);
}

} // namespace

void Viewport::applyOffset(const Point2& world) {
    // Deux points definissent la distance de decalage (p2-p1) ; le cote vient
    // de la position de p2 par rapport a chaque entite. Chaque entite
    // selectionnee est clonee puis decalee : lignes (le long de la normale),
    // cercles/arcs (rayon augmente ou reduit selon que p2 est hors ou dans),
    // polylignes (sommets pousses le long des normales moyennes, jointure
    // biseautée).
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 2) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets à décaler."));
        endCommand();
        return;
    }
    double d = geom::distance(toolPoints_[0], toolPoints_[1]);
    if (d < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Distance de décalage trop petite."));
        endCommand();
        return;
    }
    const Point2& side = toolPoints_[1];
    bool done = false;
    if (undoStack_) undoStack_->beginMacro(tr("Décaler"));
    for (geom::Entity* e : selected) {
        std::unique_ptr<geom::Entity> out;
        if (e->typeId() == geom::TypeId_Line) {
            const auto& l = static_cast<const geom::LineEntity&>(*e);
            double dx = l.end().x_ - l.start().x_, dy = l.end().y_ - l.start().y_;
            double len = std::hypot(dx, dy);
            if (len < geom::Tolerance::kDegenerateLength) continue;
            double nx = -dy / len, ny = dx / len;
            double s = (side.x_ - l.start().x_) * nx + (side.y_ - l.start().y_) * ny;
            double sign = (s >= 0) ? 1.0 : -1.0;
            out = std::make_unique<geom::LineEntity>(
                Point2(l.start().x_ + nx * sign * d, l.start().y_ + ny * sign * d),
                Point2(l.end().x_ + nx * sign * d, l.end().y_ + ny * sign * d));
        } else if (e->typeId() == geom::TypeId_Circle || e->typeId() == geom::TypeId_Arc) {
            Point2 c = (e->typeId() == geom::TypeId_Circle)
                           ? static_cast<const geom::CircleEntity&>(*e).center()
                           : static_cast<const geom::ArcEntity&>(*e).center();
            double r = (e->typeId() == geom::TypeId_Circle)
                           ? static_cast<const geom::CircleEntity&>(*e).radius()
                           : static_cast<const geom::ArcEntity&>(*e).radius();
            double r2 = (geom::distance(c, side) >= r) ? r + d : r - d;
            if (r2 < geom::Tolerance::kDegenerateLength) continue;
            if (e->typeId() == geom::TypeId_Circle) {
                out = std::make_unique<geom::CircleEntity>(c, r2);
            } else {
                const auto& a = static_cast<const geom::ArcEntity&>(*e);
                out = std::make_unique<geom::ArcEntity>(c, r2, a.startAngle(), a.endAngle());
            }
        } else if (e->typeId() == geom::TypeId_Polyline) {
            const auto& p = static_cast<const geom::PolylineEntity&>(*e);
            const auto& v = p.vertices();
            if (v.size() < 2) continue;
            double dx0 = v[1].x_ - v[0].x_, dy0 = v[1].y_ - v[0].y_;
            double l0 = std::hypot(dx0, dy0);
            if (l0 < geom::Tolerance::kDegenerateLength) continue;
            double s = (side.x_ - v[0].x_) * (-dy0 / l0) + (side.y_ - v[0].y_) * (dx0 / l0);
            double sign = (s >= 0) ? 1.0 : -1.0;
            std::vector<Point2> nv;
            nv.reserve(v.size());
            std::size_t n = v.size();
            for (std::size_t i = 0; i < n; ++i) {
                double nx = 0.0, ny = 0.0;
                auto addEdge = [&](std::size_t a, std::size_t b) {
                    double ex = v[b].x_ - v[a].x_, ey = v[b].y_ - v[a].y_;
                    double el = std::hypot(ex, ey);
                    if (el < geom::Tolerance::kDegenerateLength) return;
                    nx += -ey / el;
                    ny += ex / el;
                };
                if (i > 0) addEdge(i - 1, i);
                if (i + 1 < n) addEdge(i, i + 1);
                if (p.closed() && (i == 0 || i + 1 == n)) addEdge(n - 1, 0);
                double nl = std::hypot(nx, ny);
                if (nl < geom::Tolerance::kDegenerateLength) {
                    nv.push_back(v[i]);
                } else {
                    nv.emplace_back(v[i].x_ + nx / nl * sign * d,
                                    v[i].y_ + ny / nl * sign * d);
                }
            }
            out = std::make_unique<geom::PolylineEntity>(nv, p.closed());
        } else {
            continue;
        }
        out->setLayer(e->layer());
        out->selected = false;
        if (undoStack_) undoStack_->push(new AddEntityCommand(doc_, std::move(out), tr("Décaler")));
        else doc_->addEntity(std::move(out));
        done = true;
    }
    if (undoStack_) undoStack_->endMacro();
    if (!done) emit statusMessage(tr("Décaler : type d'objet non pris en charge."));
    endCommand();
}

void Viewport::applyChamfer(const Point2& world) {
    // Deux points designent les distances de chanfrein : d1 = distance du
    // premier point a l'intersection des supports des deux premieres
    // entites selectionnees (lignes ou arcs), d2 de meme pour le second.
    // Les entites sont raccourcies et un segment de chanfrein les relie —
    // comportement de CHAMFER sans dialogue. Les cercles complets sont
    // refuses : on ne raccourcit pas un contour ferme.
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 2) return;
    std::vector<geom::Entity*> prims;
    for (geom::Entity* e : selectedEntities()) {
        if (e->typeId() == geom::TypeId_Line || e->typeId() == geom::TypeId_Arc) {
            prims.push_back(e);
        }
    }
    if (prims.size() < 2) {
        emit statusMessage(tr("Chanfrein : sélectionnez d'abord deux lignes ou arcs."));
        endCommand();
        return;
    }
    geom::Entity* e1 = prims[0];
    geom::Entity* e2 = prims[1];
    bool isLine1 = e1->typeId() == geom::TypeId_Line;
    bool isLine2 = e2->typeId() == geom::TypeId_Line;
    // Projection de chaque point designe sur son support.
    auto project = [](geom::Entity* e, const Point2& p) {
        if (e->typeId() == geom::TypeId_Line) {
            const auto& l = static_cast<const geom::LineEntity&>(*e);
            return geom::closestPointOnSegment(p, l.start(), l.end());
        }
        const auto& a = static_cast<const geom::ArcEntity&>(*e);
        double ang = geom::angleOf(a.center(), p);
        return Point2(a.center().x_ + a.radius() * std::cos(ang),
                      a.center().y_ + a.radius() * std::sin(ang));
    };
    Point2 q1 = project(e1, toolPoints_[0]);
    Point2 q2 = project(e2, toolPoints_[1]);
    // Intersections des supports, en retenant la plus proche des points.
    std::vector<Point2> candidates;
    if (isLine1 && isLine2) {
        const auto& l1 = static_cast<const geom::LineEntity&>(*e1);
        const auto& l2 = static_cast<const geom::LineEntity&>(*e2);
        if (auto i = lineIntersection(l1.start(), l1.end(), l2.start(), l2.end())) {
            candidates.push_back(*i);
        }
    } else {
        auto circleOf = [](geom::Entity* e) {
            const auto& a = static_cast<const geom::ArcEntity&>(*e);
            return std::make_pair(a.center(), a.radius());
        };
        auto lineOf = [](geom::Entity* e) {
            const auto& l = static_cast<const geom::LineEntity&>(*e);
            return std::make_pair(l.start(), l.end());
        };
        if (isLine1 != isLine2) {
            geom::Entity* le = isLine1 ? e1 : e2;
            geom::Entity* ae = isLine1 ? e2 : e1;
            auto [s, t] = lineOf(le);
            auto [c, r] = circleOf(ae);
            double dx = t.x_ - s.x_, dy = t.y_ - s.y_;
            double len = std::hypot(dx, dy);
            if (len >= geom::Tolerance::kDegenerateLength) {
                double ux = dx / len, uy = dy / len;
                double fx = s.x_ - c.x_, fy = s.y_ - c.y_;
                double b = 2.0 * (fx * ux + fy * uy);
                double cc = fx * fx + fy * fy - r * r;
                double disc = b * b - 4.0 * cc;
                if (disc >= 0.0) {
                    double sq = std::sqrt(std::max(0.0, disc));
                    for (double sgn : {-1.0, 1.0}) {
                        double tt = (-b + sgn * sq) / 2.0;
                        candidates.emplace_back(s.x_ + ux * tt, s.y_ + uy * tt);
                    }
                }
            }
        } else {
            auto [c1, r1] = circleOf(e1);
            auto [c2, r2] = circleOf(e2);
            double dd = geom::distance(c1, c2);
            if (dd >= geom::Tolerance::kDegenerateLength &&
                dd <= r1 + r2 + geom::Tolerance::kDegenerateLength &&
                dd + std::min(r1, r2) >= std::max(r1, r2) - geom::Tolerance::kDegenerateLength) {
                double a = (r1 * r1 - r2 * r2 + dd * dd) / (2.0 * dd);
                double h2 = r1 * r1 - a * a;
                double h = std::sqrt(std::max(0.0, h2));
                double xm = c1.x_ + a * (c2.x_ - c1.x_) / dd;
                double ym = c1.y_ + a * (c2.y_ - c1.y_) / dd;
                double rx = -(c2.y_ - c1.y_) * (h / dd);
                double ry = (c2.x_ - c1.x_) * (h / dd);
                candidates.emplace_back(xm + rx, ym + ry);
                candidates.emplace_back(xm - rx, ym - ry);
            }
        }
    }
    if (candidates.empty()) {
        emit statusMessage(tr("Chanfrein : supports sans intersection."));
        endCommand();
        return;
    }
    Point2 mid((q1.x_ + q2.x_) / 2.0, (q1.y_ + q2.y_) / 2.0);
    Point2 I = *std::min_element(candidates.begin(), candidates.end(),
                                 [&](const Point2& a, const Point2& b) {
                                     return geom::distance(a, mid) < geom::distance(b, mid);
                                 });
    double d1 = geom::distance(q1, I), d2 = geom::distance(q2, I);
    if (d1 < geom::Tolerance::kDegenerateLength || d2 < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Chanfrein : distances trop petites."));
        endCommand();
        return;
    }
    // Point de raccord a distance d de I, du cote du point designe.
    auto trimPoint = [&](geom::Entity* e, const Point2& q, double d) {
        if (e->typeId() == geom::TypeId_Line) {
            double v1x = q.x_ - I.x_, v1y = q.y_ - I.y_;
            double vl = std::hypot(v1x, v1y);
            if (vl < geom::Tolerance::kDegenerateLength) return I;
            return Point2(I.x_ + v1x / vl * d, I.y_ + v1y / vl * d);
        }
        const auto& a = static_cast<const geom::ArcEntity&>(*e);
        double aI = geom::angleOf(a.center(), I);
        double aq = geom::angleOf(a.center(), q);
        double sweep = geom::normalizeAngle(aq - aI);
        double dir = (sweep <= std::numbers::pi) ? 1.0 : -1.0;
        double aT = aI + dir * (d / a.radius());
        return Point2(a.center().x_ + a.radius() * std::cos(aT),
                      a.center().y_ + a.radius() * std::sin(aT));
    };
    Point2 t1 = trimPoint(e1, q1, d1);
    Point2 t2 = trimPoint(e2, q2, d2);
    // Entite raccourcie : ligne (extremite la plus proche remplacee) ou arc
    // (angle de l'extremite la plus proche recale sur le point de raccord).
    auto shorten = [&](geom::Entity* e, const Point2& t) -> std::unique_ptr<geom::Entity> {
        if (e->typeId() == geom::TypeId_Line) {
            const auto& l = static_cast<const geom::LineEntity&>(*e);
            bool nearStart = geom::distance(l.start(), t) < geom::distance(l.end(), t);
            return std::make_unique<geom::LineEntity>(nearStart ? t : l.start(),
                                                     nearStart ? l.end() : t);
        }
        auto out = e->clone();
        auto& a = static_cast<geom::ArcEntity&>(*out);
        double angT = geom::angleOf(a.center(), t);
        if (geom::distance(a.startPoint(), t) < geom::distance(a.endPoint(), t)) {
            a.setStartAngle(angT);
        } else {
            a.setEndAngle(angT);
        }
        return out;
    };
    std::unique_ptr<geom::Entity> n1 = shorten(e1, t1);
    std::unique_ptr<geom::Entity> n2 = shorten(e2, t2);
    n1->setLayer(e1->layer());
    n2->setLayer(e2->layer());
    n1->selected = n2->selected = false;
    auto chamfer = std::make_unique<geom::LineEntity>(t1, t2);
    chamfer->setLayer(e1->layer());
    if (undoStack_) {
        undoStack_->beginMacro(tr("Chanfrein"));
        undoStack_->push(new RemoveEntityCommand(doc_, e1, tr("Chanfrein")));
        undoStack_->push(new RemoveEntityCommand(doc_, e2, tr("Chanfrein")));
        undoStack_->push(new AddEntityCommand(doc_, std::move(n1), tr("Chanfrein")));
        undoStack_->push(new AddEntityCommand(doc_, std::move(n2), tr("Chanfrein")));
        undoStack_->push(new AddEntityCommand(doc_, std::move(chamfer), tr("Chanfrein")));
        undoStack_->endMacro();
    } else {
        doc_->removeEntity(e1->id());
        doc_->removeEntity(e2->id());
        doc_->addEntity(std::move(n1));
        doc_->addEntity(std::move(n2));
        doc_->addEntity(std::move(chamfer));
    }
    endCommand();
}

} // namespace bcad::app
