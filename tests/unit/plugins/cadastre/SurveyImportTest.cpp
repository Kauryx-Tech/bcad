// Import d'un carnet de leve en bornes (K-03).
//
// Exports courants de station totale et de GNSS : `matricule X Y [Z] [code]`,
// separateurs varies, virgule ou point decimal, en-tete et commentaires.
// Une ligne illisible est signalee avec son numero, jamais devinee.

#include "entities/SurveyMarkEntity.h"
#include "io/SurveyImporter.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

} // namespace

int main() {
    // Point-virgule et virgule decimale, en-tete, BOM, commentaire.
    auto parse = parseSurveyText(
        "\xEF\xBB\xBF" "Matricule;X;Y;Z;Code\n"
        "# station de depart\n"
        "B1;350123,456;712456,789;45,20;BORNE\n"
        "R1;350130,000;712460,000;;REP\n");
    assert(parse.rejects.empty());
    assert(parse.points.size() == 2);
    assert(parse.points[0].name == "B1" && near(parse.points[0].x, 350123.456));
    assert(near(parse.points[0].y, 712456.789) && parse.points[0].z && near(*parse.points[0].z, 45.2));
    assert(parse.points[0].code == "BORNE");
    assert(!parse.points[1].z && parse.points[1].code == "REP");   // Z vide : absent, code garde

    // Virgule separatrice : point decimal ; sans Z ni code.
    parse = parseSurveyText("PT1,100.5,200.25\nPT2,101,201,12.5,ST\n");
    assert(parse.points.size() == 2 && near(parse.points[0].x, 100.5) && !parse.points[0].z);
    assert(parse.points[1].z && near(*parse.points[1].z, 12.5) && parse.points[1].code == "ST");

    // Tabulations et espaces ; code de plusieurs mots.
    parse = parseSurveyText("S1\t10.0\t20.0\t1.0\tcoin mur\n  S2   11   21  \n");
    assert(parse.points.size() == 2 && parse.points[0].code == "coin mur");
    assert(near(parse.points[1].y, 21.0));

    // Lignes ecartees, avec numero ; les autres passent.
    parse = parseSurveyText("P1;10;20\nP2;abc;20\nP3;5\n\nP4;30;40\n");
    assert(parse.points.size() == 2);
    assert(parse.rejects.size() == 2);
    assert(parse.rejects[0].rfind("ligne 2 :", 0) == 0);
    assert(parse.rejects[1].rfind("ligne 3 :", 0) == 0);

    // Import dans un document : une borne par point, nature tiree du code.
    const std::string path = "/tmp/bcad_survey_import_test.csv";
    {
        std::ofstream out(path);
        out << "B1;0;0;10,5;BORNE\nR1;5;0;;REP\nS1;5;5;;STATION\nX;faux;0\n";
    }
    core::Document doc;
    std::string message;
    const bool imported = SurveyImporter{}.readDocument(doc, path, &message);
    assert(imported);
    assert(doc.entities().size() == 3);
    const auto* first = dynamic_cast<const SurveyMarkEntity*>(doc.entities()[0].get());
    assert(first && near(first->position().x_, 0.0));
    assert(first->properties().getString("cadastre.reference") == "B1");
    assert(near(first->properties().getDouble("cadastre.altitude"), 10.5));
    assert(first->properties().getEnum("cadastre.mark_type") == 0);                          // borne
    assert(doc.entities()[1]->properties().getEnum("cadastre.mark_type") == 1);             // repere
    assert(doc.entities()[2]->properties().getEnum("cadastre.mark_type") == 3);             // station
    // La ligne ecartee est rapportee, l'import reste reussi.
    assert(message.find("3 point(s) importé(s)") != std::string::npos);
    assert(message.find("ligne 4") != std::string::npos);

    // Aucun point lisible : echec explique.
    {
        std::ofstream out(path);
        out << "rien de lisible ici\nni la\n";
    }
    core::Document empty;
    message.clear();
    assert(!SurveyImporter{}.readDocument(empty, path, &message));
    assert(empty.entities().empty() && !message.empty());
    std::remove(path.c_str());

    std::printf("Import de leve : tests PASSED\n");
    return 0;
}
