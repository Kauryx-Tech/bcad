#include "bcad/io/DxfReader.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/io/DxfColor.h"
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace bcad::geom;

namespace bcad::io {

namespace {

struct Group {
    int code;
    std::string value;
};

std::string trim(const std::string& s) {
    std::size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    std::size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<Group> tokenize(std::istream& in) {
    std::vector<Group> groups;
    std::string codeLine, valueLine;
    while (std::getline(in, codeLine) && std::getline(in, valueLine)) {
        codeLine = trim(codeLine);
        if (codeLine.empty()) continue;
        int code = 0;
        try {
            code = std::stoi(codeLine);
        } catch (...) {
            continue;
        }
        groups.push_back({ code, trim(valueLine) });
    }
    return groups;
}

// Curseur sur le flux plat de codes de groupe ; les analyseurs
// d'entités/tables consomment les groupes jusqu'à ce qu'ils rencontrent le
// prochain code 0 (début d'un nouvel enregistrement) ou un terminateur
// reconnu (ENDSEC/ENDTAB/SEQEND).
class Cursor {
public:
    explicit Cursor(const std::vector<Group>& groups) : groups_(groups) {}

    bool atEnd() const { return pos_ >= groups_.size(); }
    const Group& peek() const { return groups_[pos_]; }
    const Group& next() { return groups_[pos_++]; }
    void advance() { ++pos_; }

private:
    const std::vector<Group>& groups_;
    std::size_t pos_ = 0;
};

// XDATA portee par n'importe quelle entite. Deux formes sont acceptees :
//  - BCAD_PROPS, la forme courante, triplets cle / type / valeur ;
//  - BCAD_CADASTRE, la forme publiee avant que le noyau ne sache ecrire que des
//    cles generiques : paires cle / valeur dont le sens cadastral etait impose
//    par l'emetteur. La lire n'est pas maintenir une ecriture : plus rien
//    n'ecrit cet appid, et un fichier deja distribue se recharge sans perdre
//    ses attributs.
// Dans les deux cas le collecteur ne fait que restituer des proprietes : src/io
// ignore ce que les cles veulent dire.
class XDataCollector {
public:
    struct Record {
        std::string key;
        std::string type;
        std::string value;
    };

    void consume(const Group& g) {
        if (g.code == 1001) {
            if (g.value == kPropsAppId) mode_ = Mode::Typed;
            else if (g.value == kLegacyAppId) mode_ = Mode::LegacyPairs;
            else mode_ = Mode::Ignored;
            pending_.clear();
            return;
        }
        if (mode_ == Mode::Ignored) return;
        if (g.code == 1002) {
            if (g.value == "}") mode_ = Mode::Ignored;
            return;
        }
        if (g.code != 1000) return;
        pending_.push_back(g.value);
        const std::size_t arity = mode_ == Mode::Typed ? 3 : 2;
        if (pending_.size() < arity) return;
        if (mode_ == Mode::Typed) {
            records_.push_back({ pending_[0], pending_[1], pending_[2] });
        } else if (!pending_[1].empty()) {
            // Une valeur vide n'etait pas une propriete a l'ecriture.
            records_.push_back({ "cadastre." + pending_[0], "string", pending_[1] });
        }
        pending_.clear();
    }

    void applyTo(geom::Entity& entity) const {
        auto& props = entity.properties();
        for (const auto& rec : records_) {
            // Les valeurs viennent d'un fichier : elles sont analysees sans
            // jamais laisser une entree invalide remonter en exception.
            try {
                if (rec.type == "double") props.setDouble(rec.key, std::stod(rec.value));
                else if (rec.type == "int") props.setInt(rec.key, std::stoi(rec.value));
                else if (rec.type == "bool") props.setBool(rec.key, rec.value == "true");
                else if (rec.type == "color") applyColor(props, rec);
                else props.setString(rec.key, rec.value);
            } catch (const std::exception&) {
                continue;
            }
        }
    }

private:
    enum class Mode { Ignored, Typed, LegacyPairs };

    static void applyColor(properties::PropertyMap& props, const Record& rec) {
        std::istringstream ss(rec.value);
        geom::Color color;
        if (!(ss >> color.r >> color.g >> color.b)) return;
        ss >> color.a;
        props.setColor(rec.key, color);
    }

    static constexpr const char* kPropsAppId = "BCAD_PROPS";
    static constexpr const char* kLegacyAppId = "BCAD_CADASTRE";

    Mode mode_ = Mode::Ignored;
    std::vector<std::string> pending_;
    std::vector<Record> records_;
};

void parseText(Cursor& cur, core::Document& doc) {
    std::string layer = "0";
    std::string text;
    std::optional<int> aci;
    double x = 0, y = 0, height = 2.5, rotDeg = 0;
    XDataCollector xdata;
    while (!cur.atEnd() && cur.peek().code != 0) {
        const Group& g = cur.next();
        if (g.code == 8) layer = g.value;
        else if (g.code == 62) aci = std::stoi(g.value);
        else if (g.code == 10) x = std::stod(g.value);
        else if (g.code == 20) y = std::stod(g.value);
        else if (g.code == 40) height = std::stod(g.value);
        else if (g.code == 50) rotDeg = std::stod(g.value);
        else if (g.code == 1) text = g.value;
        else if (g.code >= 1000 && g.code <= 1071) xdata.consume(g);
    }
    auto entity = std::make_unique<geom::TextEntity>(
        geom::Point2(x, y), std::move(text), height, rotDeg * 3.14159265358979323846 / 180.0);
    xdata.applyTo(*entity);
    entity->setLayer(layer);
    if (aci) entity->setColorOverride(aciToRgb(*aci));
    doc.addEntity(std::move(entity));
}

void parseLwpolyline(Cursor& cur, core::Document& doc) {
    std::vector<geom::Point2> verts;
    std::string layer = "0";
    std::optional<int> aci;
    bool closed = false;
    XDataCollector xdata;
    while (!cur.atEnd() && cur.peek().code != 0) {
        const Group& g = cur.next();
        if (g.code == 8) layer = g.value;
        else if (g.code == 62) aci = std::stoi(g.value);
        else if (g.code == 70) closed = (std::stoi(g.value) & 1) != 0;
        else if (g.code == 10) verts.emplace_back(std::stod(g.value), 0.0);
        else if (g.code == 20 && !verts.empty()) {
            verts.back() = geom::Point2(verts.back().x(), std::stod(g.value));
        } else if (g.code >= 1000 && g.code <= 1071) xdata.consume(g);
    }
    auto entity = std::make_unique<geom::PolylineEntity>(std::move(verts), closed);
    // Le DXF ne porte pas le TypeId d'une entite de module : une parcelle y est
    // une LWPOLYLINE enrichie de XDATA. La promotion est au module, pas a src/io.
    xdata.applyTo(*entity);
    entity->setLayer(layer);
    if (aci) entity->setColorOverride(aciToRgb(*aci));
    doc.addEntity(std::move(entity));
}

void parseOldPolyline(Cursor& cur, core::Document& doc) {
    std::string layer = "0";
    std::optional<int> aci;
    bool closed = false;
    XDataCollector xdata;
    while (!cur.atEnd() && cur.peek().code != 0) {
        const Group& g = cur.next();
        if (g.code == 8) layer = g.value;
        else if (g.code == 62) aci = std::stoi(g.value);
        else if (g.code == 70) closed = (std::stoi(g.value) & 1) != 0;
        else if (g.code >= 1000 && g.code <= 1071) xdata.consume(g);
    }
    std::vector<geom::Point2> verts;
    while (!cur.atEnd() && cur.peek().code == 0 && cur.peek().value == "VERTEX") {
        cur.advance(); // consomme "0 VERTEX"
        double x = 0, y = 0;
        while (!cur.atEnd() && cur.peek().code != 0) {
            const Group& g = cur.next();
            if (g.code == 10) x = std::stod(g.value);
            else if (g.code == 20) y = std::stod(g.value);
        }
        verts.emplace_back(x, y);
    }
    if (!cur.atEnd() && cur.peek().code == 0 && cur.peek().value == "SEQEND") {
        cur.advance();
        while (!cur.atEnd() && cur.peek().code != 0) cur.advance();
    }
    auto entity = std::make_unique<geom::PolylineEntity>(std::move(verts), closed);
    xdata.applyTo(*entity);
    entity->setLayer(layer);
    if (aci) entity->setColorOverride(aciToRgb(*aci));
    doc.addEntity(std::move(entity));
}

void parseEntities(Cursor& cur, core::Document& doc) {
    while (!cur.atEnd()) {
        const Group& head = cur.peek();
        if (head.code == 0 && head.value == "ENDSEC") { cur.advance(); return; }
        if (head.code != 0) { cur.advance(); continue; }

        std::string entType = cur.next().value;

        // Les types a structure propre (portee variable, groupes reutilises)
        // sont dispatches avant la boucle generique, qui consommerait leurs
        // groupes avant qu'ils soient interpretes.
        if (entType == "LWPOLYLINE") { parseLwpolyline(cur, doc); continue; }
        if (entType == "POLYLINE") { parseOldPolyline(cur, doc); continue; }
        if (entType == "TEXT" || entType == "MTEXT") { parseText(cur, doc); continue; }

        std::string layer = "0";
        std::optional<int> aci;
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0, radius = 0, startDeg = 0, endDeg = 360;
        XDataCollector xdata;

        while (!cur.atEnd() && cur.peek().code != 0) {
            const Group& g = cur.next();
            switch (g.code) {
                case 8: layer = g.value; break;
                case 62: aci = std::stoi(g.value); break;
                case 10: x1 = std::stod(g.value); break;
                case 20: y1 = std::stod(g.value); break;
                case 11: x2 = std::stod(g.value); break;
                case 21: y2 = std::stod(g.value); break;
                case 40: radius = std::stod(g.value); break;
                case 50: startDeg = std::stod(g.value); break;
                case 51: endDeg = std::stod(g.value); break;
                default:
                    if (g.code >= 1000 && g.code <= 1071) xdata.consume(g);
                    break;
            }
        }

        std::unique_ptr<geom::Entity> entity;
        if (entType == "LINE") {
            entity = std::make_unique<geom::LineEntity>(geom::Point2(x1, y1), geom::Point2(x2, y2));
        } else if (entType == "CIRCLE") {
            entity = std::make_unique<geom::CircleEntity>(geom::Point2(x1, y1), radius);
        } else if (entType == "ARC") {
            entity = std::make_unique<geom::ArcEntity>(geom::Point2(x1, y1), radius,
                                                        geom::toRadians(startDeg), geom::toRadians(endDeg));
        } else {
            // Type d'entite non pris en charge : ses groupes ont ete consumes
            // ci-dessus, rien n'est ajoute au document.
            continue;
        }
        xdata.applyTo(*entity);
        entity->setLayer(layer);
        if (aci) entity->setColorOverride(aciToRgb(*aci));
        doc.addEntity(std::move(entity));
    }
}

void parseLayerTable(Cursor& cur, core::Document& doc) {
    while (!cur.atEnd()) {
        const Group& head = cur.peek();
        if (head.code == 0 && head.value == "ENDTAB") { cur.advance(); return; }
        if (!(head.code == 0 && head.value == "LAYER")) { cur.advance(); continue; }
        cur.advance();

        std::string name = "0";
        int aci = 7;
        int flags = 0;
        while (!cur.atEnd() && cur.peek().code != 0) {
            const Group& g = cur.next();
            if (g.code == 2) name = g.value;
            else if (g.code == 62) aci = std::stoi(g.value);
            else if (g.code == 70) flags = std::stoi(g.value);
        }
        auto& layer = doc.layerManager().createLayer(name, aciToRgb(std::abs(aci)));
        layer.visible = aci >= 0;
        layer.locked = (flags & 4) != 0;
    }
}

void parseTables(Cursor& cur, core::Document& doc) {
    while (!cur.atEnd()) {
        const Group& head = cur.peek();
        if (head.code == 0 && head.value == "ENDSEC") { cur.advance(); return; }
        if (head.code == 0 && head.value == "TABLE") {
            cur.advance();
            if (!cur.atEnd() && cur.peek().code == 2 && cur.peek().value == "LAYER") {
                cur.advance();
                if (!cur.atEnd() && cur.peek().code == 70) cur.advance(); // nombre de calques
                parseLayerTable(cur, doc);
            }
            continue;
        }
        cur.advance();
    }
}

} // namespace

bool readDxf(const std::string& path, core::Document& outDoc) {
    std::ifstream f(path);
    if (!f) return false;

    std::vector<Group> groups = tokenize(f);
    Cursor cur(groups);
    outDoc.clear();
    outDoc.layerManager().reset();
    core::Document& doc = outDoc;

    while (!cur.atEnd()) {
        const Group& g = cur.peek();
        if (g.code == 0 && g.value == "SECTION") {
            cur.advance();
            if (cur.atEnd() || cur.peek().code != 2) continue;
            std::string section = cur.next().value;
            if (section == "TABLES") parseTables(cur, doc);
            else if (section == "ENTITIES") parseEntities(cur, doc);
            else {
                while (!cur.atEnd() && !(cur.peek().code == 0 && cur.peek().value == "ENDSEC")) cur.advance();
                if (!cur.atEnd()) cur.advance();
            }
        } else {
            cur.advance();
        }
    }
    return true;
}

} // namespace bcad::io
