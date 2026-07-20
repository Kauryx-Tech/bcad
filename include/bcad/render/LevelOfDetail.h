#pragma once

#include <algorithm>

namespace bcad::render {

// Converts a target on-screen deviation (how far a tessellated chord may
// stray from the true curve, in pixels) into the world-space tolerance that
// Entity::tessellate() expects. As the camera zooms in, pixelsPerUnit grows
// and the world tolerance shrinks, so curves get denser exactly where the
// user can see the difference — and stay coarse (cheap) when zoomed out.
inline double worldToleranceForZoom(double pixelsPerUnit, double targetPixelDeviation = 0.5) {
    double ppu = std::max(pixelsPerUnit, 1e-9);
    return std::clamp(targetPixelDeviation / ppu, 1e-9, 1e6);
}

} // namespace bcad::render
