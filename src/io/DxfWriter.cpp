#include "bcad/core/Document.h"
#include "bcad/geometry/Entity.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"
#include "bcad/io/DxfColor.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace bcad::io {

namespace {

// Une seule application XDATA pour tout ce que BCAD ajoute a une entite : les
// proprietes de son PropertyMap. Le noyau ecrit des noms de cles et des types,
// jamais ce qu'ils signifient : n'importe quelle cle, de n'importe quel module,
// passe par ce chemin sans que src/io sache qu'elle est metier. La garde de
// check_arch.sh sur src/io serait violee par un simple exemple de cle ici.
constexpr const char* kPropsAppId = "BCAD_PROPS";

// Le DXF est un format a lignes : un 1000 qui en contiendrait une produirait un
// fichier que notre propre lecteur reinterpreterait. Remplacer plutot que
// jeter la propriete.
std::string asOneLine(const std::string& text) {
    std::string out = text;
    for (auto& ch : out)
        if (ch == '\n' || ch == '\r') ch = ' ';
    return out;
}

const char* typeTag(properties::PropertyType type) {
    using properties::PropertyType;
    switch (type) {
        case PropertyType::Double: return "double";
        case PropertyType::Int: return "int";
        case PropertyType::String: return "string";
        case PropertyType::Bool: return "bool";
        case PropertyType::Color: return "color";
        // Un enum s'exporte comme la chaine qu'il represente : la liste des
        // valeurs possibles est du schema, donc elle appartient au module qui
        // l'a declaree, pas au fichier d'change. Ecrire un index tout en
        // ignorant ce schema produirait une valeur hors domaine.
        case PropertyType::Enum: return "string";
    }
    return "string";
}

std::string valueOf(const properties::Property& prop) {
    using properties::PropertyType;
    std::ostringstream ss;
    ss.precision(17);
    switch (prop.type()) {
        case PropertyType::Double: return (ss << prop.asDouble(), ss.str());
        case PropertyType::Int: return std::to_string(prop.asInt());
        case PropertyType::String: return prop.asString();
        case PropertyType::Bool: return prop.asBool() ? "true" : "false";
        case PropertyType::Enum: {
            const auto& values = prop.enumValues();
            const int index = prop.asEnum();
            if (index >= 0 && static_cast<std::size_t>(index) < values.size()) return values[index];
            return std::to_string(index);
        }
        case PropertyType::Color: {
            const auto c = prop.asColor();
            return (ss << c.r << " " << c.g << " " << c.b << " " << c.a, ss.str());
        }
    }
    return "";
}

void writeAppIdTable(std::ostream& f) {
    f << "0\nTABLE\n2\nAPPID\n";
    f << "70\n1\n";
    f << "0\nAPPID\n";
    f << "2\n" << kPropsAppId << "\n";
    f << "70\n0\n";
    f << "0\nENDTAB\n";
}

// Types de ligne des calques (L-01), sous les noms d'acad.lin pour qu'AutoCAD
// les reconnaisse ; la table LTYPE les definit (tiret, blanc, point en unites
// du dessin), un calque ne pouvant citer qu'un type defini.
const char* lineTypeName(layers::Layer::LineType type) {
    switch (type) {
        case layers::Layer::LineType::Dashed: return "DASHED";
        case layers::Layer::LineType::Dotted: return "DOT";
        case layers::Layer::LineType::DashDot: return "DASHDOT";
        case layers::Layer::LineType::Continuous: break;
    }
    return "CONTINUOUS";
}

void writeLineTypeTable(std::ostream& f) {
    f << "0\nTABLE\n2\nLTYPE\n70\n4\n";
    f << "0\nLTYPE\n2\nCONTINUOUS\n70\n0\n3\nSolid line\n72\n65\n73\n0\n40\n0.0\n";
    f << "0\nLTYPE\n2\nDASHED\n70\n0\n3\nDashed __ __ __\n72\n65\n73\n2\n40\n0.75\n"
         "49\n0.5\n74\n0\n49\n-0.25\n74\n0\n";
    f << "0\nLTYPE\n2\nDOT\n70\n0\n3\nDot . . .\n72\n65\n73\n2\n40\n0.25\n"
         "49\n0.0\n74\n0\n49\n-0.25\n74\n0\n";
    f << "0\nLTYPE\n2\nDASHDOT\n70\n0\n3\nDash dot __ . __\n72\n65\n73\n4\n40\n1.0\n"
         "49\n0.5\n74\n0\n49\n-0.25\n74\n0\n49\n0.0\n74\n0\n49\n-0.25\n74\n0\n";
    f << "0\nENDTAB\n";
}

// Triplets cle / type / valeur : le type est une donnee exportee, pas une
// supposition du lecteur. Sans lui, un reel de 1250.42 reviendrait en chaine.
void writeProperties(std::ostream& f, const geom::Entity& entity) {
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

} // namespace

// Forward declaration
bool writeDxfToStream(const core::Document& doc, std::ostream& f);

bool writeDxf(const std::string& path, const core::Document& doc) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    return writeDxfToStream(doc, f);
}

bool writeDxfToStream(const core::Document& doc, std::ostream& f) {
    f.precision(9);

    f << "0\nSECTION\n2\nHEADER\n";
    f << "9\n$ACADVER\n1\nAC1015\n";
    f << "0\nENDSEC\n";

    f << "0\nSECTION\n2\nTABLES\n";
    writeLineTypeTable(f);
    f << "0\nTABLE\n2\nLAYER\n";
    auto& layers = doc.layerManager().layers();
    f << "70\n" << layers.size() << "\n";
    for (const auto& layer : layers) {
        f << "0\nLAYER\n";
        f << "2\n" << layer.name << "\n";
        f << "70\n" << (layer.locked ? 4 : 0) << "\n";
        // Couleur la plus proche de la palette ACI (avant : seuls les gris
        // etaient convertis, un calque rouge sortait blanc) ; negative si
        // le calque est eteint. Type de ligne et epaisseur (370, en 1/100 mm).
        const int aci = rgbToAci(layer.color);
        f << "62\n" << (layer.visible ? aci : -aci) << "\n";
        f << "6\n" << lineTypeName(layer.lineType) << "\n";
        f << "370\n" << static_cast<int>(std::lround(layer.lineWeight * 100.0)) << "\n";
    }
    f << "0\nENDTAB\n";
    writeAppIdTable(f);
    f << "0\nENDSEC\n";

    f << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& e : doc.entities()) {
        const auto before = f.tellp();
        e->writeDxf(f, e->layer(), e->colorOverride());
        if (f.tellp() != before) writeProperties(f, *e);
    }
    f << "0\nENDSEC\n";

    f << "0\nEOF\n";

    return f.good();
}

} // namespace bcad::io
