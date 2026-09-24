#include "bcad/cadastre/ParcelEntity.h"

#include "bcad/serialization/Serializer.h"
#include <memory>
#include <sstream>

namespace bcad::cadastre {

namespace {

// Sérialiseur natif SQLite : géométrie CSV + références cadastrales.
// Format : "closed,x,y,...,|section|numero" via serializeParams() de
// ParcelEntity, plus contenance/commune/proprietaire/nature dans un
// second champ après un '|' supplémentaire pour roundtrip complet.
class ParcelSerializer : public serialization::IEntitySerializer {
public:
    geom::TypeId typeId() const override { return TypeId_Parcel; }
    std::string_view formatName() const override { return "Cadastre Parcel (CSV)"; }

    std::string serialize(const geom::Entity& entity) const override {
        // Accepte ParcelEntity et PolylineEntity enrichie (F1 avant promotion).
        const auto& poly = static_cast<const geom::PolylineEntity&>(entity);
        const auto& props = poly.properties();
        std::ostringstream ss;
        ss.precision(17);
        ss << (poly.closed() ? 1 : 0);
        for (const auto& v : poly.vertices()) ss << ',' << v.x_ << ',' << v.y_;
        ss << '|' << props.getString("cadastre.section")
           << '|' << props.getString("cadastre.numero")
           << '|' << props.getString("cadastre.contenance")
           << '|' << props.getString("cadastre.commune")
           << '|' << props.getString("cadastre.proprietaire")
           << '|' << props.getString("cadastre.nature");
        return ss.str();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<std::string> fields;
        std::string cur;
        for (char c : data) {
            if (c == '|') { fields.push_back(cur); cur.clear(); }
            else cur += c;
        }
        fields.push_back(cur);
        if (fields.size() < 2) return nullptr;

        // fields[0] = "closed,x,y,..." (format PolylineEntity)
        std::vector<double> v;
        {
            std::stringstream ss(fields[0]);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (!token.empty()) v.push_back(std::stod(token));
            }
        }
        if (v.size() < 7) return nullptr; // closed + >=3 points
        std::vector<geom::Point2> verts;
        for (std::size_t i = 1; i + 1 < v.size(); i += 2) {
            verts.emplace_back(v[i], v[i + 1]);
        }
        auto e = std::make_unique<ParcelEntity>(std::move(verts));
        auto& props = e->properties();
        props.setString("cadastre.section", fields.size() > 1 ? fields[1] : "");
        props.setString("cadastre.numero", fields.size() > 2 ? fields[2] : "");
        // serializeParams a déjà mis section|numero en fin ; le corps
        // principal (fields[1..2] après le premier '|') est section|numero.
        // Les champs métier supplémentaires suivent après le 3e '|'.
        // Re-parse : closed,x,y,... | section | numero | contenance | ...
        if (fields.size() > 3) props.setString("cadastre.contenance", fields[3]);
        if (fields.size() > 4) props.setString("cadastre.commune", fields[4]);
        if (fields.size() > 5) props.setString("cadastre.proprietaire", fields[5]);
        if (fields.size() > 6) props.setString("cadastre.nature", fields[6]);
        return e;
    }

    void writeToStream(std::ostream& out, const geom::Entity& entity) const override {
        out << serialize(entity);
    }

    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override {
        std::string line;
        std::getline(in, line);
        return deserialize(line);
    }
};

void registerCadastreTypes() {
    serialization::SerializerRegistry::registerSerializer(std::make_unique<ParcelSerializer>());
}

struct CadastreRegistrar {
    CadastreRegistrar() { registerCadastreTypes(); }
};
static CadastreRegistrar g_cadastreRegistrar;

} // namespace

} // namespace bcad::cadastre
