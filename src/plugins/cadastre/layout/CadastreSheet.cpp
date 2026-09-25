#include "layout/CadastreSheet.h"

#include "ParcelOps.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "entities/ParcelEntity.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>

namespace bcad::cadastre {

namespace {

// Clé de déduplication des bornes : deux sommets au même endroit sont la meme
// borne, pas deux. Un centième de millimetre suffit — les coordonnées du
// parcellaire sont en metres et issues de levés bien plus larges.
std::int64_t borneKey(const geom::Point2& p) {
    constexpr double kGrid = 100000.0;
    return static_cast<std::int64_t>(std::llround(p.x_ * kGrid)) * 1000003LL +
           static_cast<std::int64_t>(std::llround(p.y_ * kGrid));
}

} // namespace

SheetFurniture buildSheetFurniture(const core::Document& document) {
    SheetFurniture furniture;
    std::map<std::int64_t, std::string> bornes;
    int nextBorne = 1;

    for (const auto& entity : document.entities()) {
        if (!isCadastreParcel(entity.get())) continue;
        const auto& parcel = static_cast<const geom::PolylineEntity&>(*entity);
        const auto& properties = parcel.properties();
        const std::string section = properties.getString("cadastre.section");
        const std::string numero = properties.getString("cadastre.numero");
        const std::string contenance = properties.getString("cadastre.contenance");
        const std::string commune = properties.getString("cadastre.commune");
        const auto& vertices = parcel.vertices();
        if (vertices.size() < 3) continue;

        furniture.labels.push_back(
            layout::Label::forParcel(section, numero, contenance, vertices));
        furniture.table.add({section, numero, contenance, commune, parcelArea(parcel)});

        for (const auto& vertex : vertices) {
            const auto [it, inserted] =
                bornes.emplace(borneKey(vertex), std::to_string(nextBorne));
            if (inserted) {
                furniture.bornes.push_back({vertex, it->second});
                ++nextBorne;
            }
        }
    }
    return furniture;
}

} // namespace bcad::cadastre
