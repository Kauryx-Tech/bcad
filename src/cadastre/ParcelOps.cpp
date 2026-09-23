#include "bcad/cadastre/ParcelOps.h"
#include "bcad/geometry/BooleanOps.h"
#include <cmath>
#include <vector>
#include <algorithm>

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

std::optional<std::pair<geom::PolylineEntity, geom::PolylineEntity>>
splitParcel(const geom::PolylineEntity& parcel, const geom::PolylineEntity& cutLine) {
    return splitParcelByLine(parcel, cutLine);
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

} // namespace bcad::cadastre