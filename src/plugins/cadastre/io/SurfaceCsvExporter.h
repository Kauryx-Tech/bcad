#pragma once

#include "bcad/plugin/FileExporter.h"

#include <string>

namespace bcad::cadastre {

// Calcul de surface par la methode des coordonnees, en CSV (K-05) : pour
// chaque parcelle, une ligne par sommet (borne, X, Y, Y(i+1) - Y(i-1),
// produit), puis 2S, S, les trous deduits et la surface nette. Piece du
// dossier technique en Cote d'Ivoire. Meme ecriture que le tableau des
// coordonnees : point-virgule, virgule decimale, UTF-8 avec BOM.
class SurfaceCsvExporter final : public plugin::IFileExporter {
public:
    std::string id() const override { return "cadastre.calcul_surface"; }
    std::string label() const override { return "Calcul de surface (CSV)"; }
    std::string extension() const override { return "csv"; }
    bool writeDocument(const core::Document& document, const std::string& path,
                       std::string* error) const override;

    static std::string toCsv(const core::Document& document);
};

} // namespace bcad::cadastre
