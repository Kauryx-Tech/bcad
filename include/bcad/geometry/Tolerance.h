#pragma once

#include <cmath>

namespace bcad::geom {

// Tolérances numériques centralisées. Les prédicats exacts de CGAL garantissent
// déjà des tests d'orientation/d'appartenance corrects quel que soit l'epsilon —
// ces constantes répondent à un autre type de question : « cette longueur/cet angle
// est-il essentiellement nul », utilisé avant une division ou comme garde-fou contre
// une entrée dégénérée. Des constantes nommées plutôt que des nombres magiques éparpillés
// par site d'appel, afin que chaque vérification de ce type ait le même sens dans tout le code et puisse être réglée en un seul endroit.
struct Tolerance {
    // En dessous de cette valeur, deux longueurs/coordonnées sont considérées comme égales.
    static constexpr double kLinear = 1e-9;

    // Garde-fou utilisé avant de diviser par une longueur qui pourrait légitimement
    // être nulle (par ex. un segment ou un vecteur dégénéré, de longueur nulle).
    static constexpr double kDegenerateLength = 1e-12;

    // En dessous de cette valeur, deux angles (radians) sont considérés comme égaux.
    static constexpr double kAngular = 1e-9;
};

inline bool nearlyZero(double v, double tol = Tolerance::kLinear) { return std::abs(v) < tol; }
inline bool nearlyEqual(double a, double b, double tol = Tolerance::kLinear) { return std::abs(a - b) < tol; }

} // namespace bcad::geom
