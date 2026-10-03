#pragma once

#include "bcad/geometry/Point.h"
#include <string>

namespace bcad::layout {

// Étiquette de la feuille : un texte posé en unités monde. L'hôte la peint sans
// savoir ce qu'elle nomme ; le module la construit (ex. cadastre::parcelLabel).
struct Label {
    std::string text;
    bcad::geom::Point2 position;
    double fontSizeMm = 3.0; // taille en mm sur feuille
};

} // namespace bcad::layout
