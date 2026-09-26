#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/layout/GeometryMm.h"

namespace bcad::layout {

class Sheet;

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

    // Place sur la feuille, en millimètres. Troisième donnée de la vue, ni
    // déduite de la source ni de l'échelle : ADR-017 décision 2. Nulle tant que
    // personne ne l'a posée — c'est alors à la composition de choisir, ce qui
    // reste son droit, mais ne devient plus son devoir.
    const RectMm& paper() const { return paper_; }
    void setPaper(const RectMm& r) { paper_ = r; }
    bool isPlaced() const { return paper_.isValid(); }

    // Taille sur la feuille (mm) — calculée depuis source + échelle, nulle sans
    // échelle : rien n'est mesurable avant qu'elle soit fixée.
    double widthOnSheet() const {
        return scale_ <= 0 ? 0.0 : source_.width() * 1000.0 / scale_;
    }
    double heightOnSheet() const {
        return scale_ <= 0 ? 0.0 : source_.height() * 1000.0 / scale_;
    }

    // Le corps est défini dans Sheet.h : la vue se mesure à la feuille, et la
    // feuille porte des vues.
    bool fitsIn(const Sheet& sheet) const;

private:
    bcad::geom::BoundingBox source_;
    double scale_ = 0;
    RectMm paper_;
};

} // namespace bcad::layout
