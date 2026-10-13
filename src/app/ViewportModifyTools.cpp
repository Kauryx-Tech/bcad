// Les outils de modification geometrique : decalage, etirement, chanfrein,
// reseau rectangulaire, hachures. Comme les outils de transformation
// (ViewportTransformTools), ils partent de la selection et accumulent leurs
// points dans `toolPoints_` ; chaque operation passe par une macro
// d'annulation unique. Tous sont entierement pilotables au clic, sans
// dialogue : les parametres se deduisent des points designes.

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

bool inRect(const Point2& p, double x0, double y0, double x1, double y1) {
    double tol = geom::Tolerance::kDegenerateLength;
    return p.x_ >= std::min(x0, x1) - tol && p.x_ <= std::max(x0, x1) + tol &&
           p.y_ >= std::min(y0, y1) - tol && p.y_ <= std::max(y0, y1) + tol;
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

void Viewport::applyStretch(const Point2& world) {
    // Trois points : deux coins de fenetre, puis la cible du deplacement
    // (vecteur = p3-p2). Les sommets des lignes et polylignes selectionnees
    // qui tombent dans la fenetre suivent le vecteur ; les centres de
    // cercles/arcs dans la fenetre deplacent l'objet entier.
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 3) return;
    std::vector<geom::Entity*> selected = selectedEntities();
    if (selected.empty()) {
        emit statusMessage(tr("Sélectionnez d'abord les objets à étirer."));
        endCommand();
        return;
    }
    const Point2& w1 = toolPoints_[0];
    const Point2& w2 = toolPoints_[1];
    double dx = toolPoints_[2].x_ - w2.x_, dy = toolPoints_[2].y_ - w2.y_;
    if (std::hypot(dx, dy) < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Vecteur d'étirement trop petit."));
        endCommand();
        return;
    }
    bool done = false;
    if (undoStack_) undoStack_->beginMacro(tr("Étirer"));
    for (geom::Entity* e : selected) {
        std::unique_ptr<geom::Entity> out;
        if (e->typeId() == geom::TypeId_Line) {
            const auto& l = static_cast<const geom::LineEntity&>(*e);
            Point2 a = l.start(), b = l.end();
            bool ma = inRect(a, w1.x_, w1.y_, w2.x_, w2.y_);
            bool mb = inRect(b, w1.x_, w1.y_, w2.x_, w2.y_);
            if (!ma && !mb) continue;
            if (ma) { a.x_ += dx; a.y_ += dy; }
            if (mb) { b.x_ += dx; b.y_ += dy; }
            out = std::make_unique<geom::LineEntity>(a, b);
        } else if (e->typeId() == geom::TypeId_Polyline) {
            const auto& p = static_cast<const geom::PolylineEntity&>(*e);
            std::vector<Point2> nv = p.vertices();
            bool any = false;
            for (auto& v : nv) {
                if (inRect(v, w1.x_, w1.y_, w2.x_, w2.y_)) {
                    v.x_ += dx;
                    v.y_ += dy;
                    any = true;
                }
            }
            if (!any) continue;
            out = std::make_unique<geom::PolylineEntity>(nv, p.closed());
        } else if (e->typeId() == geom::TypeId_Circle || e->typeId() == geom::TypeId_Arc) {
            Point2 c = (e->typeId() == geom::TypeId_Circle)
                           ? static_cast<const geom::CircleEntity&>(*e).center()
                           : static_cast<const geom::ArcEntity&>(*e).center();
            if (!inRect(c, w1.x_, w1.y_, w2.x_, w2.y_)) continue;
            out = e->clone();
            out->applyTransform(geom::Transform2D::translation(dx, dy));
        } else {
            continue;
        }
        out->setLayer(e->layer());
        out->selected = true;
        if (undoStack_) {
            undoStack_->push(new RemoveEntityCommand(doc_, e, tr("Étirer")));
            undoStack_->push(new AddEntityCommand(doc_, std::move(out), tr("Étirer")));
        } else {
            doc_->removeEntity(e->id());
            doc_->addEntity(std::move(out));
        }
        done = true;
    }
    if (undoStack_) undoStack_->endMacro();
    if (!done) emit statusMessage(tr("Étirer : aucun sommet dans la fenêtre."));
    endCommand();
}

