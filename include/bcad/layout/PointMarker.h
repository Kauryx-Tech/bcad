#pragma once

#include "bcad/geometry/Point.h"

#include <string>

namespace bcad::layout {

// Repère ponctuel de la feuille : un rond et son libellé, posés en unités monde.
// L'hôte le peint sans savoir ce qu'il désigne — une borne, un regard, une
// station — : c'est le module qui le construit et le nomme (ADR-016/017).
struct PointMarker {
    bcad::geom::Point2 position;
    std::string text;
    double radiusMm = 1.5;
};

} // namespace bcad::layout
