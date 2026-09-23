#pragma once

#include "bcad/layout/Sheet.h"
#include "bcad/geometry/BoundingBox.h"

namespace bcad::layout {

// Fenêtre du dessin dans la feuille (G4).
// Définit quelle zone du dessin est visible et à quelle échelle.
class Viewport {
public:
    Viewport() = default;

    // Zone du dessin à afficher (en unités monde)
    void setSource(const bcad::geom::BoundingBox& bbox) { source_ = bbox; }
    const bcad::geom::BoundingBox& source() const { return source_; }

    // Échelle : 1:n (ex: 500 → 1:500)
    void setScale(double s) { scale_ = s; }
    double scale() const { return scale_; }

    // Position sur la feuille (mm, coin sup-gauche de la zone imprimable)
    void setPosition(double x, double y) { x_ = x; y_ = y; }
    double x() const { return x_; }
    double y() const { return y_; }

    // Taille sur la feuille (mm) — calculée depuis source + échelle
    double widthOnSheet() const {
        return source_.width() * 1000.0 / scale_;
    }
    double heightOnSheet() const {
        return source_.height() * 1000.0 / scale_;
    }

    bool fitsIn(const Sheet& sheet) const {
        return widthOnSheet() <= sheet.printableWidth() &&
               heightOnSheet() <= sheet.printableHeight();
    }

    // Échelle auto pour que source rentre dans la feuille
    double autoScale(const Sheet& sheet) const {
        double w = source_.width();
        double h = source_.height();
        if (w < 1e-9 || h < 1e-9) return 500;
        double sx = w * 1000.0 / sheet.printableWidth();
        double sy = h * 1000.0 / sheet.printableHeight();
        double s = std::max(sx, sy);
        const double std_scales[] = {100, 200, 250, 500, 1000, 2000, 5000};
        for (double std_s : std_scales) if (s <= std_s) return std_s;
        return std_scales[6];
    }

private:
    bcad::geom::BoundingBox source_;
    double scale_ = 500;
    double x_ = 0, y_ = 0;
};

} // namespace bcad::layout
