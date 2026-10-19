// Systeme de coordonnees du dossier (K-06) : la liste sourcee du module se lit,
// un code EPSG tape devient le libelle du cartouche, la commande s'annule.
//
// Les appels a effet de bord sont hors des assert().

#include "Templates.h"
#include "commands/CrsCommand.h"
#include "crs/SystemesCoordonnees.h"
#include "io/DxfExporter.h"
#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/properties/PropertyMap.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
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

    // Le code numerique accompagne le libelle : les exports (GeoPackage, DXF)
    // ont besoin d'un entier, pas seulement du texte du cartouche.
    core::Document docCode;
    SetCrsCommand setCode(libelleSysteme(32631, connus), 32631);
    setCode.execute(docCode);
    assert(docCode.properties().getInt(kCleEpsgDossier) == 32631);
    setCode.undo(docCode);
    assert(!docCode.properties().has(kCleEpsgDossier));
    const auto fabriquee = makeSetCrs({"EPSG:2041"});
    assert(fabriquee);
    core::Document docFabrique;
    fabriquee->execute(docFabrique);
    assert(docFabrique.properties().getInt(kCleEpsgDossier) == 2041);

    // La definition OGC voyage avec la donnee : GeoPackage l'ecrit telle quelle.
    const auto* wgs = trouverSysteme(32631, connus);
    assert(wgs && wgs->wkt.rfind("PROJCS[", 0) == 0);

    // L'export DXF nomme le repere du dossier dans les proprietes du fichier.
    {
        core::Document plan;
        plan.addEntity(std::make_unique<geom::LineEntity>(geom::Point2{0, 0}, geom::Point2{10, 10}));
        SetCrsCommand crs(libelleSysteme(32631, connus), 32631);
        crs.execute(plan);
        const std::string chemin = "/tmp/bcad_crs_dxf_test.dxf";
        std::string err;
        assert(DxfExporter{}.writeDocument(plan, chemin, &err));
        assert(err.empty());
        std::ifstream fichier(chemin);
        const std::string contenu((std::istreambuf_iterator<char>(fichier)),
                                  std::istreambuf_iterator<char>());
        assert(contenu.find("$PROJECTNAME") != std::string::npos);
        assert(contenu.find("WGS 84 / UTM zone 31N") != std::string::npos);
        std::filesystem::remove(chemin);
    }

    std::cout << "Systeme de coordonnees : tests PASSED\n";
    return 0;
}
