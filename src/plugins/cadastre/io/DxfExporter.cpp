#include "DxfExporter.h"

#include "layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/layout/Composition.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include "bcad/layout/PdfExport.h"
#include "bcad/layout/Label.h"
#include "bcad/layout/Borne.h"
#include "bcad/layout/NorthArrow.h"
#include "bcad/layout/Scale.h"
// #include "bcad/layout/ParcelTable.h"  // Removed - part of old Cartouche API

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace bcad::cadastre {

namespace {

// Couleurs ACI pour calques cadastres
constexpr int ACI_CADASTRE = 3;   // Vert
constexpr int ACI_COTATION = 1;   // Rouge
constexpr int ACI_CARTOUCHE = 7;  // Blanc/Noir

// Calques requis pour export DXF cadastral
const std::vector<std::tuple<std::string, int, std::string>> kCadastreLayers = {
    {"CADASTRE", ACI_CADASTRE, "CONTINUOUS"},
    {"COTATION", ACI_COTATION, "CONTINUOUS"},
    {"CARTOUCHE", ACI_CARTOUCHE, "CONTINUOUS"},
    {"BORNE", 5, "CONTINUOUS"},
    {"ETIQUETTE", 6, "CONTINUOUS"},
};

constexpr const char* kPropsAppId = "BCAD_PROPS";

std::string asOneLine(const std::string& text) {
    std::string out = text;
    for (auto& ch : out)
        if (ch == '\n' || ch == '\r') ch = ' ';
    return out;
}

void writeAppIdTable(std::ostream& f) {
    f << "0\nTABLE\n2\nAPPID\n";
    f << "70\n1\n";
    f << "0\nAPPID\n";
    f << "2\n" << kPropsAppId << "\n";
    f << "70\n0\n";
    f << "0\nENDTAB\n";
}

std::string typeTag(bcad::properties::PropertyType type) {
    using bt = bcad::properties::PropertyType;
    switch (type) {
        case bt::Double: return "double";
        case bt::Int: return "int";
        case bt::String: return "string";
        case bt::Bool: return "bool";
        case bt::Enum: return "string";
        case bt::Color: return "color";
    }
    return "string";
}

std::string valueOf(const bcad::properties::Property& prop) {
    using bt = bcad::properties::PropertyType;
    std::ostringstream ss;
    ss.precision(17);
    switch (prop.type()) {
        case bt::Double: return (ss << prop.asDouble(), ss.str());
        case bt::Int: return std::to_string(prop.asInt());
        case bt::String: return prop.asString();
        case bt::Bool: return prop.asBool() ? "true" : "false";
        case bt::Enum: {
            const auto& values = prop.enumValues();
            const int index = prop.asEnum();
            if (index >= 0 && static_cast<std::size_t>(index) < values.size())
                return values[static_cast<std::size_t>(index)];
            return std::to_string(index);
        }
        case bt::Color: {
            const auto c = prop.asColor();
            std::ostringstream ss;
            ss << c.r << " " << c.g << " " << c.b << " " << c.a;
            return ss.str();
        }
    }
    return "";
}

void writeProperties(std::ostream& f, const bcad::geom::Entity& entity) {
    const auto& props = entity.properties();
    const auto names = props.listNames();
    if (names.empty()) return;

    f << "1001\n" << kPropsAppId << "\n";
    f << "1002\n{\n";
    for (const auto& name : names) {
        const auto* prop = props.get(name);
        if (!prop) continue;
        f << "1000\n" << asOneLine(name) << "\n";
        f << "1000\n" << typeTag(prop->type()) << "\n";
        f << "1000\n" << asOneLine(valueOf(*prop)) << "\n";
    }
    f << "1002\n}\n";
}

// Couleur vers ACI
int colorToAci(const bcad::geom::Color& c) {
    int r = static_cast<int>(c.r * 255);
    int g = static_cast<int>(c.g * 255);
    int b = static_cast<int>(c.b * 255);
    if (r == g && g == b) return std::clamp(r / 8, 1, 255);
    return 7;
}

void writeLayerTable(std::ostream& f, const bcad::layers::LayerManager& lm,
                     const std::vector<std::tuple<std::string,int,std::string>>& extra) {
    std::size_t total = lm.layers().size() + extra.size();
    f << "0\nTABLE\n2\nLAYER\n";
    f << "70\n" << total << "\n";
    for (const auto& layer : lm.layers()) {
        int r = static_cast<int>(layer.color.r * 255);
        int g = static_cast<int>(layer.color.g * 255);
        int b = static_cast<int>(layer.color.b * 255);
        int aci = colorToAci(layer.color);
        f << "0\nLAYER\n2\n" << layer.name << "\n";
        f << "70\n" << (layer.locked ? 4 : 0) << "\n";
        f << "62\n" << (layer.visible ? aci : -aci) << "\n";
        f << "6\n" << static_cast<int>(layer.lineType) << "\n";
    }
    for (const auto& [name, aci, linetype] : extra) {
        f << "0\nLAYER\n2\n" << name << "\n";
        f << "70\n0\n";
        f << "62\n" << aci << "\n";
        f << "6\n" << linetype << "\n";
    }
    f << "0\nENDTAB\n";
}

void writeAppIdTableCadastre(std::ostream& f) {
    f << "0\nTABLE\n2\nAPPID\n";
    f << "70\n1\n";
    f << "0\nAPPID\n";
    f << "2\n" << kPropsAppId << "\n";
    f << "70\n0\n";
    f << "0\nENDTAB\n";
}

} // namespace

bool DxfExporter::writeDocument(const core::Document& document,
                                const std::string& path,
                                std::string* error) const {
    // Construire la feuille via le module
    layout::Sheet sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
    layout::Viewport viewport;
    const auto bbox = document.extents();
    if (!bbox.isValid()) {
        if (error) *error = "Document vide : aucune étendue";
        return false;
    }
    viewport.setSource(bbox);

    // Meubles (étiquettes, bornes, nomenclature) via le module
    cadastre::SheetFurniture furniture = buildSheetFurniture(document);

    // Cartouche
    std::vector<bcad::validation::Diagnostic> diagnostics;
    layout::ResolvedFurniture cartouche = buildCartoucheFurniture(document,
        layout::FurnitureTemplate(), diagnostics);
    // Pas de gabarit pour l'export DXF simple : on construit un cartouche par défaut
    layout::FurnitureTemplate defaultCartouche = defaultCartoucheTemplate();
    for (auto& attendu : defaultCartouche.fields) {
        if (attendu.key.empty())
            attendu.literal = "1:500"; // échelle par défaut
    }
    std::vector<bcad::validation::Diagnostic> diags;
    layout::ResolvedFurniture resolvedCartouche = buildCartoucheFurniture(document, defaultCartouche, diags);

    // Composition
    layout::PdfExportOptions opts;
    opts.sheet = sheet;
    opts.viewport = viewport;
    opts.document = &document;
    opts.labels = std::move(furniture.labels);
    opts.bornes = std::move(furniture.bornes);
    opts.meubles.push_back(resolvedCartouche);
    opts.meubles.push_back(buildNomenclatureFurniture(document, defaultNomenclatureTemplate()));
    opts.permittedScales = defaultPermittedScales();
    layout::applyFittingScale(opts);

    // Écriture DXF
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) {
        if (error) *error = "Impossible d'ouvrir le fichier : " + path;
        return false;
    }

    f.precision(9);
    f << "0\nSECTION\n2\nHEADER\n";
    f << "9\n$ACADVER\n1\nAC1015\n";
    f << "0\nENDSEC\n";

    // Tables
    f << "0\nSECTION\n2\nTABLES\n";
    // Calques
    writeLayerTable(f, document.layerManager(), kCadastreLayers);
    // AppID
    writeAppIdTableCadastre(f);
    f << "0\nENDSEC\n";

    // Entités
    f << "0\nSECTION\n2\nENTITIES\n";

    // 1. Plan (entités du document)
    for (const auto& e : document.entities()) {
        const auto before = f.tellp();
        e->writeDxf(f, e->layer(), e->colorOverride());
        if (f.tellp() != before) writeProperties(f, *e);
    }

    // 2. Cartouche (en calque CARTOUCHE) - via composition
    // On ne peut pas écrire directement le cartouche ici sans réimplémenter drawCartouche
    // Pour la version minimale : écrire les entités du document sur calque CADASTRE
    // et laisser les meubles pour une version complète

    // Bornes
    for (const auto& borne : furniture.bornes) {
        // Borne = cercle + texte (numéro)
        // Simplification : point entity + texte
    }

    // 3. Nomenclature (tableau parcellaire)
    // 4. Flèche Nord
    // 5. Barre d'échelle

    // Pour la version MVP : écrire les entités du document avec leurs calques
    // Les calques spécifiques cadastres sont déjà dans la table LAYER

    f << "0\nENDSEC\n";
    f << "0\nEOF\n";

    return f.good();
}

} // namespace bcad::cadastre