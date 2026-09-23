#pragma once

// COGO (Coordinate Geometry) : calculs d'arpentage standard.
// Sources : Open Access Surveying Library Ch. A (Forward/Inverse Computation),
// Esri ArcGIS Pro COGO (azimut Nord 0-360°), guide CDOT COGO (gisements).
// Convention BCAD : Point2.x_ = Est, Point2.y_ = Nord (plan).

#include "bcad/geometry/Point.h"
#include <cmath>
#include <optional>
#include <sstream>
#include <string>

namespace bcad::cadastre {

inline constexpr double kPi = 3.14159265358979;
inline double deg2rad(double d) { return d * kPi / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / kPi; }

// Normalise un azimut dans [0, 360[
inline double normalizeAzimuth(double az) {
    double a = std::fmod(az, 360.0);
    if (a < 0) a += 360.0;
    return a;
}

// Forward : point d'arrivée depuis (from, azimut Nord horaire°, distance).
// dN = cos(az)*dist, dE = sin(az)*dist  (formules traverse, US Army EN0593).
inline bcad::geom::Point2 forward(const bcad::geom::Point2& from, double azimuthDeg, double dist) {
    double a = deg2rad(normalizeAzimuth(azimuthDeg));
    return {from.x_ + std::sin(a) * dist, from.y_ + std::cos(a) * dist};
}

struct InverseResult {
    double distance;
    double azimuthDeg; // 0-360, Nord horaire
};

// Inverse : distance + azimut entre deux points (to - from).
inline InverseResult inverse(const bcad::geom::Point2& from, const bcad::geom::Point2& to) {
    double dE = to.x_ - from.x_;
    double dN = to.y_ - from.y_;
    return {std::hypot(dE, dN), normalizeAzimuth(rad2deg(std::atan2(dE, dN)))};
}

enum class Quadrant { NE, SE, SW, NW };

// Gisement quadrant (ex: N 45°30' E) -> azimut décimal.
inline double bearingToAzimuth(Quadrant q, double deg, double min = 0, double sec = 0) {
    double b = deg + min / 60.0 + sec / 3600.0;
    switch (q) {
        case Quadrant::NE: return b;
        case Quadrant::SE: return 180.0 - b;
        case Quadrant::SW: return 180.0 + b;
        case Quadrant::NW: return 360.0 - b;
    }
    return b;
}

// Parse "N45 30 00E" / "S15 02W" -> azimut. Retourne nullopt si invalide.
inline std::optional<double> parseBearing(const std::string& s) {
    if (s.size() < 3) return std::nullopt;
    char ns = s.front(), ew = s.back();
    if ((ns != 'N' && ns != 'S') || (ew != 'E' && ew != 'W')) return std::nullopt;
    std::string mid = s.substr(1, s.size() - 2);
    for (char& c : mid) if (c == ',' || c == ';') c = ' ';
    std::istringstream ss(mid);
    double d = 0, m = 0, sec = 0;
    if (!(ss >> d)) return std::nullopt;
    ss >> m >> sec;
    if (d < 0 || d > 90 || m < 0 || m >= 60 || sec < 0 || sec >= 60) return std::nullopt;
    Quadrant q = (ns == 'N') ? (ew == 'E' ? Quadrant::NE : Quadrant::NW)
                             : (ew == 'E' ? Quadrant::SE : Quadrant::SW);
    return bearingToAzimuth(q, d, m, sec);
}

} // namespace bcad::cadastre
