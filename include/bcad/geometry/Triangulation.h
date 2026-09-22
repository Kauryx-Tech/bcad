#pragma once

#include "bcad/geometry/Point.h"
#include <array>
#include <vector>

namespace bcad::geom {

struct Triangle {
    std::array<Point2, 3> points;
};

// Triangulation de Delaunay simple d'un nuage de points (domaine de l'enveloppe convexe).
std::vector<Triangle> delaunayTriangulate(const std::vector<Point2>& points);

// Triangulation de Delaunay contrainte d'un polygone (éventuellement non convexe),
// utilisée pour remplir les polylignes/polygones fermés en vue du rendu ou de l'export.
// `holes` désigne des contours intérieurs optionnels (par ex. issus du résultat d'une différence booléenne).
std::vector<Triangle> triangulatePolygon(const std::vector<Point2>& outerBoundary,
                                          const std::vector<std::vector<Point2>>& holes = {});

} // namespace bcad::geom