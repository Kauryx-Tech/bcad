#pragma once

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Types.h"
#include <cctype>
#include <cmath>
#include <optional>
#include <string>

namespace bcad::app {

// Analyse la saisie de coordonnées au clavier façon AutoCAD, l'alternative
// au clic pour placer un point avec l'outil de dessin actif :
//   "12,7"      cartésien absolu
//   "12, 7"     (les espaces autour de la virgule sont tolérés)
//   "@5,3"      cartésien relatif, depuis `reference`
//   "@10<45"    polaire relatif : distance 10, angle 45 deg (0 = +X, sens
//               trigonométrique)
//   "10<45"     polaire absolu, depuis l'origine
// Renvoie nullopt pour une saisie mal formée, ou pour une forme relative
// ('@') utilisée sans point de référence (aucun point n'a encore été placé
// avec cet outil).
// Header-only : analyse pure de std::string/double, sans dépendance à Qt, ce
// qui la rend directement testable unitairement depuis le smoke test sans
// lier Qt.
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
            // Rejette les caractères en trop ("12abc") plutôt que de tronquer en silence.
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
