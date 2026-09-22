#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Types.h"
#include <cstdint>
#include <vector>

namespace bcad::core {

// Données de sommets prêtes pour le GPU, indépendantes de l'API graphique.
// Le renderer les consomme, mais le type reste dans Core pour éviter une
// dépendance Core -> Render.
struct ColorBatch {
    geom::Color color;
    std::vector<float> vertices;
    std::vector<std::int32_t> firsts;
    std::vector<std::int32_t> counts;
};

struct TessellationResult {
    std::vector<ColorBatch> batches;
    geom::BoundingBox region;
    double toleranceUsed = 0.0;
};

} // namespace bcad::core
