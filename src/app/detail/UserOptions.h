// Reglages de l'utilisateur (boite Options, S-15) : un groupe par sujet, garde
// par utilisateur (QSettings), jamais dans le dessin. Pur (aucun Qt).
//
// Ajouter un reglage :
//   1. un champ dans le groupe concerne (ou un nouveau groupe ici) ;
//   2. sa cle dans loadUserOptions / saveUserOptions (widgets/OptionsDialog.cpp) ;
//   3. sa ligne dans l'onglet d'AutoCAD qui le porte (meme fichier) ;
//   4. son effet dans MainWindow::applyOptions (window/MainWindowOptions.cpp).
//
// Groupes attendus (TODO_OUTILS.md) : accrochage automatique et zone de visee
// (S-12 a S-14), clic droit et double-clic (S-08e, S-16), poignees (M-02),
// apercu de selection (S-10), unites (U-01).

#pragma once

#include "detail/CursorStyle.h"

namespace bcad::app::detail {

struct UserOptions {
    CursorStyle cursor;   // Affichage (reticule) et Selection (cible)
};

} // namespace bcad::app::detail