void Viewport::applyChamfer(const Point2& world) {
    // Deux points designent les distances de chanfrein : d1 = distance du
    // premier point a l'intersection des deux premieres lignes selectionnees,
    // d2 de meme pour le second. Les lignes sont raccourcies et un segment
    // de chanfrein les relie — comportement de CHAMFER sans dialogue.
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 2) return;
    std::vector<geom::LineEntity*> lines;
    for (geom::Entity* e : selectedEntities()) {
        if (e->typeId() == geom::TypeId_Line) lines.push_back(static_cast<geom::LineEntity*>(e));
    }
    if (lines.size() < 2) {
        emit statusMessage(tr("Chanfrein : sélectionnez d'abord deux lignes."));
        endCommand();
        return;
    }
    geom::LineEntity* l1 = lines[0];
    geom::LineEntity* l2 = lines[1];
    auto inter = lineIntersection(l1->start(), l1->end(), l2->start(), l2->end());
    if (!inter) {
        emit statusMessage(tr("Chanfrein : lignes parallèles."));
        endCommand();
        return;
    }
    const Point2& I = *inter;
    Point2 q1 = geom::closestPointOnSegment(toolPoints_[0], l1->start(), l1->end());
    Point2 q2 = geom::closestPointOnSegment(toolPoints_[1], l2->start(), l2->end());
    double d1 = geom::distance(q1, I), d2 = geom::distance(q2, I);
    if (d1 < geom::Tolerance::kDegenerateLength || d2 < geom::Tolerance::kDegenerateLength) {
        emit statusMessage(tr("Chanfrein : distances trop petites."));
        endCommand();
        return;
    }
    auto trimPoint = [&](const geom::LineEntity* l, const Point2& q, double d) {
        double v1x = q.x_ - I.x_, v1y = q.y_ - I.y_;
        double vl = std::hypot(v1x, v1y);
        if (vl < geom::Tolerance::kDegenerateLength) {
            // Point pile sur l'intersection : on recule le long de la ligne.
            v1x = l->start().x_ - I.x_;
            v1y = l->start().y_ - I.y_;
            vl = std::hypot(v1x, v1y);
            if (vl < geom::Tolerance::kDegenerateLength) return I;
        }
        return Point2(I.x_ + v1x / vl * d, I.y_ + v1y / vl * d);
    };
    Point2 t1 = trimPoint(l1, q1, d1);
    Point2 t2 = trimPoint(l2, q2, d2);
    auto shorten = [&](const geom::LineEntity* l, const Point2& t) {
        bool nearStart = geom::distance(l->start(), t) < geom::distance(l->end(), t);
        return nearStart ? geom::LineEntity(t, l->end()) : geom::LineEntity(l->start(), t);
    };
    geom::LineEntity n1 = shorten(l1, t1);
    geom::LineEntity n2 = shorten(l2, t2);
    auto chamfer = std::make_unique<geom::LineEntity>(t1, t2);
    chamfer->setLayer(l1->layer());
    if (undoStack_) {
        undoStack_->beginMacro(tr("Chanfrein"));
        undoStack_->push(new RemoveEntityCommand(doc_, l1, tr("Chanfrein")));
        undoStack_->push(new RemoveEntityCommand(doc_, l2, tr("Chanfrein")));
        auto c1 = std::make_unique<geom::LineEntity>(n1);
        auto c2 = std::make_unique<geom::LineEntity>(n2);
        c1->setLayer(l1->layer());
        c2->setLayer(l2->layer());
        c1->selected = c2->selected = false;
        undoStack_->push(new AddEntityCommand(doc_, std::move(c1), tr("Chanfrein")));
        undoStack_->push(new AddEntityCommand(doc_, std::move(c2), tr("Chanfrein")));
        undoStack_->push(new AddEntityCommand(doc_, std::move(chamfer), tr("Chanfrein")));
        undoStack_->endMacro();
    } else {
        doc_->removeEntity(l1->id());
        doc_->removeEntity(l2->id());
        n1.setLayer(l1->layer());
        n2.setLayer(l2->layer());
        doc_->addEntity(std::make_unique<geom::LineEntity>(n1));
        doc_->addEntity(std::make_unique<geom::LineEntity>(n2));
        doc_->addEntity(std::move(chamfer));
    }
    endCommand();
}

} // namespace bcad::app
