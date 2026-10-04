// Tableau des coordonnees des bornes (K-04), sur le plan et en CSV.
//
// Exige par le dossier technique en Cote d'Ivoire, attendu de tout plan
// georeference : borne, X, Y, borne suivante, gisement (grades, depuis le nord,
// sens horaire), distance. Les numeros de borne sont ceux du plan.

#include "entities/ParcelEntity.h"
#include "io/CoordinatesCsvExporter.h"
#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) < tol; }

} // namespace

int main() {
    core::Document doc;
    // Parcelle A 1 : 20 x 10, parcourue est, nord, ouest, sud.
    auto first = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{0, 0}, {20, 0}, {20, 10}, {0, 10}});
    first->properties().setString("cadastre.section", "A");
    first->properties().setString("cadastre.numero", "1");
    doc.addEntity(std::move(first));

    auto rows = coordinateRows(doc);
    assert(rows.size() == 4);
    assert(rows[0].parcelle == "A 1" && rows[0].borne == "B1" && rows[0].vers == "B2");
    assert(near(rows[0].gisementGrades, 100.0) && near(rows[0].distance, 20.0));   // vers l'est
    assert(near(rows[1].gisementGrades, 0.0) && near(rows[1].distance, 10.0));     // vers le nord
    assert(near(rows[2].gisementGrades, 300.0));                                   // vers l'ouest
    assert(near(rows[3].gisementGrades, 200.0) && rows[3].vers == "B1");           // vers le sud, ferme
    assert(near(rows[2].x, 20.0) && near(rows[2].y, 10.0));

    // Gisement oblique : 45° vers le nord-est = 50 grades.
    core::Document oblique;
    oblique.addEntity(std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{0, 0}, {10, 10}, {0, 10}}));
    assert(near(coordinateRows(oblique)[0].gisementGrades, 50.0));

    // Une voisine partageant un cote reprend les memes numeros de borne.
    doc.addEntity(std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{20, 0}, {30, 0}, {30, 10}, {20, 10}}));
    rows = coordinateRows(doc);
    assert(rows.size() == 8);
    assert(rows[4].borne == "B2" && rows[4].vers == "B5");
    assert(rows[6].borne == "B6" && rows[6].vers == "B3");
    assert(rows[4].parcelle.rfind("id ", 0) == 0);      // non identifiee : son id

    // Le plan peint les memes libelles.
    const auto furniture = buildSheetFurniture(doc);
    assert(furniture.bornes.size() == 6);
    assert(furniture.bornes[0].text == "B1" && furniture.bornes[5].text == "B6");

    // Meuble du plan : six colonnes par ligne, nombres a la francaise.
    const auto table = buildCoordinatesFurniture(doc, defaultCoordinatesTemplate());
    assert(table.fields.size() == 8 * 6);
    assert(table.gabarit.columnLabels.size() == 6);
    assert(std::get<std::string>(table.fields[1].value.value) == "0,000");
    assert(std::get<std::string>(table.fields[4].value.value) == "100,0000");
    assert(std::get<std::string>(table.fields[5].value.value) == "20,00");

    // CSV pour le tableur : BOM, point-virgule, virgule decimale.
    const std::string csv = CoordinatesCsvExporter::toCsv(doc);
    assert(csv.rfind("\xEF\xBB\xBF" "Parcelle;Borne;X (m);Y (m);Vers;Gisement (gr);Distance (m)\r\n", 0) == 0);
    assert(csv.find("A 1;B1;0,000;0,000;B2;100,0000;20,00\r\n") != std::string::npos);
    assert(csv.find("A 1;B4;0,000;10,000;B1;200,0000;10,00\r\n") != std::string::npos);

    // Un nom de parcelle portant le separateur reste dans sa cellule.
    core::Document piege;
    auto p = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{0, 0}, {1, 0}, {1, 1}});
    p->properties().setString("cadastre.section", "A;B");
    piege.addEntity(std::move(p));
    assert(CoordinatesCsvExporter::toCsv(piege).find("\"A;B\";B1;") != std::string::npos);

    std::printf("Tableau des coordonnees : tests PASSED\n");
    return 0;
}
