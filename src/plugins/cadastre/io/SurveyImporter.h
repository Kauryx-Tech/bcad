#pragma once

#include "bcad/plugin/FileImporter.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::cadastre {

// Un point d'un carnet de leve : matricule, X, Y, Z facultatif, code terrain.
struct SurveyPoint {
    std::string name;
    double x = 0.0;
    double y = 0.0;
    std::optional<double> z;
    std::string code;
};

struct SurveyParse {
    std::vector<SurveyPoint> points;
    // Lignes ecartees, deja redigees pour l'operateur : « ligne 7 : X illisible ».
    std::vector<std::string> rejects;
};

// Lecture tolerante des exports courants de station totale et de GNSS :
// `matricule X Y [Z] [code]`, separes par point-virgule, tabulation, virgule ou
// espaces ; virgule decimale admise sauf quand la virgule separe les champs.
// Les lignes vides, les commentaires (# ou //) et une ligne d'en-tete sont
// ignores sans etre signales ; toute autre ligne illisible est signalee, jamais
// devinee.
SurveyParse parseSurveyText(std::string_view text);

// Import d'un carnet de leve en bornes (K-03) : un point = une borne, matricule
// en reference, altitude et code terrain gardes ; le code donne la nature de la
// borne (BORNE, REP, PI, STATION). Des lignes ecartees n'empechent pas l'import
// des autres : elles sont rapportees dans `error` avec un retour vrai.
class SurveyImporter final : public plugin::IFileImporter {
public:
    std::string id() const override { return "cadastre.leve"; }
    std::string label() const override { return "Points de levé (CSV, TXT)"; }
    std::string extensions() const override { return "csv txt"; }
    bool readDocument(core::Document& document, const std::string& path,
                      std::string* error) const override;
};

} // namespace bcad::cadastre
