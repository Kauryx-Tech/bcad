#include "ParcelOps.h"
#include <cassert>
#include <cmath>
#include <string>

using namespace bcad::cadastre;
using namespace bcad::geom;

int main() {
    // Carré 10x10
    PolylineEntity parcel({{0, 0}, {10, 0}, {10, 10}, {0, 10}}, true);
    assert(std::abs(parcelArea(parcel) - 100.0) < 1e-9);

    // --- splitParcel : coupe verticale en x=5 -> deux moitiés de 50 ---
    {
        PolylineEntity cut({{5, -1}, {5, 11}}, false);
        auto parts = splitParcel(parcel, cut);
        assert(parts.has_value());
        assert(std::abs(parcelArea(parts->first) - 50.0) < 1e-6);
        assert(std::abs(parcelArea(parts->second) - 50.0) < 1e-6);
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

    // --- subdivideParcel : 4 lots de 25 via direction verticale ---
    {
        PolylineEntity dirVert({{5, -1}, {5, 11}}, false);
        auto lots = subdivideParcel(parcel, 4, dirVert);
        assert(lots.has_value());
        assert(lots->size() == 4);
        for (const auto& lot : *lots)
            assert(std::abs(parcelArea(lot) - 25.0) < 1e-6);
    }

    // --- subdivideParcel : 2 lots de 50 via direction horizontale ---
    {
        PolylineEntity dirHoriz({{-1, 5}, {11, 5}}, false);
        auto lots = subdivideParcel(parcel, 2, dirHoriz);
        assert(lots.has_value());
        assert(lots->size() == 2);
        assert(std::abs(parcelArea(lots->at(0)) - 50.0) < 1e-6);
        assert(std::abs(parcelArea(lots->at(1)) - 50.0) < 1e-6);
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
