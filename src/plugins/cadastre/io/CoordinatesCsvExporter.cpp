#include "CoordinatesCsvExporter.h"

#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"

#include <fstream>

namespace bcad::cadastre {

namespace {

// Un champ texte ne doit pas casser la ligne : guillemets doubles s'il porte
// le separateur, un guillemet ou un saut de ligne.
std::string cell(const std::string& text) {
    if (text.find_first_of(";\"\n\r") == std::string::npos) return text;
    std::string quoted = "\"";
    for (char c : text) quoted += (c == '"') ? std::string("\"\"") : std::string(1, c);
    return quoted + "\"";
}

} // namespace

std::string CoordinatesCsvExporter::toCsv(const core::Document& document) {
    std::string out = "\xEF\xBB\xBF";   // BOM : le tableur reconnait l'UTF-8
    out += "Parcelle;Borne;X (m);Y (m);Vers;Gisement (gr);Distance (m)\r\n";
    for (const auto& row : coordinateRows(document)) {
        out += cell(row.parcelle) + ';' + row.borne + ';' + formatDecimal(row.x, 3) + ';' +
               formatDecimal(row.y, 3) + ';' + row.vers + ';' +
               formatDecimal(row.gisementGrades, 4) + ';' + formatDecimal(row.distance, 2) + "\r\n";
    }
    return out;
}

bool CoordinatesCsvExporter::writeDocument(const core::Document& document,
                                           const std::string& path,
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
