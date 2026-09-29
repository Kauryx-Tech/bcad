#pragma once

#include "bcad/layout/Borne.h"
#include "bcad/layout/Composition.h"
#include "bcad/layout/FurniturePaint.h"
#include "bcad/layout/Label.h"
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

// Export PDF vectoriel — interface, implémentation via QPrinter dans src/layout
// Précondition : un QGuiApplication doit exister (QPrinter + polices), sinon Qt
// aborte le processus. Un test sans fenetre lance QT_QPA_PLATFORM=offscreen.
struct PdfExportOptions {
    std::string outputPath;
    Sheet sheet;
    Viewport viewport;
    const core::Document* document = nullptr;

    // Meuble de la feuille. Les étiquettes, les bornes, les cartouches et les
    // nomenclatures sont fournis par l'appelant, résolus depuis les gabarits du
    // module : src/layout peint des libellés qu'il ne comprend pas (ADR-017) et
    // ignore tout nom de propriété métier (ADR-016).
    // Positions en unités monde ; la conversion feuille se fait par la composition.
    std::vector<Label> labels;
    std::vector<Borne> bornes;
    // Bandeau bas, peints empilés depuis le bas dans la bande réservée.
    std::vector<ResolvedFurniture> meubles;
    // Colonne de droite, peints en tableaux dans la colonne réservée.
    std::vector<ResolvedFurniture> tables;
    // Échelles admises par le profil national du module ; vide = ajustement
    // exact, sans arrondi sur une liste que personne n'a donnée.
    std::vector<int> permittedScales;
    bool showLabels = true;   // dessine `labels`
    bool showNorthArrow = true;
    bool showScaleBar = true;
    double northArrowAngleDeg = 0;  // 0 = le nord du plan est vers le haut
};

// Rend la feuille complète — plan, étiquettes, bornes, meubles, tableaux,
// flèche Nord, barre d'échelle — et pose lui-même le repère millimètre du
// peintre. Partagé par exportPdf() et par l'aperçu d'impression : un seul rendu
// pour les deux. Le peintre doit être actif sur un périphérique qui couvre la
// zone imprimable de `opts.sheet` (cas d'un QPrinter).
void drawSheet(QPainter& painter, const PdfExportOptions& opts);

// Applique `sheet` (format, orientation, marges) au QPrinter d'un export ou d'un
// aperçu : les deux doivent donner la même page.
void applyPageLayout(QPrinter* printer, const Sheet& sheet);

// Fixe l'échelle à la première admise qui tient sur la feuille, compte tenu de
// la place que les meubles et les tableaux réservent. Une seule passe suffit :
// les zones de la composition dépendent des gabarits, pas de l'échelle choisie.
// Ne touche à aucun meuble : le texte « 1:n » est un champ du module, posé par
// lui une fois l'échelle connue.
void applyFittingScale(PdfExportOptions& opts);

bool exportPdf(const PdfExportOptions& opts, std::string* error = nullptr);

} // namespace bcad::layout
