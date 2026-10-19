// Poignees (M-02, les « grips » d'AutoCAD) : ou elles se trouvent sur un objet
// et ce que devient l'objet quand on en tire une. Pur, partage entre
// l'execution, l'apercu sous le curseur et les tests.
//
// Comme AutoCAD :
//   - ligne : extremites (etirent), milieu (deplace la ligne) ;
//   - polyligne : sommets (etirent), milieux des segments (deplacent le
//     segment, ses deux sommets ensemble) ; trous compris ;
//   - cercle : centre (deplace), quatre quadrants (changent le rayon) ;
//   - arc : extremites et milieu (l'arc passe par le nouveau point et garde
//     les deux autres), centre (deplace) ;
//   - point, texte : position (deplace) ;
//   - cotations : points de definition et position de la ligne de cote ;
//   - tout autre objet : une poignee au centre de son emprise (deplace).

#pragma once

#include "detail/CircleDetail.h"

#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/RadialDimensionEntity.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/Transform2D.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace bcad::app::detail {

using geom::Point2;

enum class GripRole {
    Vertex,     // sommet ou extremite : seul ce point bouge
    Segment,    // milieu d'un segment de polyligne : ses deux sommets bougent
    Move,       // milieu d'une ligne, centre, position : tout l'objet bouge
    Quadrant,   // quadrant d'un cercle : le rayon change
    ArcMid,     // milieu d'un arc : l'arc passe par le nouveau point
};

struct Grip {
    Point2 at;
    GripRole role = GripRole::Move;
    int ring = 0;    // polyligne : 0 contour, 1.. trous
    int index = 0;   // sommet, segment, quadrant, ou point de definition
};

namespace grip_impl {

inline Point2 mid(const Point2& a, const Point2& b) {
    return Point2((a.x_ + b.x_) / 2.0, (a.y_ + b.y_) / 2.0);
}

inline Point2 arcMidPoint(const geom::ArcEntity& a) {
    const double t = a.startAngle() + a.sweep() / 2.0;
    return Point2(a.center().x_ + a.radius() * std::cos(t), a.center().y_ + a.radius() * std::sin(t));
}

inline void addRing(std::vector<Grip>& out, const std::vector<Point2>& ring, bool closed, int r) {
    const int n = static_cast<int>(ring.size());
    for (int i = 0; i < n; ++i) out.push_back({ring[i], GripRole::Vertex, r, i});
    const int segments = closed ? n : n - 1;
    for (int i = 0; i < segments; ++i) out.push_back({mid(ring[i], ring[(i + 1) % n]), GripRole::Segment, r, i});
}

// Points de definition d'une cotation, dans l'ordre de ses setters.
inline std::vector<Point2> dimensionPoints(const geom::Entity& e) {
    if (auto* d = dynamic_cast<const geom::LinearDimensionEntity*>(&e)) return {d->defPt1(), d->defPt2(), d->dimLineLoc()};
    if (auto* d = dynamic_cast<const geom::AlignedDimensionEntity*>(&e)) return {d->defPt1(), d->defPt2(), d->dimLineLoc()};
    if (auto* d = dynamic_cast<const geom::AngularDimensionEntity*>(&e)) return {d->vertex(), d->start(), d->end(), d->dimLineLoc()};
    if (auto* d = dynamic_cast<const geom::RadialDimensionEntity*>(&e)) return {d->center(), d->chordPoint(), d->dimLineLoc()};
    return {};
}

inline void setDimensionPoint(geom::Entity& e, int i, const Point2& p) {
    if (auto* d = dynamic_cast<geom::LinearDimensionEntity*>(&e)) {
        if (i == 0) d->setDefPt1(p); else if (i == 1) d->setDefPt2(p); else d->setDimLineLoc(p);
    } else if (auto* d = dynamic_cast<geom::AlignedDimensionEntity*>(&e)) {
        if (i == 0) d->setDefPt1(p); else if (i == 1) d->setDefPt2(p); else d->setDimLineLoc(p);
    } else if (auto* d = dynamic_cast<geom::AngularDimensionEntity*>(&e)) {
        if (i == 0) d->setVertex(p); else if (i == 1) d->setStart(p); else if (i == 2) d->setEnd(p); else d->setDimLineLoc(p);
    } else if (auto* d = dynamic_cast<geom::RadialDimensionEntity*>(&e)) {
        if (i == 0) d->setCenter(p); else if (i == 1) d->setChordPoint(p); else d->setDimLineLoc(p);
    }
}

} // namespace grip_impl

