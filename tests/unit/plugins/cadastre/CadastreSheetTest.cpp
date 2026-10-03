// Le meuble de feuille cadastral est le seul endroit ou les proprietes
// `cadastre.*` sont converties en objets de mise en page : src/layout et src/app
// les ignorent (ADR-016). Sans ce test, la feuille exportee ne montrerait ni
// numero de parcelle ni tableau, et rien ne le signalerait.
//
// La nomenclature est un meuble déclare comme les autres : en-têtes du gabarit,
// champs rangés par lignes, total calculé en dernière ligne.
//
// Les appels a effet de bord sont hors des assert() : assert() n'evalue pas son
// argument quand NDEBUG est defini.

#include "layout/CadastreSheet.h"

#include "entities/ParcelEntity.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/PdfExport.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

std::unique_ptr<ParcelEntity> makeParcel(double x, double y, double width, double height,
                                         const std::string& section, const std::string& numero,
                                         const std::string& contenance) {
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{
        {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}});
    parcel->properties().setString("cadastre.section", section);
    parcel->properties().setString("cadastre.numero", numero);
    parcel->properties().setString("cadastre.contenance", contenance);
    parcel->properties().setString("cadastre.commune", "Lome");
    return parcel;
}

// Deux parcelles qui se touchent partagent leurs sommets : une borne commune,
// pas deux.
void addTwoAdjacentParcels(core::Document& document) {
    document.addEntity(makeParcel(0, 0, 10, 10, "A", "01", "100,00 m²"));
    document.addEntity(makeParcel(10, 0, 10, 10, "A", "02", "100,00 m²"));
}

std::size_t distinctBornePositions(const std::vector<layout::PointMarker>& bornes) {
    std::set<std::pair<long long, long long>> keys;
    for (const auto& borne : bornes) {
        keys.insert({llround(borne.position.x() * 1000.0), llround(borne.position.y() * 1000.0)});
    }
    return keys.size();
}

std::string texteDuChamp(const layout::ResolvedFurniture& resolu, int slot) {
    for (const auto& champ : resolu.fields) {
        if (champ.slot != slot) continue;
        if (const auto* texte = std::get_if<std::string>(&champ.value.value)) return *texte;
        return {};
    }
    return {};
}

} // namespace

int main() {
    // Document vide : aucun meuble, donc aucune zone reservee a la composition.
    {
        core::Document empty;
        const auto furniture = buildSheetFurniture(empty);
        assert(furniture.labels.empty());
        assert(furniture.bornes.empty());
        const auto nomenclature = buildNomenclatureFurniture(empty, defaultNomenclatureTemplate());
        assert(nomenclature.fields.empty());
    }

    core::Document document;
    addTwoAdjacentParcels(document);
    // Une polygone sans proprietes cadastrales n'est pas une parcelle.
    document.addEntity(std::make_unique<geom::PolylineEntity>(
        std::vector<geom::Point2>{{50, 50}, {60, 50}, {60, 60}}, true));

    const auto furniture = buildSheetFurniture(document);

    assert(furniture.labels.size() == 2);
    // 8 sommets bruts, 6 bornes distinctes.
    assert(furniture.bornes.size() == 6);
    assert(distinctBornePositions(furniture.bornes) == furniture.bornes.size());

    // Etiquette au centroide, lisible sur le plan : section + numero + contenance.
    const auto& first = furniture.labels[0];
    assert(first.text.find("A") != std::string::npos);
    assert(first.text.find("01") != std::string::npos);
    assert(first.text.find("100,00") != std::string::npos);
    assert(std::abs(first.position.x() - 5.0) < 1e-9);
    assert(std::abs(first.position.y() - 5.0) < 1e-9);
    const auto& second = furniture.labels[1];
    assert(std::abs(second.position.x() - 15.0) < 1e-9);

    // La nomenclature porte les deux parcelles en lignes de trois champs, plus
    // la ligne de total des surfaces calculees (2 x 100 m²).
    const auto nomenclature =
        buildNomenclatureFurniture(document, defaultNomenclatureTemplate());
    assert(nomenclature.gabarit.columnLabels.size() == 3);
    assert(texteDuChamp(nomenclature, 0) == "A");
    assert(texteDuChamp(nomenclature, 1) == "01");
    assert(texteDuChamp(nomenclature, 2) == "100,00 m²");
    assert(texteDuChamp(nomenclature, 3) == "A");
    assert(texteDuChamp(nomenclature, 4) == "02");
    assert(texteDuChamp(nomenclature, 6) == "Total");
    assert(texteDuChamp(nomenclature, 8) == "200.00 m²");

    // Le tableau alimente la colonne reservee par la composition : une feuille
    // sans tableau ne doit pas perdre 55 mm de plan.
    const layout::Sheet sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
    layout::Viewport viewport;
    viewport.setSource(document.extents());
    const std::vector<int> echelles{500, 1000};
    const auto withoutTable = layout::composeSheet(sheet, viewport, echelles);
    const auto withTable =
        layout::composeSheet(sheet, viewport, echelles, 0.0, nomenclature.gabarit.reservedZone.w);
    assert(!withoutTable.parcelTable.isValid());
    assert(withTable.parcelTable.isValid());
    assert(std::abs(withoutTable.drawing.w - withTable.drawing.w -
                    nomenclature.gabarit.reservedZone.w - 2.0) < 1e-9);

    std::cout << "meuble de feuille cadastral OK\n";
    return 0;
}
