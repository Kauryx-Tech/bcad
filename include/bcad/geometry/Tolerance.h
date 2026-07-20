#pragma once

#include <cmath>

namespace bcad::geom {

// Centralized numeric tolerances. CGAL's exact predicates already guarantee
// correct orientation/containment tests regardless of epsilon — these
// constants are for the other kind of question: "is this length/angle
// basically zero", used before a division or as a degenerate-input guard.
// Named constants instead of magic numbers scattered per call site, so every
// such check in the codebase means the same thing and can be retuned once.
struct Tolerance {
    // Below this, two lengths/coordinates are treated as equal.
    static constexpr double kLinear = 1e-9;

    // Guard used before dividing by a length that could legitimately be
    // zero (e.g. a degenerate/zero-length segment or vector).
    static constexpr double kDegenerateLength = 1e-12;

    // Below this, two angles (radians) are treated as equal.
    static constexpr double kAngular = 1e-9;
};

inline bool nearlyZero(double v, double tol = Tolerance::kLinear) { return std::abs(v) < tol; }
inline bool nearlyEqual(double a, double b, double tol = Tolerance::kLinear) { return std::abs(a - b) < tol; }

} // namespace bcad::geom
