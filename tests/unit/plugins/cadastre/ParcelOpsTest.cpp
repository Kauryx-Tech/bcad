#include "ParcelOps.h"
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

using namespace bcad::cadastre;
using namespace bcad::geom;

namespace {

// Une coupe est materialisee par un tampon de 1e-3 (largeur de trait) traverse
// sur 10 m : elle retire 0,01 m2 a la parcelle. Les seuils en decoulent, ce
// n'est pas de la tolerance de complaisance.
constexpr double kHalfCutLoss = 5e-3;
constexpr double kCutLoss = 1e-2;

double totalArea(const std::vector<PolylineEntity>& parcels) {
    double total = 0.0;
    for (const auto& parcel : parcels) total += parcelArea(parcel);
    return total;
}

} // namespace

int main() {
    // Carré 10x10
    PolylineEntity parcel({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true);
    assert(std::abs(parcelArea(parcel) - 100.0) < 1e-9);

    // --- splitParcel : coupe verticale en x=5 -> deux moitiés de 50 ---
    {
        PolylineEntity cut({{5, -1}, {5, 11}}, false);
        auto parts = splitParcel(parcel, cut);
        assert(parts.has_value());
        assert(std::abs(parcelArea(parts->first) - 50.0) < kHalfCutLoss);
        assert(std::abs(parcelArea(parts->second) - 50.0) < kHalfCutLoss);
    }

    // --- splitParcel : coupe hors parcelle -> échec ---
    {
        PolylineEntity outside({{20, -1}, {20, 11}}, false);
        assert(!splitParcel(parcel, outside).has_value());
    }

    // --- splitParcel : ligne dégénérée -> échec ---
    {
        PolylineEntity degenerate({{5, 0}}, false);
        assert(!splitParcel(parcel, degenerate).has_value());
    }

    // --- subdivideParcel : 4 lots egaux via direction verticale ---
    {
        PolylineEntity dirVert({{5, -1}, {5, 11}}, false);
        auto lots = subdivideParcel(parcel, 4, dirVert);
        assert(lots.has_value());
        assert(lots->size() == 4);
        for (const auto& lot : *lots)
            assert(std::abs(parcelArea(lot) - 25.0) < kCutLoss);
        // Trois coupes retirent chacune 0,01 m2 de trait de coupe.
        assert(std::abs(totalArea(*lots) - (100.0 - 3 * kCutLoss)) < 1e-6);
    }

    // --- subdivideParcel : 2 lots de 50 via direction horizontale ---
    {
        PolylineEntity dirHoriz({{-1, 5}, {11, 5}}, false);
        auto lots = subdivideParcel(parcel, 2, dirHoriz);
        assert(lots.has_value());
        assert(lots->size() == 2);
        assert(std::abs(parcelArea(lots->at(0)) - 50.0) < kHalfCutLoss);
        assert(std::abs(parcelArea(lots->at(1)) - 50.0) < kHalfCutLoss);
    }

    // --- subdivideParcel : les lots suivent l'ordre geometrique, pas la taille ---
    // Regression : les moities etaient designees par aire decroissante, la 2e
    // coupe tombait donc hors de la partie restante des 3 lots.
    {
        PolylineEntity dirVert({{0, -1}, {0, 11}}, false);
        auto lots = subdivideParcel(parcel, 3, dirVert);
        assert(lots.has_value());
        assert(lots->size() == 3);
        std::vector<double> centroidX;
        for (const auto& lot : *lots) {
            double sum = 0.0;
            for (const auto& vertex : lot.vertices()) sum += vertex.x_;
            centroidX.push_back(sum / static_cast<double>(lot.vertices().size()));
        }
        // Le sens d'emission depend de l'orientation de la ligne de direction :
        // ce qui compte est que les lots soient alignes sans se retoroner.
        const bool increasing = centroidX[0] < centroidX[1] && centroidX[1] < centroidX[2];
        const bool decreasing = centroidX[0] > centroidX[1] && centroidX[1] > centroidX[2];
        assert(increasing || decreasing);
    }

    // --- subdivideParcel : n invalide ou direction dégénérée ---
    {
        PolylineEntity dirVert({{5, -1}, {5, 11}}, false);
        assert(!subdivideParcel(parcel, 1, dirVert).has_value());
        PolylineEntity badDir({{5, -1}}, false);
        assert(!subdivideParcel(parcel, 3, badDir).has_value());
    }

    // --- mergeParcels : deux rectangles adjacents -> 100 ---
    {
        PolylineEntity a({{0, 0}, {5, 0}, {5, 10}, {0, 10}}, true);
        PolylineEntity b({{5, 0}, {10, 0}, {10, 10}, {5, 10}}, true);
        auto merged = mergeParcels(a, b);
        assert(merged.has_value());
        assert(std::abs(parcelArea(*merged) - 100.0) < 1e-6);
    }

    // --- formatContenance : m² et hectares ---
    {
        assert(formatContenance(50.0) == "50 m\u00b2");
        assert(formatContenance(12345.0) == "1.23 ha");
    }

    return 0;
}
