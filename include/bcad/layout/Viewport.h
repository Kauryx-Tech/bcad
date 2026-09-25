#pragma once

#include "bcad/layout/Sheet.h"
#include "bcad/layout/Scale.h"
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

    // Échelle : 1:n (ex: 500 → 1:500). 0 = pas encore choisie, la composition
    // la déduit alors de l'échelle standard qui tient sur la feuille.
    void setScale(double s) { scale_ = s; }
    double scale() const { return scale_; }

    // Taille sur la feuille (mm) — calculée depuis source + échelle, nulle sans
    // échelle : rien n'est mesurable avant qu'elle soit fixée.
    double widthOnSheet() const {
        return scale_ <= 0 ? 0.0 : source_.width() * 1000.0 / scale_;
    }
    double heightOnSheet() const {
        return scale_ <= 0 ? 0.0 : source_.height() * 1000.0 / scale_;
    }

    bool fitsIn(const Sheet& sheet) const {
        return scale_ > 0 &&
               widthOnSheet() <= sheet.printableWidth() &&
               heightOnSheet() <= sheet.printableHeight();
    }

private:
    bcad::geom::BoundingBox source_;
    double scale_ = 0;
};

} // namespace bcad::layout
