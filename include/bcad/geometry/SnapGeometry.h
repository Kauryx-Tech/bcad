#pragma once

#include "bcad/geometry/Entity.h"
#include <optional>
#include <vector>

namespace bcad::geom {

// Tous les points d'intersection géométriques exacts entre deux entités (calculés à
// partir de leur représentation exacte, pas de leur tessellation). Prend en charge
// toute combinaison de Line/Circle/Arc/Polyline ; les paires non prises en charge
// (par ex. tout ce qui implique PointEntity) retournent un vecteur vide plutôt
// qu'une erreur, car « aucune intersection » est une réponse légitime pour une requête de snap.
std::vector<Point2> entityIntersections(const Entity& a, const Entity& b);

// Pied de la perpendiculaire depuis `reference` sur l'entité `e`. `cursorHint`
// permet de lever l'ambiguïté sur le segment/la branche d'une entité à plusieurs
// parties (une polyligne) que l'utilisateur désigne réellement. Retourne nullopt si
// le type d'entité n'a pas de perpendiculaire bien définie, ou si le pied tomberait hors du balayage d'un arc.
std::optional<Point2> perpendicularFoot(const Entity& e, const Point2& reference, const Point2& cursorHint);

} // namespace bcad::geom
