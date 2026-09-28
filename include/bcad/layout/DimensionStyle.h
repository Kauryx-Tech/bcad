#pragma once

// Dimension styles : text height, arrowheads, precision, etc.
// Inspiré de la structure DXF DIMSTYLE (groupe 2 = DIMSTYLE).
// Une dimension référence son style par nom (string) — pas de pointer.

#include "bcad/geometry/Point.h"
#include <string>
#include <optional>

namespace bcad::layout {

enum class ArrowheadType {
    ClosedFilled = 0,     // flèche fermée pleine (défaut)
    ClosedBlank = 1,      // flèche fermée vide
    Closed = 2,           // flèche fermée (alias ClosedFilled)
    Dot = 3,              // point
    DotSmall = 4,         // petit point
    DotBlank = 5,         // point vide
    Origin = 6,           // indicateur d'origine
    Origin2 = 7,          // indicateur d'origine 2
    Open = 8,             // flèche ouverte
    Open90 = 9,           // flèche ouverte à 90°
    Open30 = 10,          // flèche ouverte à 30°
    ClosedSmall = 11,     // flèche fermée petite
    Triangle = 12,        // triangle
    TriangleSmall = 13,   // petit triangle
    TriangleBlank = 14,   // triangle vide
    Integral = 15,        // intégrale
    None = 16             // aucune flèche
};

struct DimensionStyle {
    std::string name;             // nom du style (ex: "Standard", "ISO-25")
    
    // Texte
    double textHeight = 2.5;      // hauteur du texte (unités monde)
    std::string textFont;         // nom de police (ex: "OpenSans", "ISO3098")
    bcad::geom::Color textColor = bcad::geom::Color::fromRgb255(0, 0, 0);
    double textOffset = 0.5;      // distance texte ↔ ligne de cote (unités monde)
    bool textAbove = true;        // texte au-dessus de la ligne de cote
    int precision = 2;            // décimales (0 = entier, 2 = 0.00, etc.)
    bool suppressZero = false;    // supprimer zéros inutiles (0.50 → .5)
    std::string prefix;           // préfixe (ex: "R " pour rayon)
    std::string suffix;           // suffixe (ex: " mm")
    
    // Flèches
    ArrowheadType arrowhead1 = ArrowheadType::ClosedFilled;  // flèche côté 1
    ArrowheadType arrowhead2 = ArrowheadType::ClosedFilled;  // flèche côté 2
    double arrowSize = 1.0;       // taille flèche (multiplicateur textHeight)
    
    // Lignes
    double dimLineOffset = 0.5;   // distance ligne cote ↔ objet (unités monde)
    double extLineOffset = 0.0;   // dépassement lignes d'extension
    double extLineExtend = 0.0;   // extension au-delà de la ligne de cote
    bcad::geom::Color dimLineColor = bcad::geom::Color::fromRgb255(0, 0, 0);
    bcad::geom::Color extLineColor = bcad::geom::Color::fromRgb255(0, 0, 0);
    
    // Arrondi / unités
    int roundOff = 0;             // arrondi (0 = aucun, 1 = 0.1, 2 = 0.01, etc.)
    bool forceUnit = false;       // forcer affichage unités
    std::string unitSuffix;       // suffixe unité (ex: " m", " mm")
    
    // Tolérances (pour cotations avec tolérance)
    bool showTolerance = false;
    double toleranceUpper = 0.0;
    double toleranceLower = 0.0;
    int tolerancePrecision = 2;
    
    // Style par défaut (ISO-ish)
    static DimensionStyle standard() {
        return DimensionStyle{"Standard"};
    }
    
    explicit DimensionStyle(std::string n = "Standard") : name(std::move(n)) {}
};

} // namespace bcad::layout