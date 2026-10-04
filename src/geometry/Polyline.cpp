#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/BooleanOps.h"
#include <cmath>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::geom {
namespace {

// Decoupe en conservant les champs vides : un champ vide n'est pas une absence
// de donnee, c'est une donnee mal formee, et elle doit etre refusee.
std::vector<std::string_view> splitOn(std::string_view text, char separator) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t at = text.find(separator, start);
        if (at == std::string_view::npos) {
            out.push_back(text.substr(start));
            return out;
        }
        out.push_back(text.substr(start, at - start));
        start = at + 1;
    }
}

// Un nombre, et rien d'autre. « 10;3 » doit echouer ici plutot que de rendre 10
// : lire un prefixe et jeter le reste est exactement la facon dont une grammaire
// en devore une autre en silence.
bool parseNumber(std::string_view text, double& value) {
    if (text.empty()) return false;
    std::size_t consumed = 0;
    double parsed = 0.0;
    try {
        parsed = std::stod(std::string(text), &consumed);
    } catch (const std::exception&) {
        return false;
    }
    if (consumed != text.size()) return false;
    if (std::isnan(parsed) || std::isinf(parsed)) return false;
    value = parsed;
    return true;
}

// Une suite complete de paires a partir d'un rang donne.
bool parsePairs(const std::vector<std::string_view>& fields,
                std::size_t from,
                std::vector<Point2>& out) {
    if (fields.size() < from || (fields.size() - from) % 2 != 0) return false;
    for (std::size_t i = from; i + 1 < fields.size(); i += 2) {
        double x = 0.0;
        double y = 0.0;
        if (!parseNumber(fields[i], x) || !parseNumber(fields[i + 1], y)) return false;
        out.emplace_back(x, y);
    }
    return true;
}

} // namespace

std::string PolylineEntity::encodeRings(const Rings& rings) {
    std::ostringstream ss;
    ss.precision(17);
    ss << (rings.closed ? 1 : 0);
    for (const auto& v : rings.outer) ss << ',' << v.x_ << ',' << v.y_;
    for (const auto& hole : rings.holes) {
        if (hole.empty()) continue;
        ss << ';';
        for (std::size_t i = 0; i < hole.size(); ++i) {
            if (i != 0) ss << ',';
            ss << hole[i].x_ << ',' << hole[i].y_;
        }
    }
    return ss.str();
}

bool PolylineEntity::decodeRings(std::string_view payload, Rings& out) {
    Rings parsed;
    const std::vector<std::string_view> sections = splitOn(payload, ';');

    const std::vector<std::string_view> outerFields = splitOn(sections.front(), ',');
    double flag = 0.0;
    if (outerFields.empty() || !parseNumber(outerFields.front(), flag)) return false;
    parsed.closed = flag != 0.0;
    if (!parsePairs(outerFields, 1, parsed.outer)) return false;

    for (std::size_t s = 1; s < sections.size(); ++s) {
        std::vector<Point2> hole;
        if (!parsePairs(splitOn(sections[s], ','), 0, hole)) return false;
        if (hole.empty()) return false;
        parsed.holes.push_back(std::move(hole));
    }

    out = std::move(parsed);
    return true;
}

std::string PolylineEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << "Polyligne (" << (closed_ ? "fermée" : "ouverte") << ")\nSommets : " << vertices_.size()
       << "\nLongueur : " << length();
    if (closed_) ss << "\nSurface : " << std::abs(polygonArea(*this));
    return ss.str();
}

void PolylineEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    auto midpoint = [](const Point2& a, const Point2& b) {
        return Point2((a.x_ + b.x_) / 2.0, (a.y_ + b.y_) / 2.0);
    };
    std::size_t n = vertices_.size();
    for (std::size_t i = 0; i < n; ++i) add(vertices_[i], SnapPointType::Endpoint);
    std::size_t segCount = closed_ ? n : (n == 0 ? 0 : n - 1);
    for (std::size_t i = 0; i < segCount; ++i) {
        add(midpoint(vertices_[i], vertices_[(i + 1) % n]), SnapPointType::Midpoint);
    }
}

} // namespace bcad::geom