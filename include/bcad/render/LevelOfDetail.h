#pragma once

#include <algorithm>

namespace bcad::render {

// Convertit un écart-cible à l'écran (de combien une corde tessellée peut
// s'éloigner de la courbe réelle, en pixels) en la tolérance en espace monde
// attendue par Entity::tessellate(). Quand la caméra zoome, pixelsPerUnit
// augmente et la tolérance monde diminue, si bien que les courbes deviennent
// plus denses précisément là où l'utilisateur peut voir la différence — et
// restent grossières (donc peu coûteuses) en dézoomant.
inline double worldToleranceForZoom(double pixelsPerUnit, double targetPixelDeviation = 0.5) {
    double ppu = std::max(pixelsPerUnit, 1e-9);
    return std::clamp(targetPixelDeviation / ppu, 1e-9, 1e6);
}

} // namespace bcad::render
