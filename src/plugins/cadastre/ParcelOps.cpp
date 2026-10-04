#include "ParcelOps.h"
#include "bcad/geometry/BooleanOps.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <limits>

namespace bcad::cadastre {

namespace {

// Crée un polygone tampon (buffer) autour d'un segment de ligne pour faire une coupe
std::vector<geom::PolylineEntity> bufferLine(const geom::Point2& a, const geom::Point2& b, double width) {
    double dx = b.x_ - a.x_;
    double dy = b.y_ - a.y_;
    double len = std::hypot(dx, dy);
    if (len < 1e-9) return {};
    
    // Vecteur normal unitaire
    double nx = -dy / len;
    double ny = dx / len;
    double half = width * 0.5;
    
    // Rectangle autour du segment
    geom::Point2 p1{a.x_ - nx * half, a.y_ - ny * half};
    geom::Point2 p2{b.x_ - nx * half, b.y_ - ny * half};
    geom::Point2 p3{b.x_ + nx * half, b.y_ + ny * half};
    geom::Point2 p4{a.x_ + nx * half, a.y_ + ny * half};
    
    geom::PolylineEntity rect({p1, p2, p3, p4}, true);
    return {rect};
}

// Coupe une parcelle par une ligne de coupe (polyline ouverte ou fermée)
// Retourne les deux parties résultantes si la coupe traverse la parcelle
std::optional<std::pair<geom::PolylineEntity, geom::PolylineEntity>>
splitParcelByLine(const geom::PolylineEntity& parcel, const geom::PolylineEntity& cutLine) {
    // La cutLine doit avoir au moins 2 points
    if (cutLine.vertices().size() < 2) return std::nullopt;
    
    // Créer un buffer fin autour de la ligne de coupe
    // Pour simplifier : on prend chaque segment de la cutLine et on crée un buffer
    // puis on fait l'union de tous les buffers
    std::vector<geom::PolylineEntity> buffers;
    const auto& cv = cutLine.vertices();
    const double bufferWidth = 1e-3; // largeur très fine pour la coupe
    
    for (size_t i = 0; i + 1 < cv.size(); ++i) {
        auto buf = bufferLine(cv[i], cv[i + 1], bufferWidth);
        buffers.insert(buffers.end(), buf.begin(), buf.end());
    }
    
    if (buffers.empty()) return std::nullopt;
    
    // Union de tous les buffers pour former la zone de coupe
    geom::PolylineEntity cutZone = buffers.front();
    for (size_t i = 1; i < buffers.size(); ++i) {
        auto uni = geom::booleanOp(cutZone, buffers[i], geom::BooleanOp::Union);
        if (uni.empty()) return std::nullopt;
        cutZone = uni.front();
    }
    
    // Vérifier que la zone de coupe intersecte la parcelle
    auto inter = geom::booleanOp(parcel, cutZone, geom::BooleanOp::Intersection);
    if (inter.empty()) return std::nullopt;
    
    // Difference : parcelle - zone de coupe = deux parties
    auto diff = geom::booleanOp(parcel, cutZone, geom::BooleanOp::Difference);
    if (diff.size() != 2) return std::nullopt;
    
    // Trier les deux résultats pour avoir un ordre déterministe (par aire)
    auto a = diff[0];
    auto b = diff[1];
    if (parcelArea(a) < parcelArea(b)) std::swap(a, b);
    
    return std::make_pair(std::move(a), std::move(b));
}

} // namespace

// --- Fonctions publiques (dans le namespace) ---

std::optional<std::pair<geom::PolylineEntity, geom::PolylineEntity>>
splitParcel(const geom::PolylineEntity& parcel, const geom::PolylineEntity& cutLine) {
    return splitParcelByLine(parcel, cutLine);
}

std::optional<std::vector<geom::PolylineEntity>>
subdivideParcel(const geom::PolylineEntity& parcel, int n,
                const geom::PolylineEntity& directionLine) {
    if (n < 2) return std::nullopt;
    
    // La directionLine doit avoir au moins 2 points (segment)
    const auto& dv = directionLine.vertices();
    if (dv.size() < 2) return std::nullopt;
    
    // Direction perpendiculaire à la ligne de direction (pour des coupes perpendiculaires)
    geom::Point2 d{dv[1].x_ - dv[0].x_, dv[1].y_ - dv[0].y_};
    double len = std::hypot(d.x_, d.y_);
    if (len < 1e-9) return std::nullopt;
    
    // Normale unitaire (perpendiculaire)
    double nx = -d.y_ / len;
    double ny = d.x_ / len;
    
    // Bounding box de la parcelle
    auto bbox = parcel.boundingBox();
    if (!bbox.isValid()) return std::nullopt;
    
    // Projeter la bbox sur la normale pour trouver l'étendue
    double minProj = std::numeric_limits<double>::infinity();
    double maxProj = -std::numeric_limits<double>::infinity();
    
    for (const auto& v : parcel.vertices()) {
        double proj = v.x_ * nx + v.y_ * ny;
        minProj = std::min(minProj, proj);
        maxProj = std::max(maxProj, proj);
    }
    
    double totalLength = maxProj - minProj;
    if (totalLength < 1e-9) return std::nullopt;
    
    double step = totalLength / n;
    std::vector<geom::PolylineEntity> result;
    result.reserve(n);
    
    geom::PolylineEntity currentParcel = parcel;
    
    for (int i = 0; i < n - 1; ++i) {
        double cutPos = minProj + (i + 1) * step;
        
        // Créer une ligne de coupe perpendiculaire passant par cutPos sur la normale
        // Points sur la ligne : on prend un point sur la ligne à cutPos et on étend perpendiculairement
        // Direction de la ligne de coupe = direction parallèle à directionLine (d)
        // Point sur la ligne : point de la bbox + (cutPos - minProj) * normale
        geom::Point2 p0{minProj * nx, minProj * ny}; // origine arbitraire sur la ligne de référence
        geom::Point2 cutPoint{cutPos * nx, cutPos * ny};
        
        // Ligne de coupe : perpendiculaire à la normale, donc parallèle à directionLine
        // Passe par cutPoint
        geom::Point2 dir{d.x_, d.y_};
        double cutLen = (parcel.boundingBox().width() + parcel.boundingBox().height()) * 2;
        geom::Point2 p1{cutPoint.x_ - dir.x_ * cutLen, cutPoint.y_ - dir.y_ * cutLen};
        geom::Point2 p2{cutPoint.x_ + dir.x_ * cutLen, cutPoint.y_ + dir.y_ * cutLen};
        
        geom::PolylineEntity cutLine({p1, p2}, false);
        
        auto split = splitParcelByLine(currentParcel, cutLine);
        if (!split.has_value()) return std::nullopt;
        
        // splitParcelByLine trie ses deux moities par aire decroissante, pas par
        // cote : il faut designer le lot d'apres la projection du centroide, sinon
        // a partir de 3 lots la coupe suivante tombe hors de la partie restante.
        auto projectedCentroid = [&](const geom::PolylineEntity& polygon) {
            if (polygon.vertices().empty()) return 0.0;
            double sum = 0.0;
            for (const auto& v : polygon.vertices())
                sum += v.x_ * nx + v.y_ * ny;
            return sum / static_cast<double>(polygon.vertices().size());
        };
        const bool firstIsLowSide = projectedCentroid(split->first) < cutPos;

        // Le lot emis est la partie cote minProj, on poursuit avec l'autre.
        geom::PolylineEntity lot = firstIsLowSide ? split->first : split->second;
        currentParcel = firstIsLowSide ? split->second : split->first;
        
        // Attribuer les propriétés du lot original (section, numero, etc.)
        // Pour l'instant on garde les mêmes, un vrai système ferait l'attribution
        
        result.push_back(std::move(lot));
    }
    
    // Le dernier morceau
    if (currentParcel.vertices().size() >= 3) {
        result.push_back(std::move(currentParcel));
    }
    
    if (result.size() != static_cast<size_t>(n)) return std::nullopt;
    
    return result;
}

std::optional<geom::PolylineEntity>
mergeParcels(const geom::PolylineEntity& a, const geom::PolylineEntity& b) {
    auto result = geom::booleanOp(a, b, geom::BooleanOp::Union);
    if (result.empty()) return std::nullopt;
    return result.front();
}

double parcelArea(const geom::PolylineEntity& parcel) {
    return std::abs(geom::polygonArea(parcel));
}

std::string formatContenance(double areaM2) {
    if (areaM2 >= 10000) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f ha", areaM2 / 10000.0);
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f m²", areaM2);
    return buf;
}

std::string formatSquareMetres(double areaM2) {
    char raw[64];
    snprintf(raw, sizeof(raw), "%.2f", areaM2);
    std::string number(raw);
    const auto dot = number.find('.');
    std::string integer = number.substr(0, dot);
    const std::string decimals = number.substr(dot + 1);
    // Espace des milliers (espace insecable fine non utilisee : police du plan).
    for (int pos = static_cast<int>(integer.size()) - 3; pos > 0; pos -= 3) {
        if (integer[static_cast<size_t>(pos) - 1] == '-') break;
        integer.insert(static_cast<size_t>(pos), " ");
    }
    return integer + "," + decimals + " m²";
}

namespace {

// Nombre a la francaise ou a l'anglaise : virgule ou point decimal, espaces
// de milliers ignores.
std::optional<double> readNumber(std::string_view token) {
    std::string clean;
    for (char c : token) {
        if (c == ' ') continue;
        clean += (c == ',') ? '.' : c;
    }
    if (clean.empty()) return std::nullopt;
    char* end = nullptr;
    const double value = std::strtod(clean.c_str(), &end);
    if (end != clean.c_str() + clean.size() || !std::isfinite(value) || value < 0) return std::nullopt;
    return value;
}

} // namespace

std::optional<double> parseContenance(std::string_view text) {
    // Decoupage en paires (nombre, unite) : « 2 ha 3 a 50 ca », « 1 250,50 m² ».
    std::string lowered;
    for (char c : text) lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    // Normaliser les ecritures de m² : « m² », « m2 ».
    for (const std::string sq : {"m²", "m2"}) {
        for (size_t at = lowered.find(sq); at != std::string::npos; at = lowered.find(sq, at + 1))
            lowered.replace(at, sq.size(), " m ");
    }
    double total = 0.0;
    bool any = false;
    std::string pendingNumber;
    auto flush = [&](double factor) -> bool {
        const auto value = readNumber(pendingNumber);
        if (!value) return false;
        total += *value * factor;
        any = true;
        pendingNumber.clear();
        return true;
    };
    size_t i = 0;
    while (i < lowered.size()) {
        const char c = lowered[i];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == ',' || c == '.' || c == ' ') {
            pendingNumber += c;
            ++i;
            continue;
        }
        if (!std::isalpha(static_cast<unsigned char>(c))) return std::nullopt;
        std::string unit;
        while (i < lowered.size() && std::isalpha(static_cast<unsigned char>(lowered[i]))) unit += lowered[i++];
        double factor = 0.0;
        if (unit == "ha") factor = 10000.0;
        else if (unit == "a") factor = 100.0;
        else if (unit == "ca" || unit == "m") factor = 1.0;
        else return std::nullopt;
        if (!flush(factor)) return std::nullopt;
    }
    // Un nombre seul, sans unite : des m².
    if (pendingNumber.find_first_not_of(' ') != std::string::npos) {
        if (!flush(1.0)) return std::nullopt;
    }
    if (!any) return std::nullopt;
    return total;
}

} // namespace bcad::cadastre