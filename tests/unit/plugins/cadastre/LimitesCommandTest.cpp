// « Riverains et présence » (K-08) : la commande prepare, cote par cote, la
// presence au bornage et le riverain non leve, sans ecraser une saisie ;
// le tableau des riverains les reprend ; un voisin leve l'emporte sur un nom
// saisi ; annuler retire seulement ce que la commande a cree.
//
// Les appels a effet de bord sont hors des assert().

#include "commands/LimitesCommand.h"
#include "entities/ParcelEntity.h"
#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/properties/PropertyMap.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

int addParcel(core::Document& doc, double x0, const std::string& numero, const std::string& proprietaire) {
    auto parcel = std::make_unique<ParcelEntity>(
        std::vector<geom::Point2>{{x0, 0}, {x0 + 10, 0}, {x0 + 10, 10}, {x0, 10}});
    parcel->properties().setString("cadastre.section", "A");
    parcel->properties().setString("cadastre.numero", numero);
    parcel->properties().setString("cadastre.proprietaire", proprietaire);
    return doc.addEntity(std::move(parcel))->id();
}

std::vector<RiverainRow> rowsOf(const core::Document& doc, const std::string& parcelle) {
    std::vector<RiverainRow> out;
    for (auto& row : riverainRows(doc))
        if (row.parcelle == parcelle) out.push_back(row);
    return out;
}

} // namespace

int main() {
    assert(limiteKey(0, "presence") == "cadastre.limite.000.presence");
    assert(limiteKey(12, "riverain") == "cadastre.limite.012.riverain");
    assert(presenceValues().front() == "Non renseignée");

    core::Document doc;
    const int a1 = addParcel(doc, 0, "1", "Kossi A.");
    const int a2 = addParcel(doc, 10, "2", "Ama B.");
    const int ligne = doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{1, 1}))->id();

    // Sans saisie : rien d'invente.
    auto rows = rowsOf(doc, "A 1");
    assert(rows.size() == 4);
    assert(rows[0].riverain == "—" && rows[0].presence == "Non renseignée");
    assert(rows[1].riverain == "Ama B. (A 2)");

    // La commande prepare 4 cotes x 2 champs sur A 1, rien sur A 2 ni la ligne.
    auto refus = makeEditLimites({});
    assert(!refus);
    auto refusTexte = makeEditLimites({"x"});
    assert(!refusTexte);
    auto prepare = makeEditLimites({std::to_string(a1), std::to_string(ligne)});
    assert(prepare);
    prepare->execute(doc);
    auto& props = doc.findEntity(a1)->properties();
    const properties::Property* presence0 = props.find(limiteKey(0, "presence"));
    assert(presence0 && presence0->type() == properties::PropertyType::Enum);
    assert(presence0->description() == "Côté B1 - B2 · présence au bornage");
    const properties::Property* riverain3 = props.find(limiteKey(3, "riverain"));
    assert(riverain3 && riverain3->description() == "Côté B4 - B1 · riverain non levé");
    assert(!props.find(limiteKey(4, "presence")));
    assert(!doc.findEntity(a2)->properties().find(limiteKey(0, "presence")));
    assert(!doc.findEntity(ligne)->properties().find(limiteKey(0, "presence")));

    // La saisie : un riverain non leve au cote 0, un nom ignore au cote 1
    // (A 2 le borde, le dessin fait foi), une absence au cote 1.
    props.setString(limiteKey(0, "riverain"), "Voie publique");
    props.setString(limiteKey(1, "riverain"), "Nom saisi");
    props.setEnum(limiteKey(1, "presence"), 2);
    rows = rowsOf(doc, "A 1");
    assert(rows[0].riverain == "Voie publique");
    assert(rows[1].riverain == "Ama B. (A 2)" && rows[1].presence == "Absent");
    assert(rows[2].riverain == "—" && rows[2].presence == "Non renseignée");

    // Relancer ne cree rien et garde la saisie ; l'annuler ne retire donc rien.
    EditLimitesCommand relance({a1});
    relance.execute(doc);
    relance.undo(doc);
    assert(props.getString(limiteKey(0, "riverain")) == "Voie publique");
    assert(props.getEnum(limiteKey(1, "presence")) == 2);

    // Annuler la premiere passe retire ses champs, et seulement eux.
    prepare->undo(doc);
    assert(!props.find(limiteKey(0, "presence")) && !props.find(limiteKey(3, "riverain")));
    assert(props.getString("cadastre.proprietaire") == "Kossi A.");
    rows = rowsOf(doc, "A 1");
    assert(rows[0].riverain == "—");

    std::cout << "cadastre_limites_test OK\n";
    return 0;
}
