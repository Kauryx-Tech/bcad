#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Aff_transformation_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

namespace bcad::geom {

// Noyau à prédicats robustes / constructions rapides : le choix standard pour
// la géométrie CAO interactive (assez exact pour les booléens, assez rapide pour l'UI).
using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;

using Point2 = Kernel::Point_2;
using Vector2 = Kernel::Vector_2;
using AffTransform2 = CGAL::Aff_transformation_2<Kernel>;
using Polygon2 = CGAL::Polygon_2<Kernel>;
using PolygonWithHoles2 = CGAL::Polygon_with_holes_2<Kernel>;

struct Color {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    static Color fromRgb255(int r255, int g255, int b255, float a = 1.0f) {
        return Color{ r255 / 255.0f, g255 / 255.0f, b255 / 255.0f, a };
    }
};

} // namespace bcad::geom
