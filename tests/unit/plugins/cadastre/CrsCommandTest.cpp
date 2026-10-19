// Systeme de coordonnees du dossier (K-06) : la liste sourcee du module se lit,
// un code EPSG tape devient le libelle du cartouche, la commande s'annule.
//
// Les appels a effet de bord sont hors des assert().

#include "Templates.h"
#include "commands/CrsCommand.h"
#include "crs/SystemesCoordonnees.h"
#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

int main() {
    // La donnee livree avec le module (arbre des sources).
    const auto connus = lireSystemesCoordonnees(BCAD_CADASTRE_CRS_JSON);
    assert(connus.size() >= 10);
    bool togo = false, ci = false;
    for (const auto& s : connus) {
        togo = togo || (s.pays == "TG" && s.epsg == 32631);
        ci = ci || (s.pays == "CI" && s.epsg == 2041);
        assert(!s.valide);   // rien n'est confirme par un geometre a ce jour
    }
    assert(togo && ci);
    assert(lireSystemesCoordonnees("/nulle/part.json").empty());

    // Saisie du code.
    assert(codeEpsgSaisi("32631") == 32631);
    assert(codeEpsgSaisi(" EPSG:2041 ") == 2041);
    assert(codeEpsgSaisi("epsg 32628") == 32628);
    assert(!codeEpsgSaisi("UTM 31"));
    assert(!codeEpsgSaisi(""));
    assert(!codeEpsgSaisi("-5"));

    // Libelle imprime.
    assert(libelleSysteme(32631, connus) == "WGS 84 / UTM zone 31N (EPSG:32631)");
    assert(libelleSysteme(4326, connus) == "EPSG:4326 (non répertorié)");

    // Commande : ecrit, garde le libelle, s'annule.
    core::Document doc;
    SetCrsCommand set(libelleSysteme(2041, connus));
    set.execute(doc);
    assert(doc.properties().getString(kCleSystemeCoordonnees) == "Abidjan 1987 / UTM zone 30N (EPSG:2041)");
    assert(doc.properties().find(kCleSystemeCoordonnees)->description() == "Système de coordonnées");
    set.undo(doc);
    assert(!doc.properties().has(kCleSystemeCoordonnees));
    assert(!makeSetCrs({"pas un code"}));
    assert(makeSetCrs({"32631"}));

    // Le cartouche du plan a un champ pour lui.
    bool champ = false;
    for (const auto& f : defaultCartoucheTemplate().fields) champ = champ || f.key == kCleSystemeCoordonnees;
    assert(champ);

    std::cout << "Systeme de coordonnees : tests PASSED\n";
    return 0;
}
