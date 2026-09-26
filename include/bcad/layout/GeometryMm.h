#pragma once

// Le rectangle de papier, en millimetres. Separe de `Composition.h` parce que
// c'est une DONNEE du document — la place d'une vue ou d'un meuble sur la
// feuille — et non un resultat de calcul de mise en page. Publie ici, ce type
// n'oblige personne a tirer la composition, l'echelle ou le cartouche avec lui.

namespace bcad::layout {

struct RectMm {
    double x = 0, y = 0, w = 0, h = 0;

    double left() const { return x; }
    double top() const { return y; }
    double right() const { return x + w; }
    double bottom() const { return y + h; }
    bool isValid() const { return w > 0.0 && h > 0.0; }
    bool contains(const RectMm& other, double tolerance = 1e-6) const {
        return other.left() >= left() - tolerance && other.top() >= top() - tolerance &&
               other.right() <= right() + tolerance && other.bottom() <= bottom() + tolerance;
    }
};

} // namespace bcad::layout
