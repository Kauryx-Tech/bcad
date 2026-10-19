// Champs de la palette Propriétés (P-01) : champs exposés, valeurs lues,
// écrites et refusées, calculs purs de GeometryFieldDetail.h.

#include "detail/GeometryFieldDetail.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

using namespace bcad;
using app::detail::formatFieldValue;
using app::detail::GeometryEditReject;
using app::detail::GeometryField;
using app::detail::geometryFields;
using app::detail::geometryFieldValue;
using app::detail::geometryVertexCount;
using app::detail::isReadOnlyGeometryField;
using app::detail::parseFieldValue;
using app::detail::sameFieldValue;
using app::detail::withGeometryField;
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
bool same(const std::vector<GeometryField>& got, const std::vector<GeometryField>& want) { return got == want; }
double value(const geom::Entity& e, GeometryField f, int vertex = 0) {
    const std::optional<double> v = geometryFieldValue(e, f, vertex);
    return v ? *v : std::nan("");
}
// Écriture sur une copie, avec le refus attendu et la valeur resultante.
void writeCheck(const geom::Entity& e, GeometryField f, double v, GeometryEditReject expectedReject,
                const char* what, const geom::Entity* expected = nullptr) {
    GeometryEditReject reject = GeometryEditReject::None;
    const std::unique_ptr<geom::Entity> out = withGeometryField(e, f, 0, v, &reject);
    if (expectedReject != GeometryEditReject::None) {
        check(!out && reject == expectedReject, what);
        return;
    }
    check(out && reject == GeometryEditReject::None, what);
    if (!out || !expected) return;
    check(out->serializeParams() == expected->serializeParams(), what);
}
} // namespace

