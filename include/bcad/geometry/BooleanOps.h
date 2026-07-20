#pragma once

#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Types.h"
#include <vector>

namespace bcad::geom {

enum class BooleanOp { Union, Intersection, Difference, SymmetricDifference };

// Opérations booléennes sur polygones basées sur CGAL::Boolean_set_operations_2.
// Les entrées doivent être des polylignes fermées et simples (sans auto-intersection) ;
// le résultat peut contenir des trous et/ou plusieurs morceaux disjoints, il est
// donc retourné sous forme d'une PolylineEntity par contour extérieur (l'aplatissement
// des trous dans la liste de sommets de ce contour n'est PAS effectué — les appelants
// qui ont besoin des trous doivent utiliser booleanOpWithHoles à la place).
std::vector<PolygonWithHoles2> booleanOpWithHoles(const PolylineEntity& a,
                                                   const PolylineEntity& b,
                                                   BooleanOp op);

// Enveloppe pratique : uniquement les contours extérieurs, sous forme de polylignes prêtes à tracer.
std::vector<PolylineEntity> booleanOp(const PolylineEntity& a,
                                       const PolylineEntity& b,
                                       BooleanOp op);

double polygonArea(const PolylineEntity& polyline);

bool isSimplePolygon(const PolylineEntity& polyline);

} // namespace bcad::geom
