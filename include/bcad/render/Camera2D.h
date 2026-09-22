#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Point.h"
#include <algorithm>

namespace bcad::render {

struct ScreenPoint {
    double x = 0.0;
    double y = 0.0;
};

// État de panoramique/zoom pour le viewport 2D. L'espace écran suit la
// convention Qt : origine en haut à gauche, y croît vers le bas ; l'espace
// monde suit la convention CAO habituelle avec y vers le haut, donc chaque
// conversion inverse y.
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
        double sx = viewportW_ / 2.0 + (p.x_ - centerX_) * scale_;
        double sy = viewportH_ / 2.0 - (p.y_ - centerY_) * scale_;
        return { sx, sy };
    }

    void panByScreenDelta(double dxPx, double dyPx) {
        centerX_ -= dxPx / scale_;
        centerY_ += dyPx / scale_;
    }

    // Multiplie le zoom par `factor` (>1 rapproche) tout en gardant fixe à
    // l'écran le point monde actuellement sous `pivotPx` — le comportement
    // standard de "zoom vers le curseur" attendu de tout viewport CAO/DCC.
    void zoomAt(double factor, ScreenPoint pivotPx) {
        geom::Point2 worldBefore = screenToWorld(pivotPx);
        scale_ = std::clamp(scale_ * factor, kMinScale, kMaxScale);
        centerX_ = worldBefore.x_ - (pivotPx.x - viewportW_ / 2.0) / scale_;
        centerY_ = worldBefore.y_ + (pivotPx.y - viewportH_ / 2.0) / scale_;
    }

    void zoomToFit(const geom::BoundingBox& worldBounds, double marginRatio = 0.1) {
        if (!worldBounds.isValid() || viewportW_ <= 0 || viewportH_ <= 0) return;
        double w = std::max(worldBounds.width(), 1e-6);
        double h = std::max(worldBounds.height(), 1e-6);
        double sx = viewportW_ / w;
        double sy = viewportH_ / h;
        scale_ = std::clamp(std::min(sx, sy) * (1.0 - marginRatio), kMinScale, kMaxScale);
        geom::Point2 c = worldBounds.center();
        centerX_ = c.x_;
        centerY_ = c.y_;
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
    double scale_ = 1.0; // pixels par unité monde
    int viewportW_ = 800;
    int viewportH_ = 600;
};

} // namespace bcad::render
