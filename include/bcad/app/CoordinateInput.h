#pragma once

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Types.h"
#include <cctype>
#include <cmath>
#include <optional>
#include <string>

namespace bcad::app {

// Parses AutoCAD-style typed coordinate entry, the keyboard alternative to
// clicking a point for the active drawing tool:
//   "12,7"      absolute cartesian
//   "12, 7"     (whitespace around the comma is tolerated)
//   "@5,3"      relative cartesian, from `reference`
//   "@10<45"    relative polar: distance 10, angle 45 deg (0 = +X, CCW+)
//   "10<45"     absolute polar, from the origin
// Returns nullopt for malformed input, or for a relative ('@') form used
// without a reference point (no point has been placed yet in this tool).
// Header-only: pure std::string/double parsing, no Qt dependency, so it's
// directly unit-testable from the smoke test without linking Qt.
inline std::optional<geom::Point2> parseCoordinateInput(const std::string& textIn,
                                                          std::optional<geom::Point2> reference) {
    std::string text = textIn;
    std::size_t b = text.find_first_not_of(" \t");
    std::size_t e = text.find_last_not_of(" \t");
    if (b == std::string::npos) return std::nullopt;
    text = text.substr(b, e - b + 1);
    if (text.empty()) return std::nullopt;

    bool relative = text.front() == '@';
    if (relative) text = text.substr(1);
    if (relative && !reference) return std::nullopt;
    if (text.empty()) return std::nullopt;

    auto trim = [](std::string s) {
        std::size_t sb = s.find_first_not_of(" \t");
        std::size_t se = s.find_last_not_of(" \t");
        return sb == std::string::npos ? std::string() : s.substr(sb, se - sb + 1);
    };
    auto parseDouble = [&](const std::string& raw, double& out) -> bool {
        std::string s = trim(raw);
        if (s.empty()) return false;
        try {
            std::size_t consumed = 0;
            out = std::stod(s, &consumed);
            // Reject trailing garbage ("12abc") rather than silently truncating.
            return consumed == s.size();
        } catch (...) {
            return false;
        }
    };

    std::size_t ltPos = text.find('<');
    if (ltPos != std::string::npos) {
        double dist = 0.0, angleDeg = 0.0;
        if (!parseDouble(text.substr(0, ltPos), dist)) return std::nullopt;
        if (!parseDouble(text.substr(ltPos + 1), angleDeg)) return std::nullopt;

        double baseX = 0.0, baseY = 0.0;
        if (relative) {
            baseX = CGAL::to_double(reference->x());
            baseY = CGAL::to_double(reference->y());
        }
        double angleRad = geom::toRadians(angleDeg);
        return geom::Point2(baseX + dist * std::cos(angleRad), baseY + dist * std::sin(angleRad));
    }

    std::size_t commaPos = text.find(',');
    if (commaPos == std::string::npos) return std::nullopt;
    double x = 0.0, y = 0.0;
    if (!parseDouble(text.substr(0, commaPos), x)) return std::nullopt;
    if (!parseDouble(text.substr(commaPos + 1), y)) return std::nullopt;

    if (relative) {
        return geom::Point2(CGAL::to_double(reference->x()) + x, CGAL::to_double(reference->y()) + y);
    }
    return geom::Point2(x, y);
}

} // namespace bcad::app
