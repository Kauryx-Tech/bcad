#pragma once

#include "bcad/geometry/Types.h"
#include <string>

namespace bcad::layers {

struct Layer {
    std::string name = "0";
    geom::Color color = geom::Color::fromRgb255(255, 255, 255);
    double lineWeight = 0.25; // mm, DXF-style convention
    bool visible = true;
    bool locked = false;

    enum class LineType { Continuous, Dashed, Dotted, DashDot };
    LineType lineType = LineType::Continuous;
};

} // namespace bcad::layers
