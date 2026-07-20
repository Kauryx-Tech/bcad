#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Types.h"
#include <algorithm>

namespace bcad::render {

struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
};

// Pan/zoom state for the 2D viewport. Screen space follows Qt convention:
// origin top-left, y grows downward; world space is the usual y-up CAD
// convention, so every conversion flips y.
class Camera2D {
public:
    void setViewportSize(int widthPx, int heightPx) {
        viewportW_ = widthPx;
        viewportH_ = heightPx;
    }

    geom::Point2 screenToWorld(ScreenPoint s) const {
        double wx = centerX_ + (s.x - viewportW_ / 2.0) / scale_;
        double wy = centerY_ - (s.y - viewportH_ / 2.0) / scale_;
        return { wx, wy };
    }

    ScreenPoint worldToScreen(const geom::Point2& p) const {
        double sx = viewportW_ / 2.0 + (CGAL::to_double(p.x()) - centerX_) * scale_;
        double sy = viewportH_ / 2.0 - (CGAL::to_double(p.y()) - centerY_) * scale_;
        return { sx, sy };
    }

    void panByScreenDelta(double dxPx, double dyPx) {
        centerX_ -= dxPx / scale_;
        centerY_ += dyPx / scale_;
    }

    // Multiply zoom by `factor` (>1 zooms in) while keeping the world point
    // currently under `pivotPx` fixed on screen — the standard "zoom to
    // cursor" behavior expected of any CAD/DCC viewport.
    void zoomAt(double factor, ScreenPoint pivotPx) {
        geom::Point2 worldBefore = screenToWorld(pivotPx);
        scale_ = std::clamp(scale_ * factor, kMinScale, kMaxScale);
        centerX_ = CGAL::to_double(worldBefore.x()) - (pivotPx.x - viewportW_ / 2.0) / scale_;
        centerY_ = CGAL::to_double(worldBefore.y()) + (pivotPx.y - viewportH_ / 2.0) / scale_;
    }

    void zoomToFit(const geom::BoundingBox& worldBounds, double marginRatio = 0.1) {
        if (!worldBounds.isValid() || viewportW_ <= 0 || viewportH_ <= 0) return;
        double w = std::max(worldBounds.width(), 1e-6);
        double h = std::max(worldBounds.height(), 1e-6);
        double sx = viewportW_ / w;
        double sy = viewportH_ / h;
        scale_ = std::clamp(std::min(sx, sy) * (1.0 - marginRatio), kMinScale, kMaxScale);
        geom::Point2 c = worldBounds.center();
        centerX_ = CGAL::to_double(c.x());
        centerY_ = CGAL::to_double(c.y());
    }

    geom::BoundingBox visibleWorldRegion() const {
        geom::BoundingBox bb;
        bb.expand(screenToWorld({0, 0}));
        bb.expand(screenToWorld({ static_cast<double>(viewportW_), static_cast<double>(viewportH_) }));
        return bb;
    }

    double pixelsPerUnit() const { return scale_; }
    double worldUnitsPerPixel() const { return 1.0 / scale_; }
    int viewportWidth() const { return viewportW_; }
    int viewportHeight() const { return viewportH_; }

private:
    static constexpr double kMinScale = 1e-4;
    static constexpr double kMaxScale = 1e6;

    double centerX_ = 0.0;
    double centerY_ = 0.0;
    double scale_ = 1.0; // pixels per world unit
    int viewportW_ = 800;
    int viewportH_ = 600;
};

} // namespace bcad::render
