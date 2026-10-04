// Contenance calculee contre contenance declaree (K-09).
//
// La contenance etait une chaine libre que rien ne comparait a la surface du
// contour, et la tolerance de leve du gabarit (`survey_tolerance.default_m`)
// n'etait lue par personne. La regle `cadastre.contenance` les relie : ecart
// admis = perimetre (contour et trous) x tolerance lineaire du profil.

#include "ParcelOps.h"
#include "Templates.h"
#include "entities/ParcelEntity.h"
#include "validation/CadastreValidators.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

} // namespace

int main() {
    // --- Lecture des contenances ecrites dans un acte ---
    assert(near(*parseContenance("1 250,50 m²"), 1250.5));
    assert(near(*parseContenance("1250.5 m2"), 1250.5));
    assert(near(*parseContenance("50m²"), 50.0));
    assert(near(*parseContenance("2,35 ha"), 23500.0));
    assert(near(*parseContenance("2 ha 3 a 50 ca"), 20350.0));
    assert(near(*parseContenance("12 a 50 ca"), 1250.0));
    assert(near(*parseContenance("  842 "), 842.0));          // nombre seul : m²
    assert(near(*parseContenance("1 HA"), 10000.0));          // casse indifferente
    assert(!parseContenance(""));
    assert(!parseContenance("environ cent metres"));
    assert(!parseContenance("12 km"));
    assert(!parseContenance("ha"));

    // --- Ecriture en m² a la francaise ---
    assert(formatSquareMetres(1250.5) == "1 250,50 m²");
    assert(formatSquareMetres(7.0) == "7,00 m²");
    assert(formatSquareMetres(1234567.891) == "1 234 567,89 m²");

    // --- Regle : parcelle 20 x 10 = 200 m², perimetre 60 m ---
    CadastreTemplates profil;
    profil.profile = "essai";
    profil.surveyToleranceM = 0.02;                           // ecart admis 1,2 m²
    ParcelAreaRuleValidator regle(profil);
    assert(regle.id() == "cadastre.contenance");
    assert(regle.label().find("essai") != std::string::npos);

    ParcelEntity parcel({{0, 0}, {20, 0}, {20, 10}, {0, 10}});
    std::vector<geom::Entity*> lot{&parcel};

    // Non declaree : information, avec la valeur calculee.
    auto diags = regle.validate(lot);
    assert(diags.size() == 1 && diags[0].severity == validation::Severity::Info);
    assert(diags[0].message.find("200,00 m²") != std::string::npos);

    // Dans la tolerance : rien a signaler.
    parcel.properties().setString("cadastre.contenance", "201 m²");
    assert(regle.validate(lot).empty());

    // Au-dela : avertissement chiffre.
    parcel.properties().setString("cadastre.contenance", "2 a 5 ca");   // 205 m²
    diags = regle.validate(lot);
    assert(diags.size() == 1 && diags[0].severity == validation::Severity::Warning);
    assert(diags[0].message.find("205,00 m²") != std::string::npos);
    assert(diags[0].message.find("5,00 m²") != std::string::npos);
    assert(diags[0].entityIds == std::vector<int>{parcel.id()});

    // Illisible : avertissement, la valeur n'est pas devinee.
    parcel.properties().setString("cadastre.contenance", "deux cents");
    diags = regle.validate(lot);
    assert(diags.size() == 1 && diags[0].severity == validation::Severity::Warning);
    assert(diags[0].message.find("illisible") != std::string::npos);

    // Le profil change la tolerance en cours de dossier.
    parcel.properties().setString("cadastre.contenance", "205 m²");
    CadastreTemplates large = profil;
    large.surveyToleranceM = 0.10;                            // ecart admis 6 m²
    regle.appliquerProfil(large);
    assert(regle.validate(lot).empty());
    regle.appliquerProfil(profil);
    assert(!regle.validate(lot).empty());

    // Trous : surface nette, et le perimetre des trous compte dans l'ecart admis.
    ParcelEntity troue({{0, 0}, {20, 0}, {20, 10}, {0, 10}});
    troue.addHole({{5, 2}, {10, 2}, {10, 6}, {5, 6}});        // -20 m², perimetre +18 m
    troue.properties().setString("cadastre.contenance", "180,5 m²");
    std::vector<geom::Entity*> lotTroue{&troue};
    assert(regle.validate(lotTroue).empty());                 // ecart 0,5 <= 78 x 0,02 = 1,56

    // --- La tolerance vient du gabarit Togo livre avec le module ---
    memoriserRepertoiresDeDonnees({BCAD_CADASTRE_DATA_DIR});
    const CadastreTemplates togo = chargerGabaritDuProfil("cadastre_togo");
    assert(!togo.source.empty());
    assert(near(togo.surveyToleranceM, 0.02));
    memoriserRepertoiresDeDonnees({});

    std::printf("Contenance : tests PASSED\n");
    return 0;
}
