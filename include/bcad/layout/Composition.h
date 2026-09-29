#pragma once

// Découpage de la feuille (I2/I3) : mathématiques pures en millimètres, sans Qt
// ni QPrinter. Le même calcul sert à l'export PDF et à l'aperçu d'impression,
// et se vérifie par un test numérique sans ouvrir de fenêtre.

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Point.h"
#include "bcad/layout/GeometryMm.h"
#include "bcad/layout/Scale.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"

#include <algorithm>
#include <vector>

namespace bcad::layout {

// Repère monde (mètres, Y vers le haut) -> repère feuille (millimètres, Y vers
// le bas). `rect` est la place occupée par le plan, `origin` le coin monde
// (minX, maxY) ancré au coin haut-gauche de cette place.
struct PageMapping {
    RectMm rect;
    double scale = 500;
    geom::Point2 origin;

    geom::Point2 toPage(const geom::Point2& world) const {
        return {rect.x + (world.x_ - origin.x_) * 1000.0 / scale,
                rect.y + (origin.y_ - world.y_) * 1000.0 / scale};
    }

    // Grandeur monde d'une taille fixe sur papier : bornes, ronds, épaisseurs.
    double toWorld(double mm) const { return mm * scale / 1000.0; }
};

struct SheetComposition {
    RectMm printable;     // cadre de la feuille (zone imprimable)
    RectMm drawing;       // zone où le plan a le droit d'aller
    RectMm cartouche;     // bandeau bas, vide si le cartouche est vide
    RectMm parcelTable;   // colonne de droite, vide sans tableau
    RectMm northArrow;    // carré réservé dans l'angle haut-droit du plan
    RectMm scaleBar;      // bande réservée dans l'angle bas-gauche du plan
    PageMapping mapping;  // place du plan, centrée dans `drawing`
    double suggestedScale = 0;  // échelle standard qui tient dans `drawing`
};

// Échelle admise pour que `source` tienne dans une zone de la feuille, prise
// dans la liste du profil national du module. `Sheet::printableHeight()` ne
// connaît ni les meubles ni le tableau : passer par la zone libre est ce qui
// évite que le plan déborde dessus.
inline double permittedScaleFor(const geom::BoundingBox& source, const RectMm& zone,
                                const std::vector<int>& permitted) {
    if (!source.isValid() || !zone.isValid()) return 500;
    const double sx = source.width() * 1000.0 / zone.w;
    const double sy = source.height() * 1000.0 / zone.h;
    return nearestPermittedScale(std::max(sx, sy), permitted);
}

// `bottomBandMm` : ce que les meubles du bandeau bas réservent (somme des
// hauteurs que leurs gabarits déclarent) — 0 = pas de meuble. `rightColumnMm`
// à 0 = pas de tableau. La flèche Nord est posée sur le plan et non soustraite
// de la zone de dessin. L'hôte réserve, le module déclare : ni l'un ni l'autre
// ne devine.
//
// Une vue placée (`Viewport::isPlaced()`) porte sa position : source, échelle
// et emplacement sont trois données distinctes (ADR-017, décision 2). La place
// décidée donne l'ancre (coin haut-gauche), l'échelle donne la taille —
// jamais l'inverse : centrer d'office une vue placée serait réécrire le choix
// de l'opérateur. Sans place, le plan est centré dans la zone libre comme
// avant. Et sans échelle, l'ajustement remplit la place décidée, pas la zone
// libre : la place est la donnée, la zone libre n'est que son défaut.
inline SheetComposition composeSheet(const Sheet& sheet,
                                     const Viewport& viewport,
                                     const std::vector<int>& permittedScales,
                                     double bottomBandMm = 0.0,
                                     double rightColumnMm = 0.0,
                                     double northArrowSizeMm = 15.0) {
    SheetComposition composition;
    constexpr double kGapMm = 2.0;

    // Le peripherique de rendu d'un QPrinter COUVRE la zone imprimable, marges
    // deja deductives : repere = millimetre de cette zone, origine a son coin
    // haut-gauche. Translator encore des marges deplacerait la feuille de
    // 10 mm hors du papier, et le cadre serait rogne en bas et a droite.
    composition.printable = {0, 0, sheet.printableWidth(), sheet.printableHeight()};
    RectMm freeZone = composition.printable;

    if (bottomBandMm > 0.0) {
        const double height = std::min(bottomBandMm, freeZone.h);
        composition.cartouche = {freeZone.x, freeZone.bottom() - height, freeZone.w, height};
        freeZone.h = std::max(0.0, composition.cartouche.y - freeZone.y);
    }

    if (rightColumnMm > 0.0 && freeZone.w > rightColumnMm + kGapMm) {
        composition.parcelTable = {freeZone.right() - rightColumnMm, freeZone.y,
                                   rightColumnMm, freeZone.h};
        freeZone.w -= rightColumnMm + kGapMm;
    }

    composition.drawing = freeZone;
    const RectMm& placeDecidee = viewport.isPlaced() ? viewport.paper() : freeZone;
    composition.suggestedScale =
        permittedScaleFor(viewport.source(), placeDecidee, permittedScales);

    const auto& source = viewport.source();
    const double scale = viewport.scale() > 0 ? viewport.scale() : composition.suggestedScale;
    const double planWidth = source.width() * 1000.0 / scale;
    const double planHeight = source.height() * 1000.0 / scale;

    RectMm plan;
    plan.w = planWidth;
    plan.h = planHeight;
    if (viewport.isPlaced()) {
        plan.x = viewport.paper().x;
        plan.y = viewport.paper().y;
    } else {
        plan.x = freeZone.x + std::max(0.0, (freeZone.w - planWidth) * 0.5);
        plan.y = freeZone.y + std::max(0.0, (freeZone.h - planHeight) * 0.5);
    }
    // Un plan plus grand que la zone libre reste ancré en haut à gauche : le
    // débord est visible, donc corrigeable par l'appelant (autre format, autre
    // échelle) plutôt qu'une rognage silencieux centré.

    composition.mapping.rect = plan;
    composition.mapping.scale = scale;
    composition.mapping.origin = {source.minX, source.maxY};

    const double arrow = std::max(0.0, northArrowSizeMm);
    composition.northArrow = {plan.right() - arrow, plan.y, arrow, arrow};
    composition.scaleBar = {plan.x, plan.bottom() - 8.0, 60.0, 6.0};
    return composition;
}

} // namespace bcad::layout
