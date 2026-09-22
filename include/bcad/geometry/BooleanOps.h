#pragma once

#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Types.h"
#include <vector>

namespace bcad::geom {

enum class BooleanOp { Union, Intersection, Difference, SymmetricDifference };

// Opérations booléennes sur polygones (implémentation basée sur CGAL::Boolean_set_operations_2 en interne).
// Les entrées doivent être des polylignes fermées et simples (sans auto-intersection) ;
// le résultat peut contenir des trous et/ou plusieurs morceaux disjoints, il est
// donc retourné sous forme d'une PolylineEntity par contour extérieur.
std::vector<PolylineEntity> booleanOp(const PolylineEntity& a,
                                       const PolylineEntity& b,
                                       BooleanOp op);

// Version qui préserve les trous (retourne un PolygonWithHoles2 par résultat).
struct PolygonWithHolesResult {
    PolylineEntity outer;
    std::vector<PolylineEntity> holes;
};

std::vector<PolygonWithHolesResult> booleanOpWithHoles(const PolylineEntity& a,
                                                        const PolylineEntity& b,
                                                        BooleanOp op);

double polygonArea(const PolylineEntity& polyline);

bool isSimplePolygon(const PolylineEntity& polyline);

} // namespace bcad::geom