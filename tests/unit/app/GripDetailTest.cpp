// Poignees (M-02) : emplacement des poignees et objet obtenu en tirant
// chacune, calculs purs de GripDetail.h.

#include "detail/GripDetail.h"

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

    // --- Plusieurs poignees chaudes d'un objet (Maj+clic) ---
    g = gripsOf(line);
    out = app::detail::moveGrips(line, {g[0], g[1]}, 3, 4);
    l = dynamic_cast<geom::LineEntity*>(out.get());
    check(l && same(l->start(), 3, 4) && same(l->end(), 13, 4), "deux extremites chaudes : la ligne se deplace");
    g = gripsOf(square);
    out = app::detail::moveGrips(square, {g[0], g[1]}, 0, -2);
    p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && same(p->vertices()[0], 0, -2) && same(p->vertices()[1], 10, -2) && same(p->vertices()[2], 10, 10),
          "deux sommets chauds : seuls eux bougent");

    // --- Modes : Entree les fait defiler, transformation depuis la base ---
    using app::detail::GripMode;
    using app::detail::gripTransform;
    using app::detail::nextGripMode;
    check(nextGripMode(GripMode::Stretch) == GripMode::Move && nextGripMode(GripMode::Mirror) == GripMode::Stretch,
          "modes : Etirer, Deplacer, Rotation, Echelle, Miroir, Etirer");
    const Point2 base(10, 0);
    auto t = gripTransform(GripMode::Move, base, Point2(15, 5));
    check(t && same(t->transform(Point2(0, 0)), 5, 5), "deplacer : de la base au point");
    t = gripTransform(GripMode::Rotate, base, Point2(10, 7));   // 90 degres
    check(t && same(t->transform(Point2(20, 0)), 10, 10), "rotation : angle de la base au point");
    t = gripTransform(GripMode::Rotate, base, Point2(10, 7), std::numbers::pi / 2);
    check(t && same(t->transform(Point2(20, 0)), 20, 0), "rotation : la reference est retranchee");
    t = gripTransform(GripMode::Scale, base, Point2(12, 0));
    check(t && same(t->transform(Point2(11, 0)), 12, 0), "echelle : facteur = distance a la base");
    t = gripTransform(GripMode::Scale, base, Point2(16, 0), 2.0);
    check(t && same(t->transform(Point2(11, 0)), 13, 0), "echelle : longueur / reference");
    t = gripTransform(GripMode::Mirror, base, Point2(10, 5));
    check(t && same(t->transform(Point2(13, 1)), 7, 1), "miroir : axe de la base au point");
    check(!gripTransform(GripMode::Move, base, base), "point confondu avec la base : rien");

    // --- Poignees multifonctions de la polyligne ---
    using app::detail::insertVertex;
    using app::detail::removeVertex;
    g = gripsOf(square);
    out = insertVertex(square, g[1], Point2(12, 5));            // apres le sommet 1
    p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && p->vertices().size() == 5 && same(p->vertices()[2], 12, 5), "ajouter un sommet apres une poignee de sommet");
    out = insertVertex(square, g[4], Point2(5, -3));            // segment 0
    p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && same(p->vertices()[1], 5, -3) && same(p->vertices()[2], 10, 0), "ajouter un sommet dans un segment");
    out = removeVertex(square, g[2]);
    p = dynamic_cast<geom::PolylineEntity*>(out.get());
    check(p && p->vertices().size() == 3 && same(p->vertices()[2], 0, 10), "supprimer un sommet");
    const geom::PolylineEntity triangle({Point2(0, 0), Point2(5, 0), Point2(0, 5)}, true);
    check(!removeVertex(triangle, gripsOf(triangle)[0]), "triangle ferme : pas moins de 3 sommets");
    check(!removeVertex(open, gripsOf(open)[3]), "milieu de segment : pas de suppression");
    check(!insertVertex(line, gripsOf(line)[0], Point2(1, 1)), "ligne : pas de sommet a ajouter");

    if (failures) return 1;
    std::printf("Poignees : tests PASSED\n");
    return 0;
}
