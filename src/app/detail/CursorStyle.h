// Reglages du pointeur du canevas (S-11), comme AutoCAD : reticule en croix
// (CURSORSIZE, en % de l'ecran), cible de selection (PICKBOX, demi-cote en
// px, qui est aussi la tolerance de designation), couleur du reticule, et le
// choix du pointeur du systeme pour qui prefere la fleche. Par defaut : la
// croix d'AutoCAD.
//
// Pur (aucun Qt) : la boite Options les modifie, QSettings les garde, le
// canevas les applique ; teste par cursor_style_test.

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace bcad::app::detail {

struct CursorStyle {
    static constexpr int kCrosshairMin = 1, kCrosshairMax = 100, kCrosshairDefault = 5;
    static constexpr int kPickboxMin = 2, kPickboxMax = 30, kPickboxDefault = 6;

    int crosshairPercent = kCrosshairDefault;
    int pickboxPx = kPickboxDefault;
    // Couleur choisie du reticule (0xRRGGBB) ; vide = celle du theme.
    std::optional<std::uint32_t> crosshairRgb;
    bool systemPointer = false;

    // Valeurs ramenees dans leurs bornes (reglage lu d'un fichier, saisie).
    CursorStyle clamped() const {
        CursorStyle out = *this;
        out.crosshairPercent = std::clamp(crosshairPercent, kCrosshairMin, kCrosshairMax);
        out.pickboxPx = std::clamp(pickboxPx, kPickboxMin, kPickboxMax);
        return out;
    }

    // Demi-longueur des traits en px pour un canevas w x h : le pourcentage
    // de la plus grande dimension, comme CURSORSIZE ; a 100 %, les traits
    // traversent tout le canevas.
    double crosshairHalfLength(int width, int height) const {
        const double span = std::max(width, height);
        const int percent = std::clamp(crosshairPercent, kCrosshairMin, kCrosshairMax);
        return percent >= kCrosshairMax ? span : span * percent / 200.0;
    }
};

// Ce que montre le pointeur selon l'etat de la commande, comme AutoCAD.
enum class CursorShape {
    CrosshairAndBox,   // au repos : on pointe un objet ou on lance une commande
    Crosshair,         // un point est attendu
    Box,               // des objets sont a designer
};

} // namespace bcad::app::detail
