// Poignees (M-02) : emplacement des poignees et objet obtenu en tirant
// chacune, calculs purs de GripDetail.h.

#include "GripDetail.h"

#include <cmath>
#include <cstdio>
#include <numbers>

using namespace bcad;
using app::detail::Grip;
using app::detail::GripRole;
using app::detail::gripsOf;
using app::detail::moveGrip;
using geom::Point2;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAILED: %s\n", what);
        ++failures;
    }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
bool same(const Point2& a, double x, double y) { return near(a.x_, x) && near(a.y_, y); }
} // namespace

int main() {
    // --- Ligne : extremites, milieu ---
    const geom::LineEntity line(Point2(0, 0), Point2(10, 0));
    auto g = gripsOf(line);
    check(g.size() == 3 && same(g[0].at, 0, 0) && same(g[1].at, 10, 0) && same(g[2].at, 5, 0),
          "ligne : deux extremites et le milieu");
    auto out = moveGrip(line, g[1], Point2(10, 5));
    auto* l = dynamic_cast<geom::LineEntity*>(out.get());
    check(l && same(l->start(), 0, 0) && same(l->end(), 10, 5), "ligne : l'extremite seule bouge");
    out = moveGrip(line, g[2], Point2(5, 3));
    l = dynamic_cast<geom::LineEntity*>(out.get());
    check(l && same(l->start(), 0, 3) && same(l->end(), 10, 3), "ligne : le milieu deplace la ligne");
    check(same(line.end(), 10, 0), "ligne : l'original reste intact");

    // --- Polyligne fermee : sommets, milieux des segments, trou ---
    geom::PolylineEntity square({Point2(0, 0), Point2(10, 0), Point2(10, 10), Point2(0, 10)}, true);
    square.addHole({Point2(4, 4), Point2(6, 4), Point2(6, 6)});
    g = gripsOf(square);
    check(g.size() == 8 + 6, "polyligne fermee : 4 sommets, 4 milieux, trou 3 + 3");
    out = moveGrip(square, g[2], Point2(12, 12));
    auto* p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && same(p->vertices()[2], 12, 12) && same(p->vertices()[1], 10, 0), "polyligne : un sommet etire");
    // Milieu du segment 1 (10,0)-(10,10) : les deux sommets glissent ensemble.
    const Grip seg = g[5];
    check(seg.role == GripRole::Segment && same(seg.at, 10, 5), "polyligne : milieu du segment 1");
    out = moveGrip(square, seg, Point2(13, 5));
    p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && same(p->vertices()[1], 13, 0) && same(p->vertices()[2], 13, 10) && same(p->vertices()[0], 0, 0),
          "polyligne : le milieu deplace le segment");
    // Segment de fermeture (sommet 3 vers sommet 0).
    out = moveGrip(square, g[7], Point2(-2, 5));
    p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && same(p->vertices()[3], -2, 10) && same(p->vertices()[0], -2, 0), "polyligne : segment de fermeture");
    out = moveGrip(square, g[8], Point2(3, 3));
    p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && same(p->holes()[0][0], 3, 3) && same(p->vertices()[0], 0, 0), "polyligne : sommet du trou");

    // Polyligne ouverte : pas de segment de fermeture.
    const geom::PolylineEntity open({Point2(0, 0), Point2(5, 0), Point2(5, 5)}, false);
    check(gripsOf(open).size() == 3 + 2, "polyligne ouverte : 3 sommets, 2 milieux");

    // --- Cercle : centre, quadrants ---
    const geom::CircleEntity circle(Point2(0, 0), 5);
    g = gripsOf(circle);
    check(g.size() == 5 && same(g[1].at, 5, 0) && same(g[2].at, 0, 5), "cercle : centre et quatre quadrants");
    out = moveGrip(circle, g[2], Point2(0, 8));
    auto* c = dynamic_cast<geom::CircleEntity*>(out.get());
    check(c && near(c->radius(), 8) && same(c->center(), 0, 0), "cercle : un quadrant change le rayon");
    out = moveGrip(circle, g[0], Point2(3, 4));
    c = dynamic_cast<geom::CircleEntity*>(out.get());
    check(c && same(c->center(), 3, 4) && near(c->radius(), 5), "cercle : le centre deplace");
    check(!moveGrip(circle, g[1], Point2(0, 0)), "cercle : rayon nul refuse");

    // --- Arc : extremites, milieu, centre ---
    const geom::ArcEntity arc(Point2(0, 0), 10, 0.0, std::numbers::pi);
    g = gripsOf(arc);
    check(g.size() == 4 && same(g[0].at, 10, 0) && same(g[2].at, 0, 10), "arc : extremites, milieu, centre");
    out = moveGrip(arc, g[2], Point2(0, 5));   // plus plat, memes extremites
    auto* a = dynamic_cast<geom::ArcEntity*>(out.get());
    check(a && same(a->startPoint(), 10, 0) && same(a->endPoint(), -10, 0) && a->radius() > 10,
          "arc : le milieu change la courbure, extremites gardees");
    out = moveGrip(arc, g[1], Point2(0, -10));
    a = dynamic_cast<geom::ArcEntity*>(out.get());
    check(a && near(a->radius(), 10) && same(a->center(), 0, 0), "arc : une extremite glisse sur le meme cercle");
    check(!moveGrip(arc, g[2], Point2(0, 0)), "arc : trois points alignes refuses");

    // --- Point, texte, cotation ---
    const geom::PointEntity pt(Point2(1, 1));
    out = moveGrip(pt, gripsOf(pt)[0], Point2(4, 5));
    check(same(static_cast<geom::PointEntity&>(*out).position(), 4, 5), "point : deplace");
    const geom::LinearDimensionEntity dim(Point2(0, 0), Point2(10, 0), Point2(5, 3));
    g = gripsOf(dim);
    check(g.size() == 3, "cotation lineaire : deux points de definition et la ligne de cote");
    out = moveGrip(dim, g[2], Point2(5, 8));
    auto* d = dynamic_cast<geom::LinearDimensionEntity*>(out.get());
    check(d && same(d->dimLineLoc(), 5, 8) && same(d->defPt2(), 10, 0), "cotation : ligne de cote deplacee");

    if (failures) return 1;
    std::printf("Poignees : tests PASSED\n");
    return 0;
}
