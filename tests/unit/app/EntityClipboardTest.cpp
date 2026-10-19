// Presse-papiers des objets (E-01) : copies independantes, point de base,
// collage deplace, calques recrees dans un autre dessin.

#include "detail/EntityClipboard.h"

#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"

#include <cmath>
#include <cstdio>

using namespace bcad;
using app::EntityClipboard;
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
} // namespace

int main() {
    layers::LayerManager source;
    layers::Layer& murs = source.createLayer("Murs", geom::Color::fromRgb255(255, 0, 0));
    murs.lineWeight = 0.5;
    murs.lineType = layers::Layer::LineType::Dashed;

    geom::LineEntity line(Point2(10, 20), Point2(30, 20));
    line.setLayer("Murs");
    line.selected = true;
    geom::CircleEntity circle(Point2(40, 40), 5);

    const std::vector<geom::Entity*> picked{&line, &circle};
    const Point2 base = EntityClipboard::lowerLeft(picked);
    check(near(base.x_, 10) && near(base.y_, 20), "point de base : coin inferieur gauche de l'emprise");

    EntityClipboard clip;
    check(clip.empty(), "vide au depart");
    clip.store(picked, base, source);
    check(clip.size() == 2 && !clip.entities()[0]->selected, "copies deselectionnees");

    // L'original peut changer ou disparaitre : le presse-papiers garde sa copie.
    line.setStart(Point2(0, 0));
    auto pasted = clip.clonesAt(Point2(110, 120));
    auto* l = dynamic_cast<geom::LineEntity*>(pasted[0].get());
    check(l && near(l->start().x_, 110) && near(l->start().y_, 120) && near(l->end().x_, 130),
          "collage : le point de base vient sur le point d'insertion");
    check(l && l->layer() == "Murs", "collage : le calque est garde");
    auto again = clip.clonesAt(Point2(10, 20));
    check(again.size() == 2 && near(static_cast<geom::LineEntity&>(*again[0]).start().x_, 10),
          "collage aux coordonnees d'origine, plusieurs fois");

    // Autre dessin sans le calque : recree avec ses reglages.
    layers::LayerManager target;
    clip.ensureLayers(target);
    const layers::Layer* created = target.find("Murs");
    check(created && near(created->lineWeight, 0.5) && created->lineType == layers::Layer::LineType::Dashed &&
              created->color == geom::Color::fromRgb255(255, 0, 0),
          "calque manquant recree avec ses reglages");

    // Dessin qui a deja le calque : le sien reste.
    layers::LayerManager other;
    other.createLayer("Murs", geom::Color::fromRgb255(0, 0, 255));
    clip.ensureLayers(other);
    check(other.find("Murs")->color == geom::Color::fromRgb255(0, 0, 255), "calque existant garde ses reglages");

    // Une nouvelle copie remplace l'ancienne.
    clip.store({&circle}, Point2(35, 35), source);
    check(clip.size() == 1 && near(clip.base().x_, 35), "nouvelle copie remplace le contenu");

    if (failures) return 1;
    std::printf("Presse-papiers : tests PASSED\n");
    return 0;
}
