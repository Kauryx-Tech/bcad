// Echelle de l'interface, comme AutoCAD : celle de l'affichage. Sous Windows,
// AutoCAD suit la mise a l'echelle de l'ecran (125 %, 150 %…) ; une
// application WSLg est dessinee a 96 ppp et paraitrait plus petite. Le choix
// se fait avant la creation de QApplication (Qt fixe l'echelle au demarrage).

#pragma once

#include <QString>

namespace bcad::app {

// Reglage enregistre : 0 = automatique, sinon un facteur (1.25…).
inline constexpr const char* kUiScaleSetting = "interface/scale";

// Facteur a appliquer : variable QT_SCALE_FACTOR deja posee (respectee
// telle quelle, 0 renvoye) ; reglage de l'utilisateur ; sinon automatique
// (echelle Windows sous WSL, 1 ailleurs).
double resolveUiScale();

// Pose QT_SCALE_FACTOR si besoin. A appeler avant QApplication.
void applyUiScale();

} // namespace bcad::app
