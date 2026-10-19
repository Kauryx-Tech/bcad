// « Cartouche du dossier » : les champs que le plan attend sont prepares dans
// les attributs du dessin, avec leur libelle, sans ecraser une saisie ; le
// cartouche du plan reprend ce que l'operateur a saisi ; annuler retire
// seulement ce que la commande a cree.
//
// Les appels a effet de bord sont hors des assert().

#include "commands/DossierCommand.h"
#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/validation/Diagnostics.h"

#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

std::size_t attributFields() {
    std::size_t n = 0;
    for (const auto& f : defaultCartoucheTemplate().fields)
        if (f.role == "attribut" && !f.key.empty()) ++n;
    return n;
}

std::string cartoucheValue(const core::Document& doc, const std::string& key) {
    std::vector<validation::Diagnostic> diagnostics;
    const auto cartouche = buildCartoucheFurniture(doc, defaultCartoucheTemplate(), diagnostics);
    for (const auto& f : cartouche.fields)
        if (f.key == key)
            if (const auto* s = std::get_if<std::string>(&f.value.value)) return *s;
    return {};
}

} // namespace

int main() {
    core::Document doc;
    doc.properties().addString("cadastre.dossier.geometre", "Kossi A.");   // deja saisi

    EditDossierCommand prepare;
    prepare.execute(doc);
    const std::size_t expected = attributFields();
    assert(expected > 10);
    assert(doc.properties().listNames().size() == expected);
    // Libelle donne a chaque champ, saisie gardee.
    const properties::Property* geometre = doc.properties().find("cadastre.dossier.geometre");
    assert(geometre && geometre->description() == "Géomètre");
    assert(doc.properties().getString("cadastre.dossier.geometre") == "Kossi A.");
    const properties::Property* projet = doc.properties().find("cadastre.dossier.projet");
    assert(projet && projet->description() == "Projet" && projet->asString().empty());

    // Le plan reprend ce que l'operateur saisit.
    doc.properties().setString("cadastre.dossier.projet", "Lotissement Agoè");
    assert(cartoucheValue(doc, "cadastre.dossier.projet") == "Lotissement Agoè");
    assert(cartoucheValue(doc, "cadastre.dossier.geometre") == "Kossi A.");

    // Relancee : rien de cree, rien d'ecrase.
    EditDossierCommand again;
    again.execute(doc);
    assert(doc.properties().listNames().size() == expected);
    assert(doc.properties().getString("cadastre.dossier.projet") == "Lotissement Agoè");
    again.undo(doc);
    assert(doc.properties().listNames().size() == expected);

    // Annuler la premiere : seuls les champs crees partent, la saisie anterieure reste.
    prepare.undo(doc);
    assert(doc.properties().listNames().size() == 1);
    assert(doc.properties().getString("cadastre.dossier.geometre") == "Kossi A.");

    assert(!makeEditDossier({"x"}));
    assert(makeEditDossier({}));

    std::cout << "Cartouche du dossier : tests PASSED\n";
    return 0;
}
