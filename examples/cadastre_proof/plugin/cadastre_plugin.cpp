#include "bcad/plugin/PluginRegistry.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/commands/Command.h"
#include "bcad/serialization/Serializer.h"
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// MODULE METIER CADASTRE : entité parcelle complète avec propriétés métier.
// Parcelle = polygone fermé + section/numéro/contenance/commune
// Validation : >=3 sommets, aire > 0, non auto-intersectante (simple)
// ---------------------------------------------------------------------------
namespace {

// ── Helpers géométriques ────────────────────────────────────────────────
double signedArea(const std::vector<bcad::geom::Point2>& v) {
    double a = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        auto& p = v[i];
        auto& q = v[(i + 1) % v.size()];
        a += (p.x_ * q.y_ - q.x_ * p.y_);
    }
    return a * 0.5;
}

bool segmentsIntersect(bcad::geom::Point2 a, bcad::geom::Point2 b,
                       bcad::geom::Point2 c, bcad::geom::Point2 d) {
    auto cross = [](bcad::geom::Point2 o, bcad::geom::Point2 p, bcad::geom::Point2 q) {
        return (p.x_ - o.x_) * (q.y_ - o.y_) - (p.y_ - o.y_) * (q.x_ - o.x_);
    };
    auto onSeg = [](bcad::geom::Point2 p, bcad::geom::Point2 q, bcad::geom::Point2 r) {
        return q.x_ <= std::max(p.x_, r.x_) && q.x_ >= std::min(p.x_, r.x_) &&
               q.y_ <= std::max(p.y_, r.y_) && q.y_ >= std::min(p.y_, r.y_);
    };
    double d1 = cross(c, d, a), d2 = cross(c, d, b);
    double d3 = cross(a, b, c), d4 = cross(a, b, d);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
        ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) return true;
    if (d1 == 0 && onSeg(c, a, d)) return true;
    if (d2 == 0 && onSeg(c, b, d)) return true;
    if (d3 == 0 && onSeg(a, c, b)) return true;
    if (d4 == 0 && onSeg(a, d, b)) return true;
    return false;
}

bool isSimple(const std::vector<bcad::geom::Point2>& v) {
    size_t n = v.size();
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) continue; // arête fermante adjacente
            if (segmentsIntersect(v[i], v[(i+1)%n], v[j], v[(j+1)%n])) return false;
        }
    return true;
}

// ── Entité parcelle ────────────────────────────────────────────────────
class ParcelEntity : public bcad::geom::PolylineEntity {
public:
    ParcelEntity(std::vector<bcad::geom::Point2> vertices,
                 std::string section, std::string numero,
                 std::string contenance, std::string commune = "")
        : PolylineEntity(std::move(vertices), /*closed=*/true),
          section_(std::move(section)), numero_(std::move(numero)),
          contenance_(std::move(contenance)), commune_(std::move(commune)) {}

    bcad::geom::TypeId typeId() const override { return bcad::geom::TypeId{"cadastre.parcel"}; }

    std::unique_ptr<bcad::geom::Entity> clone() const override {
        auto c = std::make_unique<ParcelEntity>(vertices(), section_, numero_, contenance_, commune_);
        c->setId(id());
        c->setLayer(layer());
        return c;
    }

    double area() const { return std::abs(signedArea(vertices())); }

    bool isValid(std::string* reason = nullptr) const {
        if (vertices().size() < 3) { if (reason) *reason = "au moins 3 sommets"; return false; }
        if (area() < 1e-9) { if (reason) *reason = "aire nulle (sommets colinéaires)"; return false; }
        if (!isSimple(vertices())) { if (reason) *reason = "polygone auto-intersectant"; return false; }
        if (section_.empty() || numero_.empty()) { if (reason) *reason = "section/numéro manquant"; return false; }
        return true;
    }

    const std::string& section() const { return section_; }
    const std::string& numero() const { return numero_; }
    const std::string& contenance() const { return contenance_; }
    const std::string& commune() const { return commune_; }

private:
    std::string section_, numero_, contenance_, commune_;
};

