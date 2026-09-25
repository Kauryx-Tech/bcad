#pragma once

#include "bcad/layout/Borne.h"
#include "bcad/layout/Composition.h"
#include "bcad/layout/Cartouche.h"
#include "bcad/layout/Label.h"
#include "bcad/layout/ParcelTable.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include <string>
#include <vector>

class QPainter;
class QPrinter;
class QRectF;

namespace bcad::core {
class Document;
}

namespace bcad::layout {

// Export PDF vectoriel (I2) — interface, implémentation via QPrinter dans src/layout
// Précondition : un QGuiApplication doit exister (QPrinter + polices du cartouche),
// sinon Qt aborte le processus. Un test sans fenetre lance QT_QPA_PLATFORM=offscreen.
struct PdfExportOptions {
    std::string outputPath;
    Sheet sheet;
    Viewport viewport;
    Cartouche cartouche;
    const core::Document* document = nullptr;

    // Meuble de la feuille. Les étiquettes, les bornes et le tableau sont fourni
    // par l'appelant : src/layout ignore tout nom de propriété métier (ADR-016),
    // c'est le plugin qui les construit depuis ses entités.
    // Positions en unités monde ; la conversion feuille se fait par la composition.
    std::vector<Label> labels;
    std::vector<Borne> bornes;
    ParcelTable parcelTable;
    bool showLabels = true;   // dessine `labels`
    bool showNorthArrow = true;
    bool showScaleBar = true;
    double northArrowAngleDeg = 0;  // 0 = le nord du plan est vers le haut
};

// Rend la feuille complète — plan, étiquettes, bornes, cartouche, flèche Nord,
// barre d'échelle, tableau des parcelles — et pose lui-même le repère millimètre
// du peintre. Partagé par exportPdf() et par l'aperçu d'impression : un seul
// rendu pour les deux. Le peintre doit être actif sur un périphérique qui
// couvre la zone imprimable de `opts.sheet` (cas d'un QPrinter).
void drawSheet(QPainter& painter, const PdfExportOptions& opts);

// Applique `sheet` (format, orientation, marges) au QPrinter d'un export ou d'un
// aperçu : les deux doivent donner la même page.
void applyPageLayout(QPrinter* printer, const Sheet& sheet);

// Fixe l'échelle au standard qui tient sur la feuille et reporte « 1:n » dans le
// cartouche. Une seule passe suffit : les zones de la composition dépendent du
// cartouche et du tableau, pas de l'échelle choisie.
void applySuggestedScale(PdfExportOptions& opts);

// Largeur réservée par le tableau des parcelles quand il n'est pas vide.
inline constexpr double kParcelTableWidthMm = 55.0;

bool exportPdf(const PdfExportOptions& opts, std::string* error = nullptr);

// Dessine le cartouche enrichi (grille label+valeur) dans `rect`.
// Partagé par drawSheet() et par l'aperçu impression (MainWindow).
// Chaque cellule non vide du cartouche apparaît comme "LABEL : valeur".
void drawCartouche(QPainter& painter, const QRectF& rect, const Cartouche& cartouche);

} // namespace bcad::layout
