#pragma once

#include "bcad/core/Document.h"
#include <optional>

namespace bcad::app {

// Grid is handled separately by Viewport (it's not document geometry), but
// lives in the same enum so the snap indicator can draw it like any other
// snap result.
enum class SnapType { None, Endpoint, Midpoint, Center, Intersection, Quadrant, Perpendicular, Nearest, Grid };

struct SnapResult {
    SnapType type = SnapType::None;
    geom::Point2 point{ 0, 0 };

    explicit operator bool() const { return type != SnapType::None; }
};

// Object snapping: finds the geometrically "interesting" point nearest the
// cursor (endpoint, midpoint, center, intersection) within a world-space
// tolerance, plus perpendicular-from-reference when a reference point (the
// tool's last placed point) is supplied. Endpoint/Center/Intersection are
// one priority tier above Midpoint — matches the usual CAD feel where
// "hard" points are stickier than a midpoint even when it's a hair closer.
// Perpendicular only activates when nothing else qualifies, since it needs
// an explicit reference point to be meaningful.
class SnapEngine {
public:
    SnapResult findSnap(const core::Document& doc, const geom::Point2& cursor, double worldTolerance,
                         std::optional<geom::Point2> referencePoint = std::nullopt) const;
};

} // namespace bcad::app