// Format canonique : x,y;x,y;...|section|numero|contenance|commune
std::string parcelToString(const ParcelEntity& p) {
    std::ostringstream ss;
    for (size_t i = 0; i < p.vertices().size(); ++i) {
        if (i) ss << ';';
        ss << p.vertices()[i].x_ << ',' << p.vertices()[i].y_;
    }
    ss << '|' << p.section() << '|' << p.numero() << '|' << p.contenance() << '|' << p.commune();
    return ss.str();
}

std::unique_ptr<bcad::geom::Entity> makeParcel(std::string_view params) {
    // Factory appelée sans params (EntityRegistry::create sans args) → parcelle par défaut
    if (params.empty()) {
        std::vector<bcad::geom::Point2> v = {{0,0},{10,0},{10,5},{0,5}};
        return std::make_unique<ParcelEntity>(std::move(v), "A", "001", "50m²");
    }
    std::string s(params);
    // split sur '|'
    std::vector<std::string> fields;
    std::string cur;
    for (char c : s) {
        if (c == '|') { fields.push_back(cur); cur.clear(); }
        else cur += c;
    }
    fields.push_back(cur);
    if (fields.size() < 4) return nullptr;

    // sommets
    std::vector<bcad::geom::Point2> vertices;
    std::istringstream vtok(fields[0]);
    std::string v;
    while (std::getline(vtok, v, ';')) {
        if (v.empty()) continue;
        std::istringstream ps(v);
        double x = 0, y = 0; char sep = 0;
        if (!(ps >> x >> sep >> y) || sep != ',') return nullptr;
        vertices.emplace_back(x, y);
    }
    std::string commune = fields.size() > 4 ? fields[4] : "";
    auto e = std::make_unique<ParcelEntity>(std::move(vertices), fields[1], fields[2], fields[3], commune);
    std::string reason;
    if (!e->isValid(&reason)) return nullptr;
    return e;
}

// ── Commande ───────────────────────────────────────────────────────────
class CreateParcelCommand : public bcad::commands::Command {
public:
    std::string_view text() const override { return "cadastre.create_parcel"; }
    void execute(bcad::core::Document&) override {}
    void undo(bcad::core::Document&) override {}
    std::unique_ptr<bcad::commands::Command> clone() const override {
        return std::make_unique<CreateParcelCommand>();
    }
};

std::unique_ptr<bcad::commands::Command> makeCreateParcel(const std::vector<std::string>&) {
    return std::make_unique<CreateParcelCommand>();
}

// ── Serializer ─────────────────────────────────────────────────────────
class ParcelSerializer : public bcad::serialization::IEntitySerializer {
public:
    bcad::geom::TypeId typeId() const override { return bcad::geom::TypeId{"cadastre.parcel"}; }
    std::string_view formatName() const override { return "Cadastre Parcel (CSV)"; }

    std::string serialize(const bcad::geom::Entity& entity) const override {
        const auto& p = static_cast<const ParcelEntity&>(entity);
        return parcelToString(p);
    }

    std::unique_ptr<bcad::geom::Entity> deserialize(const std::string& data) const override {
        return makeParcel(data);
    }

    void writeToStream(std::ostream& out, const bcad::geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<bcad::geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }
};

} // namespace

extern "C" int bcad_plugin_api_version() {
    return bcad::plugin::PLUGIN_API_VERSION;
}

extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& registry) {
    registry.info().name = "cadastre";
    registry.info().version = "1.0.0";
    registry.info().description = "Module metier cadastre (Phase 13, ADR-005)";
    registry.info().author = "bcad";
    bool ok = registry.registerEntityType(bcad::geom::TypeId{"cadastre.parcel"}, makeParcel);
    ok = registry.registerCommand("cadastre.create_parcel", makeCreateParcel) && ok;
    ok = registry.registerSerializer(std::make_unique<ParcelSerializer>()) && ok;
    return ok;
}

extern "C" void bcad_plugin_shutdown() {}