inline std::vector<Grip> gripsOf(const geom::Entity& e) {
    using namespace grip_impl;
    std::vector<Grip> out;
    if (e.typeId() == geom::TypeId_Line) {
        const auto& l = static_cast<const geom::LineEntity&>(e);
        out = {{l.start(), GripRole::Vertex, 0, 0}, {l.end(), GripRole::Vertex, 0, 1},
               {mid(l.start(), l.end()), GripRole::Move, 0, 0}};
    } else if (auto* p = dynamic_cast<const geom::PolylineEntity*>(&e)) {
        addRing(out, p->vertices(), p->closed(), 0);
        for (std::size_t h = 0; h < p->holes().size(); ++h) addRing(out, p->holes()[h], true, static_cast<int>(h) + 1);
    } else if (e.typeId() == geom::TypeId_Circle) {
        const auto& c = static_cast<const geom::CircleEntity&>(e);
        out.push_back({c.center(), GripRole::Move, 0, 0});
        for (int q = 0; q < 4; ++q) {
            const double t = q * std::numbers::pi / 2.0;
            out.push_back({Point2(c.center().x_ + c.radius() * std::cos(t), c.center().y_ + c.radius() * std::sin(t)),
                           GripRole::Quadrant, 0, q});
        }
    } else if (e.typeId() == geom::TypeId_Arc) {
        const auto& a = static_cast<const geom::ArcEntity&>(e);
        out = {{a.startPoint(), GripRole::Vertex, 0, 0}, {a.endPoint(), GripRole::Vertex, 0, 1},
               {arcMidPoint(a), GripRole::ArcMid, 0, 0}, {a.center(), GripRole::Move, 0, 0}};
    } else if (e.typeId() == geom::TypeId_Point) {
        out.push_back({static_cast<const geom::PointEntity&>(e).position(), GripRole::Move, 0, 0});
    } else if (e.typeId() == geom::TypeId_Text) {
        out.push_back({static_cast<const geom::TextEntity&>(e).position(), GripRole::Move, 0, 0});
    } else if (const auto pts = dimensionPoints(e); !pts.empty()) {
        for (int i = 0; i < static_cast<int>(pts.size()); ++i) out.push_back({pts[i], GripRole::Vertex, 0, i});
    } else {
        const geom::BoundingBox bb = e.boundingBox();
        if (bb.isValid()) out.push_back({Point2((bb.minX + bb.maxX) / 2.0, (bb.minY + bb.maxY) / 2.0), GripRole::Move, 0, 0});
    }
    return out;
}

// L'objet apres avoir tire la poignee `g` jusqu'a `to` (copie, l'original
// intact). Rien si le resultat serait degenere (rayon nul, arc aligne).
inline std::unique_ptr<geom::Entity> moveGrip(const geom::Entity& e, const Grip& g, const Point2& to) {
    using namespace grip_impl;
    const double dx = to.x_ - g.at.x_, dy = to.y_ - g.at.y_;
    std::unique_ptr<geom::Entity> out = e.clone();
    if (g.role == GripRole::Move) {
        out->applyTransform(geom::Transform2D::translation(dx, dy));
        return out;
    }
    if (out->typeId() == geom::TypeId_Line) {
        auto& l = static_cast<geom::LineEntity&>(*out);
        if (g.index == 0) l.setStart(to); else l.setEnd(to);
        return out;
    }
    if (auto* p = dynamic_cast<geom::PolylineEntity*>(out.get())) {
        std::vector<Point2>& ring = g.ring == 0 ? p->vertices() : p->holes()[g.ring - 1];
        const int n = static_cast<int>(ring.size());
        if (g.index < 0 || g.index >= n) return nullptr;
        if (g.role == GripRole::Vertex) {
            ring[g.index] = to;
        } else {
            for (int i : {g.index, (g.index + 1) % n}) {
                ring[i].x_ += dx;
                ring[i].y_ += dy;
            }
        }
        return out;
    }
    if (out->typeId() == geom::TypeId_Circle) {
        auto& c = static_cast<geom::CircleEntity&>(*out);
        const double r = geom::distance(c.center(), to);
        if (r < geom::Tolerance::kDegenerateLength) return nullptr;
        c.setRadius(r);
        return out;
    }
    if (out->typeId() == geom::TypeId_Arc) {
        auto& a = static_cast<geom::ArcEntity&>(*out);
        Point2 s = a.startPoint(), m = arcMidPoint(a), f = a.endPoint();
        if (g.role == GripRole::ArcMid) m = to;
        else if (g.index == 0) s = to;
        else f = to;
        const auto def = arcFrom3P(s, m, f);
        if (!def) return nullptr;
        a.setCenter(def->center);
        a.setRadius(def->radius);
        a.setStartAngle(def->startAngle);
        a.setEndAngle(def->endAngle);
        return out;
    }
    if (!dimensionPoints(*out).empty()) {
        setDimensionPoint(*out, g.index, to);
        return out;
    }
    return nullptr;
}

} // namespace bcad::app::detail
