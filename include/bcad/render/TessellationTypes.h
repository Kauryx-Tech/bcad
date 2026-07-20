#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Types.h"
#include <cstdint>
#include <vector>

namespace bcad::render {

// GPU-ready, GL-agnostic vertex data for every entity sharing one resolved
// color. One draw call per batch (glMultiDrawArrays over `firsts`/`counts`)
// instead of one per entity, which is what makes large drawings survive.
struct ColorBatch {
    geom::Color color;
    std::vector<float> vertices; // interleaved x,y, all polylines concatenated
    std::vector<std::int32_t> firsts; // first vertex index of each polyline
    std::vector<std::int32_t> counts; // vertex count of each polyline
};

struct TessellationResult {
    std::vector<ColorBatch> batches;
    geom::BoundingBox region;
    double toleranceUsed = 0.0;
};

} // namespace bcad::render
