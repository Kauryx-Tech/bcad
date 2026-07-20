#pragma once

#include "bcad/geometry/Types.h"
#include <cmath>

namespace bcad::geom {

// Fine fabrique par-dessus CGAL::Aff_transformation_2. Les transformations se composent avec '*'
// (convention CGAL : (a * b)(p) == a(b(p))).
class Transform2D {
public:
    static AffTransform2 identity() {
        return AffTransform2(CGAL::IDENTITY);
    }

    static AffTransform2 translation(double dx, double dy) {
        return AffTransform2(CGAL::TRANSLATION, Vector2(dx, dy));
    }

    static AffTransform2 rotation(double radians, const Point2& pivot = Point2(0, 0)) {
        AffTransform2 toOrigin(CGAL::TRANSLATION, Vector2(-pivot.x(), -pivot.y()));
        AffTransform2 rot(CGAL::ROTATION, std::sin(radians), std::cos(radians));
        AffTransform2 back(CGAL::TRANSLATION, Vector2(pivot.x(), pivot.y()));
        return back * rot * toOrigin;
    }

    static AffTransform2 scaling(double factor, const Point2& pivot = Point2(0, 0)) {
        return scaling(factor, factor, pivot);
    }

    static AffTransform2 scaling(double sx, double sy, const Point2& pivot = Point2(0, 0)) {
        AffTransform2 toOrigin(CGAL::TRANSLATION, Vector2(-pivot.x(), -pivot.y()));
        AffTransform2 scale(sx, 0, 0, sy);
        AffTransform2 back(CGAL::TRANSLATION, Vector2(pivot.x(), pivot.y()));
        return back * scale * toOrigin;
    }

    static AffTransform2 mirrorX() { return AffTransform2(1, 0, 0, -1); }
    static AffTransform2 mirrorY() { return AffTransform2(-1, 0, 0, 1); }

    // Réflexion par rapport à la droite arbitraire passant par p1/p2 (le cas général
    // de l'outil Miroir — mirrorX/mirrorY ne couvrent que les cas particuliers alignés
    // sur les axes). Construction standard de matrice de réflexion par rapport à une
    // droite passant par l'origine, composée avec une translation pour que la droite
    // n'ait pas besoin de passer par l'origine du monde.
    static AffTransform2 mirrorAcrossLine(const Point2& p1, const Point2& p2) {
        double angle = std::atan2(CGAL::to_double(p2.y() - p1.y()), CGAL::to_double(p2.x() - p1.x()));
        double c2 = std::cos(2.0 * angle);
        double s2 = std::sin(2.0 * angle);
        AffTransform2 reflect(c2, s2, s2, -c2);
        AffTransform2 toOrigin(CGAL::TRANSLATION, Vector2(-p1.x(), -p1.y()));
        AffTransform2 back(CGAL::TRANSLATION, Vector2(p1.x(), p1.y()));
        return back * reflect * toOrigin;
    }
};

} // namespace bcad::geom
