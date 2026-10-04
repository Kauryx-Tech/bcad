// Dessin des cotations (A-01) et cotation lineaire horizontale/verticale (A-02).
//
// Les entites de cotation se dessinaient comme une ligne brisee de trois points
// sans texte, et la cotation lineaire mesurait toujours la distance oblique.
// Le dessin (lignes d'attache, ligne de cote, fleches, texte lisible) est
// calcule une fois et sert au canevas, au PDF, a la selection et a l'emprise.

#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/RadialDimensionEntity.h"
#include "bcad/geometry/Transform2D.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <sstream>

using namespace bcad::geom;

namespace {

constexpr double kPi = std::numbers::pi;

bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) < tol; }

bool passesThrough(const std::vector<Point2>& path, const Point2& p) {
    return std::any_of(path.begin(), path.end(),
                       [&](const Point2& q) { return near(q.x_, p.x_) && near(q.y_, p.y_); });
}

} // namespace

int main() {
    // --- Lineaire horizontale : mesure en X, pieds sur la ligne de cote ---
    LinearDimensionEntity horizontal({0, 0}, {10, 3}, {5, 8}, 0.0);
    horizontal.properties().setDouble(kDimensionTextHeightProperty, 1.0);
    assert(near(horizontal.measuredValue(), 10.0));
    assert(horizontal.dimensionText() == "10.00");
    auto g = dimensionGraphics(horizontal);
    assert(passesThrough(g.path, {0, 8}) && passesThrough(g.path, {10, 8}));   // pieds a y = 8
    assert(passesThrough(g.path, {0, 0.25}));       // ligne d'attache : ecart h/4 a l'objet
    assert(passesThrough(g.path, {0, 8.5}));        // depassement h/2
    // Fleche au premier pied : barbes a une hauteur de texte vers l'interieur.
    assert(passesThrough(g.path, {1, 1.0 / 6.0 + 8}) && passesThrough(g.path, {1, 8 - 1.0 / 6.0}));
    assert(near(g.label.angle, 0) && near(g.label.position.x_, 5) && near(g.label.position.y_, 8.25));
    assert(g.label.text == "10.00" && near(g.label.height, 1.0));
    assert(horizontal.tessellate(0.01) == g.path);

    // --- Lineaire verticale : mesure en Y, texte lisible de bas en haut ---
    LinearDimensionEntity vertical({0, 0}, {10, 3}, {15, 1}, kPi / 2);
    assert(near(vertical.measuredValue(), 3.0));
    g = dimensionGraphics(vertical);
    assert(passesThrough(g.path, {15, 0}) && passesThrough(g.path, {15, 3}));
    assert(near(g.label.angle, kPi / 2));
    // Sans propriete : hauteur par defaut.
    assert(near(g.label.height, kDefaultDimensionTextHeight));

    // --- Alignee : parallele aux origines, mesure oblique ---
    AlignedDimensionEntity aligned({0, 0}, {3, 4}, {-4, 3});
    aligned.properties().setDouble(kDimensionTextHeightProperty, 0.5);
    assert(near(aligned.measuredValue(), 5.0));
    g = dimensionGraphics(aligned);
    // Decalage de 5 suivant la normale (-0,8 ; 0,6).
    assert(passesThrough(g.path, {-4, 3}) && passesThrough(g.path, {-1, 7}));
    assert(near(g.label.angle, std::atan2(4.0, 3.0)));

    // --- Texte jamais a l'envers ---
    assert(near(readableAngle(kPi), 0.0));
    assert(near(readableAngle(-kPi / 2), kPi / 2));
    assert(near(readableAngle(3 * kPi / 4), -kPi / 4));
    AlignedDimensionEntity backwards({10, 0}, {0, 0}, {5, -2});   // origines de droite a gauche
    assert(near(dimensionGraphics(backwards).label.angle, 0.0));

    // --- Angulaire : arc au rayon de la position, entre les deux cotes ---
    AngularDimensionEntity angular({0, 0}, {5, 0}, {0, 5}, {3, 4});
    angular.properties().setDouble(kDimensionTextHeightProperty, 0.2);
    assert(near(angular.measuredValue(), 90.0) && angular.dimensionText() == "90.0\xC2\xB0");
    g = dimensionGraphics(angular);
    assert(passesThrough(g.path, {5, 0}) && passesThrough(g.path, {0, 5}));   // rayon 5
    for (const auto& p : g.path) assert(std::hypot(p.x_, p.y_) <= 5.0 + 0.2 + 1e-9);
    assert(near(std::hypot(g.label.position.x_, g.label.position.y_), 5.05));

    // --- Rayon et diametre ---
    RadialDimensionEntity radius({0, 0}, {4, 0}, RadialDimensionEntity::RadialType::Radius, {2, 0});
    assert(radius.dimensionText() == "R 4.00");
    g = dimensionGraphics(radius);
    assert(passesThrough(g.path, {0, 0}) && passesThrough(g.path, {4, 0}));
    RadialDimensionEntity diameter({0, 0}, {4, 0}, RadialDimensionEntity::RadialType::Diameter, {0, 0});
    assert(near(diameter.measuredValue(), 8.0) && diameter.dimensionText() == "\xC3\x98 8.00");
    assert(passesThrough(dimensionGraphics(diameter).path, {-4, 0}));

    // --- Selection sur le trace, emprise avec le texte ---
    assert(near(horizontal.distanceTo({5, 8}), 0.0));        // sur la ligne de cote
    assert(horizontal.distanceTo({5, 4}) > 3.0);               // ni sur la mesure oblique
    const BoundingBox bb = horizontal.boundingBox();
    assert(bb.maxY >= 8.25 + 1.0 - 1e-9 && near(bb.minY, 0.25));

    // Toute la largeur du texte se selectionne, pas seulement son milieu.
    LinearDimensionEntity longText({0, 0}, {1000, 0}, {500, 2}, 0.0);
    longText.properties().setDouble(kDimensionTextHeightProperty, 1.0);
    const auto lg = dimensionGraphics(longText);              // « 1000.00 » : 7 caracteres
    assert(near(longText.distanceTo({lg.label.position.x_ + 1.8, lg.label.position.y_ + 0.5}), 0.0));

    // --- Symetrie : la direction de cote suit (axe a 45°, x <-> y) ---
    LinearDimensionEntity mirrored({0, 0}, {10, 5}, {5, 8}, 0.0);
    mirrored.applyTransform(Transform2D::mirrorAcrossLine({0, 0}, {1, 1}));
    assert(near(std::abs(std::cos(mirrored.rotation())), 0.0, 1e-12));   // devenue verticale
    assert(near(mirrored.measuredValue(), 10.0));

    // --- Copie : proprietes et calque gardes ---
    horizontal.setLayer("Cotations");
    auto copy = horizontal.clone();
    assert(copy->layer() == "Cotations");
    assert(near(dimensionTextHeight(static_cast<const DimensionEntity&>(*copy)), 1.0));

    // --- DXF : lignes et texte lisibles partout, plus d'entite DIMENSION ---
    std::ostringstream dxf;
    horizontal.writeDxf(dxf, "Cotations", std::nullopt);
    const std::string out = dxf.str();
    assert(out.find("DIMENSION") == std::string::npos);
    assert(out.rfind("0\nLINE\n8\nCotations\n", 0) == 0);
    assert(out.find("0\nTEXT\n8\nCotations\n") != std::string::npos);
    assert(out.find("1\n10.00\n") != std::string::npos && out.find("72\n1\n") != std::string::npos);
    // Chaque groupe est une paire code/valeur : nombre de lignes pair, pas de
    // « 0 » orphelin en fin d'entite.
    std::size_t lines = 0;
    for (char ch : out) lines += ch == '\n';
    assert(lines % 2 == 0);
    // Angle du texte en degres.
    std::ostringstream vdxf;
    vertical.writeDxf(vdxf, "C", std::nullopt);
    assert(vdxf.str().find("50\n90\n") != std::string::npos);

    std::printf("Dessin des cotations : tests PASSED\n");
    return 0;
}
