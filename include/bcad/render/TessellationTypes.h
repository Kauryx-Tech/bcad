#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Types.h"
#include <cstdint>
#include <vector>

namespace bcad::render {

// Données de sommets prêtes pour le GPU, indépendantes de l'API graphique,
// pour chaque entité partageant une même couleur résolue. Un appel de dessin
// par lot (glMultiDrawArrays sur `firsts`/`counts`) plutôt qu'un par entité,
// ce qui est précisément ce qui permet aux grands dessins de rester fluides.
struct ColorBatch {
    geom::Color color;
    std::vector<float> vertices; // x,y entrelacés, toutes les polylignes concaténées
    std::vector<std::int32_t> firsts; // index du premier sommet de chaque polyligne
    std::vector<std::int32_t> counts; // nombre de sommets de chaque polyligne
};

struct TessellationResult {
    std::vector<ColorBatch> batches;
    geom::BoundingBox region;
    double toleranceUsed = 0.0;
};

} // namespace bcad::render
