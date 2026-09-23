#include "bcad/io/DxfReader.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/io/DxfColor.h"
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

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

namespace {
    const char* kCadastreAppId = "BCAD_CADASTRE";

    void parseXData(Cursor& cur, geom::PolylineEntity& entity) {
        std::string currentKey;
        bool inCadastreApp = false;
        while (!cur.atEnd() && cur.peek().code != 0) {
            const Group& g = cur.next();
            if (g.code == 1001) {
                inCadastreApp = (g.value == "BCAD_CADASTRE");
            } else if (inCadastreApp) {
                if (g.code == 1002 && g.value == "}") {
                    break; // fin XDATA cadastre
                } else if (g.code == 1000) {
                    if (currentKey.empty()) {
                        currentKey = g.value;
                    } else {
                        // valeur pour currentKey
                        if (auto* prop = entity.properties().get("cadastre." + currentKey)) {
                            prop->setFromString(g.value);
                        }
                        currentKey.clear();
                    }
                }
            }
        }
    }
}

void parseLwpolyline(Cursor& cur, core::Document& doc, const std::string& layer, std::optional<int> aci) {
    std::vector<geom::Point2> verts;
    bool closed = false;
    while (!cur.atEnd() && cur.peek().code != 0) {
        const Group& g = cur.next();
        if (g.code == 70) closed = (std::stoi(g.value) & 1) != 0;
        else if (g.code == 10) {
            verts.emplace_back(std::stod(g.value), 0.0);
        } else if (g.code == 20 && !verts.empty()) {
            verts.back() = geom::Point2(verts.back().x(), std::stod(g.value));
        } else if (g.code == 1001 && g.value == "BCAD_CADASTRE") {
            // XDATA cadastre détecté - parser l'XDATA
            // reculer d'un pas pour que parseXData voie le 1001
            cur.advance();
            // Note: on ne peut pas reculer, donc on parse directement ici
            // En fait, on a déjà consommé le 1001, donc on continue directement
        }
    }
    auto entity = std::make_unique<geom::PolylineEntity>(std::move(verts), closed);
    // Parser l'XDATA après la polyligne (le curseur est maintenant sur l'XDATA ou le prochain code 0)
    parseXData(cur, *entity);
    entity->setLayer(layer);
    if (aci) entity->setColorOverride(aciToRgb(*aci));
    doc.addEntity(std::move(entity));
}

void parseOldPolyline(Cursor& cur, core::Document& doc, const std::string& layer, std::optional<int> aci) {
    bool closed = false;
    while (!cur.atEnd() && cur.peek().code != 0) {
        const Group& g = cur.next();
        if (g.code == 70) closed = (std::stoi(g.value) & 1) != 0;
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
        std::string layer = "0";
        std::optional<int> aci;
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0, radius = 0, startDeg = 0, endDeg = 360;
        bool haveX1 = false, haveY1 = false, haveX2 = false, haveY2 = false;

        if (entType == "LWPOLYLINE") { parseLwpolyline(cur, doc, layer, aci); continue; }
        if (entType == "POLYLINE") { parseOldPolyline(cur, doc, layer, aci); continue; }

        while (!cur.atEnd() && cur.peek().code != 0) {
            const Group& g = cur.next();
            switch (g.code) {
                case 8: layer = g.value; break;
                case 62: aci = std::stoi(g.value); break;
                case 10: x1 = std::stod(g.value); haveX1 = true; break;
                case 20: y1 = std::stod(g.value); haveY1 = true; break;
                case 11: x2 = std::stod(g.value); haveX2 = true; break;
                case 21: y2 = std::stod(g.value); haveY2 = true; break;
                case 40: radius = std::stod(g.value); break;
                case 50: startDeg = std::stod(g.value); break;
                case 51: endDeg = std::stod(g.value); break;
                default: break;
            }
        }
        (void)haveX1; (void)haveY1; (void)haveX2; (void)haveY2;

        std::unique_ptr<geom::Entity> entity;
        if (entType == "LINE") {
            entity = std::make_unique<geom::LineEntity>(geom::Point2(x1, y1), geom::Point2(x2, y2));
        } else if (entType == "CIRCLE") {
            entity = std::make_unique<geom::CircleEntity>(geom::Point2(x1, y1), radius);
        } else if (entType == "ARC") {
            entity = std::make_unique<geom::ArcEntity>(geom::Point2(x1, y1), radius,
                                                         geom::toRadians(startDeg), geom::toRadians(endDeg));
        } else {
            continue; // type d'entité non pris en charge : ignoré silencieusement
        }
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
