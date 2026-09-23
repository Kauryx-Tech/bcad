#include "bcad/cadastre/ParcelOps.h"
#include "bcad/geometry/BooleanOps.h"

namespace bcad::cadastre {

std::optional<std::pair<geom::PolylineEntity, geom::PolylineEntity>>
splitParcel(const geom::PolylineEntity& parcel, const geom::PolylineEntity& cutLine) {
    // Stratégie : épaissir la ligne de coupe en polygone fin, faire Difference
    // des deux côtés. Simplifié : coupe par une ligne infinie approximée.
    // Pour l'instant : utilise BooleanOps Difference avec un demi-plan.
    // TODO: coupe exacte par segment — pour l'instant on retourne nullopt si non impl.
    (void)parcel; (void)cutLine;
    return std::nullopt;
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