int main() {
    // --- Ligne : deux extrémités éditables, longueur calculée ---
    const geom::LineEntity line(Point2(0, 0), Point2(10, 0));
    check(same(geometryFields(line), {GeometryField::StartX, GeometryField::StartY, GeometryField::EndX,
                                       GeometryField::EndY, GeometryField::Length}),
          "ligne : début X/Y, fin X/Y, longueur");
    check(near(value(line, GeometryField::EndX), 10) && near(value(line, GeometryField::EndY), 0),
          "ligne : coordonnées de la fin lues");
    check(near(value(line, GeometryField::Length), 10), "ligne : longueur calculée");
    check(isReadOnlyGeometryField(GeometryField::Length), "ligne : longueur en lecture seule");
    check(!isReadOnlyGeometryField(GeometryField::EndY), "ligne : fin Y éditable");

    writeCheck(line, GeometryField::EndY, 5, GeometryEditReject::None, "ligne : on écrit la fin Y");
    {
        const std::unique_ptr<geom::Entity> out = withGeometryField(line, GeometryField::EndY, 0, 5);
        const auto* moved = dynamic_cast<const geom::LineEntity*>(out.get());
        check(moved && near(moved->end().x_, 10) && near(moved->end().y_, 5), "ligne : la fin vaut (10, 5)");
        check(near(line.end().y_, 0), "ligne : l'original reste intact");
    }
    // Début et fin confondus : segment de longueur nulle, refusé.
    writeCheck(line, GeometryField::EndX, 0, GeometryEditReject::Degenerate, "ligne : fin sur le début");
    writeCheck(line, GeometryField::StartX, 10, GeometryEditReject::Degenerate, "ligne : début sur la fin");
    writeCheck(line, GeometryField::Length, 3, GeometryEditReject::ReadOnly, "ligne : longueur non écrite");
    writeCheck(line, GeometryField::Radius, 3, GeometryEditReject::NotApplicable, "ligne : rayon sans objet");
    writeCheck(line, GeometryField::StartX, std::nan(""), GeometryEditReject::NotFinite,
               "ligne : valeur non finie refusée");

    // --- Cercle : centre, rayon éditables, diamètre et aire calculés ---
    const geom::CircleEntity circle(Point2(1, 2), 3);
    check(same(geometryFields(circle), {GeometryField::CenterX, GeometryField::CenterY, GeometryField::Radius,
                                         GeometryField::Diameter, GeometryField::Area}),
          "cercle : centre, rayon, diamètre, aire");
    check(near(value(circle, GeometryField::Diameter), 6), "cercle : diamètre = 2 x rayon");
    check(near(value(circle, GeometryField::Area), std::numbers::pi * 9), "cercle : aire = pi r²");
    check(isReadOnlyGeometryField(GeometryField::Area), "cercle : aire en lecture seule");
    writeCheck(circle, GeometryField::Radius, 0, GeometryEditReject::RadiusNotPositive, "cercle : rayon nul");
    writeCheck(circle, GeometryField::Radius, -2, GeometryEditReject::RadiusNotPositive, "cercle : rayon négatif");
    {
        const std::unique_ptr<geom::Entity> out = withGeometryField(circle, GeometryField::CenterX, 0, 7);
        const auto* moved = dynamic_cast<const geom::CircleEntity*>(out.get());
        check(moved && near(moved->center().x_, 7) && near(moved->center().y_, 2) && near(moved->radius(), 3),
              "cercle : déplacer le centre garde le rayon");
    }

    // --- Arc : angles en degrés, longueur d'arc calculée ---
    const geom::ArcEntity arc(Point2(0, 0), 2, 0.0, std::numbers::pi / 2);
    check(same(geometryFields(arc), {GeometryField::CenterX, GeometryField::CenterY, GeometryField::Radius,
                                      GeometryField::StartAngle, GeometryField::EndAngle, GeometryField::ArcLength}),
          "arc : centre, rayon, angles, longueur d'arc");
    check(near(value(arc, GeometryField::StartAngle), 0) && near(value(arc, GeometryField::EndAngle), 90),
          "arc : angles affichés en degrés");
    check(near(value(arc, GeometryField::ArcLength), std::numbers::pi), "arc : longueur d'arc = rayon x balayage");
    check(isReadOnlyGeometryField(GeometryField::ArcLength), "arc : longueur d'arc en lecture seule");
    {
        const std::unique_ptr<geom::Entity> out = withGeometryField(arc, GeometryField::StartAngle, 0, 45);
        const auto* turned = dynamic_cast<const geom::ArcEntity*>(out.get());
        check(turned && near(turned->startAngle(), std::numbers::pi / 4) && near(turned->endAngle(), std::numbers::pi / 2),
              "arc : angle de départ converti en radians");
        check(near(arc.startAngle(), 0), "arc : l'original reste intact");
    }
    writeCheck(arc, GeometryField::Radius, 0, GeometryEditReject::RadiusNotPositive, "arc : rayon nul");

    // --- Point : position seule ---
    const geom::PointEntity point(Point2(3, 4));
    check(same(geometryFields(point), {GeometryField::PositionX, GeometryField::PositionY}), "point : X et Y");
    writeCheck(point, GeometryField::PositionX, 7, GeometryEditReject::None, "point : on écrit X");
    {
        const std::unique_ptr<geom::Entity> out = withGeometryField(point, GeometryField::PositionX, 0, 7);
        const auto* moved = dynamic_cast<const geom::PointEntity*>(out.get());
        check(moved && near(moved->position().x_, 7) && near(moved->position().y_, 4), "point : position (7, 4)");
    }

    // --- Polyligne : sommet choisi, longueur, aire seulement si fermée ---
    const geom::PolylineEntity square({Point2(0, 0), Point2(10, 0), Point2(10, 10), Point2(0, 10)}, true);
    check(same(geometryFields(square), {GeometryField::VertexX, GeometryField::VertexY, GeometryField::Length,
                                         GeometryField::Area}),
          "polyligne fermée : sommet, longueur, aire");
    check(geometryVertexCount(square) == 4, "polyligne : quatre sommets éditables");
    check(near(value(square, GeometryField::Length), 40), "polyligne : longueur du contour");
    check(near(value(square, GeometryField::Area), 100), "polyligne : aire du carré");
    check(near(value(square, GeometryField::VertexX, 2), 10), "polyligne : sommet 2 lu");
    {
        GeometryEditReject reject = GeometryEditReject::None;
        const std::unique_ptr<geom::Entity> out =
            withGeometryField(square, GeometryField::VertexY, 2, 20, &reject);
        const auto* moved = dynamic_cast<const geom::PolylineEntity*>(out.get());
        check(out && reject == GeometryEditReject::None && moved && near(moved->vertices()[2].y_, 20),
              "polyligne : sommet 2 écrit");
        check(near(square.vertices()[2].y_, 10), "polyligne : l'original reste intact");
    }
    {
        // Sommet n° 5 d'un carré de 4 sommets : refusé (writeCheck vise toujours
        // le sommet 0, il ne peut pas exprimer ce cas).
        GeometryEditReject reject = GeometryEditReject::None;
        const std::unique_ptr<geom::Entity> out = withGeometryField(square, GeometryField::VertexX, 5, 3.0, &reject);
        check(!out && reject == GeometryEditReject::InvalidVertex, "polyligne : sommet hors champ");
    }
    const geom::PolylineEntity open({Point2(0, 0), Point2(10, 0), Point2(10, 10)}, false);
    check(same(geometryFields(open), {GeometryField::VertexX, GeometryField::VertexY, GeometryField::Length}),
          "polyligne ouverte : pas d'aire");
    check(!geometryFieldValue(open, GeometryField::Area), "polyligne ouverte : aire sans valeur");

    // --- Lecture d'une saisie : virgule décimale, bords tolérés, refus net ---
    check(parseFieldValue("12.5") && near(*parseFieldValue("12.5"), 12.5), "saisie : point décimal");
    check(parseFieldValue("12,5") && near(*parseFieldValue("12,5"), 12.5), "saisie : virgule décimale");
    check(parseFieldValue("  7  ") && near(*parseFieldValue("  7  "), 7), "saisie : espaces de bord");
    check(parseFieldValue("-4.5") && near(*parseFieldValue("-4.5"), -4.5), "saisie : négatif");
    check(!parseFieldValue(""), "saisie : vide refusé");
    check(!parseFieldValue("   "), "saisie : espaces seuls refusés");
    check(!parseFieldValue("12a"), "saisie : texte mêlé refusé");
    check(!parseFieldValue("1.2.3"), "saisie : deux points refusés");
    check(!parseFieldValue("nan"), "saisie : nan refusé");
    check(!parseFieldValue("inf"), "saisie : inf refusé");

    // --- Forme affichée : douze chiffres, pas de « -0 », retour stable ---
    check(formatFieldValue(12.5) == "12.5", "affichage : 12,5 s'écrit 12.5");
    check(formatFieldValue(0.0) == "0" && formatFieldValue(-0.0) == "0", "affichage : -0 s'écrit 0");
    check(formatFieldValue(-3) == "-3", "affichage : négatif");
    for (const double v : {10.0, 12.5, -0.125, 1234.5678, 0.0}) {
        const std::string shown = formatFieldValue(v);
        const std::optional<double> again = parseFieldValue(shown);
        check(again && *again == v, "affichage : relire la forme affichée rend la valeur");
    }
    check(sameFieldValue(10, 10) && sameFieldValue(10, *parseFieldValue("10")), "champ inchangé : rien à pousser");
    check(!sameFieldValue(10, 11) && !sameFieldValue(10, 10.5), "champ changé : écriture à faire");

    if (failures) return 1;
    std::printf("Champs geometrie : tests PASSED\n");
    return 0;
}
