#include "SurfaceCsvExporter.h"
#include "CsvCell.h"

#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"

#include <fstream>

namespace bcad::cadastre {

std::string SurfaceCsvExporter::toCsv(const core::Document& document) {
    std::string out = "\xEF\xBB\xBF";
    out += "Parcelle;Borne;X (m);Y (m);Y(i+1) - Y(i-1);X × (Y(i+1) - Y(i-1))\r\n";
    bool first = true;
    for (const auto& calc : surfaceComputations(document)) {
        if (!first) out += "\r\n";   // une ligne vide entre deux parcelles
        first = false;
        const std::string nom = csvCell(calc.parcelle);
        for (const auto& step : calc.steps) {
            out += nom + ';' + step.borne + ';' + formatDecimal(step.x, 3) + ';' +
                   formatDecimal(step.y, 3) + ';' + formatDecimal(step.deltaY, 3) + ';' +
                   formatDecimal(step.produit, 4) + "\r\n";
        }
        out += nom + ";;;;2S;" + formatDecimal(calc.doubleArea, 4) + "\r\n";
        out += nom + ";;;;S (m²);" + formatDecimal(calc.outerArea, 2) + "\r\n";
        if (calc.holesArea > 0.0) {
            out += nom + ";;;;Trous (m²);-" + formatDecimal(calc.holesArea, 2) + "\r\n";
            out += nom + ";;;;Surface nette (m²);" + formatDecimal(calc.netArea, 2) + "\r\n";
        }
    }
    return out;
}

bool SurfaceCsvExporter::writeDocument(const core::Document& document, const std::string& path,
                                       std::string* error) const {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        if (error) *error = "écriture impossible : " + path;
        return false;
    }
    file << toCsv(document);
    if (!file) {
        if (error) *error = "écriture interrompue : " + path;
        return false;
    }
    return true;
}

} // namespace bcad::cadastre
