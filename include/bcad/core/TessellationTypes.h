#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Types.h"
#include <cstdint>
#include <vector>

namespace bcad::core {

// Données de sommets prêtes pour le GPU, indépendantes de l'API graphique.
// Le renderer les consomme, mais le type reste dans Core pour éviter une
// dépendance Core -> Render.
//
// Un lot = un style de trait : couleur, épaisseur (mm, celle du calque) et
// type de ligne (valeur de layers::Layer::LineType). `distances` donne, pour
// chaque sommet, la longueur cumulée (unités du dessin) depuis le début de
// son tracé : les tirets restent continus d'un segment à l'autre.
struct ColorBatch {
    geom::Color color;
    float lineWeightMm = 0.25f;
    std::int32_t lineType = 0;
    std::vector<float> vertices;   // x, y par sommet
    std::vector<float> distances;  // une valeur par sommet
    std::vector<std::int32_t> firsts;
    std::vector<std::int32_t> counts;
};

struct TessellationResult {
    std::vector<ColorBatch> batches;
    geom::BoundingBox region;
    double toleranceUsed = 0.0;
};

} // namespace bcad::core
