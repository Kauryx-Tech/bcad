// Découpage de la feuille : vérifié numériquement, sans QPrinter.
// Les appels a effet de bord sont hors des assert() : assert() n'evalue pas son
// argument quand NDEBUG est defini.
//
// L'hôte réserve, le module déclare : la composition prend des hauteurs de
// bande et une liste d'échelles, jamais un cartouche ni une liste nationale.

#include "bcad/geometry/BoundingBox.h"
#include "bcad/layout/Composition.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace bcad;
using namespace bcad::layout;

namespace {

geom::BoundingBox box(double width, double height) {
    geom::BoundingBox bbox;
    bbox.expand(geom::Point2(0, 0));
    bbox.expand(geom::Point2(width, height));
    return bbox;
}

// Ce que le gabarit du module déclare pour son bandeau bas.
constexpr double kBandeauBasMm = 25.0;
const std::vector<int> kEchelles{500, 1000, 2000, 5000};

bool sameRect(const RectMm& a, const RectMm& b) {
    return std::abs(a.x - b.x) < 1e-9 && std::abs(a.y - b.y) < 1e-9 &&
           std::abs(a.w - b.w) < 1e-9 && std::abs(a.h - b.h) < 1e-9;
}

} // namespace

int main() {
    // A3 paysage : 420x297, marges 10 -> imprimable 400x277.
    const Sheet sheet(PaperFormat::A3, Orientation::Paysage);
    Viewport viewport;
    viewport.setSource(box(200, 100));

    const auto composition = composeSheet(sheet, viewport, kEchelles, kBandeauBasMm);

    assert(sameRect(composition.printable, {0, 0, 400, 277}));
    // Le bandeau bas occupe le bas, a l'interieur de l'imprimable.
    assert(sameRect(composition.cartouche, {0, 252, 400, 25}));
    // La zone de dessin ne descend pas sur les meubles.
    assert(std::abs(composition.drawing.bottom() - composition.cartouche.top()) < 1e-9);
    assert(composition.printable.contains(composition.drawing));

    // 200 x 100 m dans 400 x 252 mm : 1:500 est la plus grande echelle admise
    // qui tienne sans empieter sur le bandeau.
    assert(std::abs(composition.suggestedScale - 500) < 1e-9);
    assert(composition.drawing.contains(composition.mapping.rect));
    assert(std::abs(composition.mapping.rect.w - 400) < 1e-9);
    assert(std::abs(composition.mapping.rect.h - 200) < 1e-9);

    // Flèche Nord et barre d'échelle sont posees SUR le plan, donc dans la zone
    // de dessin : elles ne volent pas de place au plan mais doivent rester
    // imprimables.
    assert(composition.mapping.rect.contains(composition.northArrow));
    assert(composition.mapping.rect.contains(composition.scaleBar));

    // Le plan deborde la zone libre : ancrage en haut-gauche, jamais un rognage
    // centre qui cacherait moitie du dessin.
    Viewport tooBig;
    tooBig.setSource(box(5000, 5000));
    const auto overflow = composeSheet(sheet, tooBig, kEchelles, kBandeauBasMm);
    assert(std::abs(overflow.mapping.rect.x - overflow.drawing.x) < 1e-9);
    assert(std::abs(overflow.mapping.rect.y - overflow.drawing.y) < 1e-9);
    assert(overflow.mapping.rect.w > overflow.drawing.w);

    // L'echelle calculee sur la feuille entiere est un piege : elle ignore le
    // bandeau. Un plan de 200 x 130 m y gagne 1:500 mais depasse de 8 mm sur
    // les meubles ; la composition descend a 1:1000 et tout tient.
    Viewport tall;
    tall.setSource(box(200, 130));
    const auto naive = composeSheet(sheet, tall, kEchelles);
    assert(std::abs(naive.suggestedScale - 500) < 1e-9);
    assert(std::abs(naive.mapping.rect.h - 260) < 1e-9);
    assert(naive.mapping.rect.h > naive.printable.h - 25.0);  // déborderait
    const auto composed = composeSheet(sheet, tall, kEchelles, kBandeauBasMm);
    assert(composed.suggestedScale > naive.suggestedScale);
    assert(composed.drawing.contains(composed.mapping.rect));

    // Colonne du tableau : elle est retranchee de la place du plan, pas
    // superposee.
    const auto withTable = composeSheet(sheet, viewport, kEchelles, kBandeauBasMm, 55.0);
    assert(withTable.parcelTable.isValid());
    assert(std::abs(withTable.parcelTable.w - 55) < 1e-9);
    assert(std::abs(withTable.parcelTable.right() - withTable.printable.right()) < 1e-9);
    assert(withTable.printable.contains(withTable.parcelTable));
    assert(std::abs(withTable.drawing.w - (400 - 55 - 2)) < 1e-9);
    assert(withTable.drawing.right() <= withTable.parcelTable.left() + 1e-9);
    assert(withTable.mapping.rect.h <= composed.mapping.rect.h + 1e-9);

    // Repere feuille : Y vers le bas. L'origine monde (minX, maxY) tombe au coin
    // haut-gauche de la place du plan, un point plus au sud descend sur la
    // feuille, un point plus a l'est va vers la droite.
    const auto& mapping = composition.mapping;
    const auto topLeft = mapping.toPage(mapping.origin);
    assert(std::abs(topLeft.x() - mapping.rect.x) < 1e-9);
    assert(std::abs(topLeft.y() - mapping.rect.y) < 1e-9);
    const auto south = mapping.toPage({mapping.origin.x_, mapping.origin.y_ - 10});
    const auto east = mapping.toPage({mapping.origin.x_ + 10, mapping.origin.y_});
    assert(south.y() > topLeft.y());
    assert(east.x() > topLeft.x());
    // 2 mm de papier a 1:500 = 1 m de terrain.
    assert(std::abs(mapping.toWorld(2.0) - 1.0) < 1e-9);

    // Barre d'echelle : uniquement des distances rondes, et jamais plus large
    // que la place reservee.
    const auto bar = makeScaleBar(500, 60);
    assert(bar.groundMeters == 20);
    assert(std::abs(bar.lengthMm - 40) < 1e-9);
    assert(bar.segments * bar.segmentMeters == bar.groundMeters);
    assert(bar.label() == "20 m");
    for (int scale : {200, 250, 500, 1000, 2000, 5000}) {
        const auto b = makeScaleBar(scale, 60);
        assert(b.groundMeters > 0);
        assert(b.lengthMm <= 60 + 1e-9);
        assert(std::abs(b.lengthMm - b.groundMeters * 1000.0 / scale) < 1e-9);
    }

    // Un dessin sans source ne doit pas inventer d'echelle.
    Viewport noSource;
    const auto emptyComposition = composeSheet(sheet, noSource, kEchelles);
    assert(std::abs(emptyComposition.suggestedScale - 500) < 1e-9);

    // Une vue placée porte sa position : l'ancre décidée (coin haut-gauche),
    // la taille à l'échelle — pas le centrage d'office.
    Viewport placee;
    placee.setSource(box(40, 30));
    placee.setScale(1000); // 40×30 mm de plan...
    placee.setPaper({10.0, 20.0, 40.0, 30.0}); // ...ancrés en (10, 20)
    assert(placee.isPlaced());
    const auto composee = composeSheet(sheet, placee, kEchelles, kBandeauBasMm);
    assert(sameRect(composee.mapping.rect, {10.0, 20.0, 40.0, 30.0}));
    // Sans échelle, l'ajustement remplit la place décidée, pas la zone libre :
    // 40×30 m dans 40×30 mm, c'est 1:1000 dans la liste.
    Viewport placeeSansEchelle;
    placeeSansEchelle.setSource(box(40, 30));
    placeeSansEchelle.setPaper({10.0, 20.0, 40.0, 30.0});
    const auto ajustee = composeSheet(sheet, placeeSansEchelle, kEchelles, kBandeauBasMm);
    assert(std::abs(ajustee.suggestedScale - 1000) < 1e-9);
    assert(sameRect(ajustee.mapping.rect, {10.0, 20.0, 40.0, 30.0}));

    std::cout << "composition feuille OK\n";
    return 0;
}
