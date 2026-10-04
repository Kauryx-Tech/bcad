#pragma once

#include "bcad/plugin/FileExporter.h"

#include <string>

namespace bcad::cadastre {

// Tableau des coordonnees des bornes en CSV (K-04) : une ligne par cote de
// parcelle — parcelle, borne, X, Y, borne suivante, gisement (grades),
// distance. Ecrit « a la francaise » pour un tableur francophone : separateur
// point-virgule, virgule decimale, UTF-8 avec BOM. Memes lignes et memes
// numeros de borne que le tableau du plan (`coordinateRows`).
class CoordinatesCsvExporter final : public plugin::IFileExporter {
public:
    std::string id() const override { return "cadastre.coordonnees"; }
    std::string label() const override { return "Tableau des coordonnées (CSV)"; }
    std::string extension() const override { return "csv"; }
    bool writeDocument(const core::Document& document, const std::string& path,
                       std::string* error) const override;

    // Le texte du fichier, sans l'ecrire : ce que le test verifie.
    static std::string toCsv(const core::Document& document);
};

} // namespace bcad::cadastre
