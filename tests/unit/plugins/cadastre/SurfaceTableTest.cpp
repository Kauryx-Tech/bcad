// Calcul de surface par la methode des coordonnees (K-05), et protection des
// exports CSV contre l'injection de formules.
//
// 2S = somme de X(i) x (Y(i+1) - Y(i-1)) ; les trous sont calcules de meme et
// deduits. Un nom de parcelle commencant par = + - @ serait execute par le
// tableur a l'ouverture du CSV : il doit arriver comme du texte.

#include "entities/ParcelEntity.h"
#include "io/CoordinatesCsvExporter.h"
#include "io/SurfaceCsvExporter.h"
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

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

} // namespace

int main() {
    core::Document doc;
    // Parcelle 20 x 10 (200 m²) parcourue dans le sens trigonometrique, avec
    // un trou de 5 x 4 (20 m²).
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{0, 0}, {20, 0}, {20, 10}, {0, 10}});
    parcel->addHole({{5, 2}, {10, 2}, {10, 6}, {5, 6}});
    parcel->properties().setString("cadastre.section", "A");
    parcel->properties().setString("cadastre.numero", "7");
    doc.addEntity(std::move(parcel));

    auto calcs = surfaceComputations(doc);
    assert(calcs.size() == 1);
    const auto& calc = calcs.front();
    assert(calc.parcelle == "A 7");
    assert(calc.steps.size() == 4);
    // Sommet B2 (20, 0) : Y(B3) - Y(B1) = 10 - 0 ; produit 20 x 10 = 200.
    assert(calc.steps[1].borne == "B2");
    assert(near(calc.steps[1].deltaY, 10.0) && near(calc.steps[1].produit, 200.0));
    // Sommet B4 (0, 10) : produit nul.
    assert(near(calc.steps[3].produit, 0.0));
    assert(near(calc.doubleArea, 400.0));      // 2S, positif dans ce sens de parcours
    assert(near(calc.outerArea, 200.0));
    assert(near(calc.holesArea, 20.0));
    assert(near(calc.netArea, 180.0));

    // Sens horaire : 2S negatif, surface identique.
    core::Document horaire;
    horaire.addEntity(std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{0, 0}, {0, 10}, {20, 10}, {20, 0}}));
    const auto inverse = surfaceComputations(horaire).front();
    assert(near(inverse.doubleArea, -400.0) && near(inverse.outerArea, 200.0));

    // CSV du calcul : lignes par sommet, puis 2S, S, trous et surface nette.
    const std::string csv = SurfaceCsvExporter::toCsv(doc);
    assert(csv.rfind("\xEF\xBB\xBF" "Parcelle;Borne;X (m);Y (m);", 0) == 0);
    assert(csv.find("A 7;B2;20,000;0,000;10,000;200,0000\r\n") != std::string::npos);
    assert(csv.find("A 7;;;;2S;400,0000\r\n") != std::string::npos);
    assert(csv.find("A 7;;;;S (m²);200,00\r\n") != std::string::npos);
    assert(csv.find("A 7;;;;Trous (m²);-20,00\r\n") != std::string::npos);
    assert(csv.find("A 7;;;;Surface nette (m²);180,00\r\n") != std::string::npos);

    // Injection de formule : la cellule arrive en texte, dans les deux exports.
    core::Document piege;
    auto p = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{0, 0}, {1, 0}, {1, 1}});
    p->properties().setString("cadastre.section", "=HYPERLINK(\"http://x\")");
    piege.addEntity(std::move(p));
    const std::string coords = CoordinatesCsvExporter::toCsv(piege);
    const std::string surfaces = SurfaceCsvExporter::toCsv(piege);
    for (const std::string* text : {&coords, &surfaces}) {
        assert(text->find("\r\n=") == std::string::npos);            // jamais de « = » en tete de ligne
        assert(text->find("\"'=HYPERLINK(\"\"http://x\"\")\";B1") != std::string::npos);
    }
    for (const char* risque : {"+1", "-1", "@SUM(A1)"}) {
        core::Document d;
        auto q = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{{0, 0}, {1, 0}, {1, 1}});
        q->properties().setString("cadastre.section", risque);
        d.addEntity(std::move(q));
        assert(CoordinatesCsvExporter::toCsv(d).find(std::string("\r\n'") + risque) != std::string::npos);
    }

    std::printf("Calcul de surface et CSV : tests PASSED\n");
    return 0;
}
