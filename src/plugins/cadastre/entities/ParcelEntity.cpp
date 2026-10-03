#include "ParcelEntity.h"
#include "FieldEncoding.h"
#include "SerializerRegistration.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <memory>
#include <sstream>
#include <string_view>

namespace bcad::cadastre {

namespace {

class ParcelSerializer : public serialization::IEntitySerializer {
public:
    geom::TypeId typeId() const override { return TypeId_Parcel; }
    std::string_view formatName() const override { return "Cadastre Parcel (CSV)"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& p = static_cast<const ParcelEntity&>(entity);
        const auto& props = p.properties();
        std::string ss = geom::PolylineEntity::encodeRings(
            geom::PolylineEntity::Rings{p.closed(), p.vertices(), p.holes()});
        ss += '|' + encodeField(props.getString("cadastre.section"))
            + '|' + encodeField(props.getString("cadastre.numero"))
            + '|' + encodeField(props.getString("cadastre.contenance"))
            + '|' + encodeField(props.getString("cadastre.commune"))
            + '|' + encodeField(props.getString("cadastre.proprietaire"))
            + '|' + std::to_string(props.getEnum("cadastre.nature"));
        return ss;
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

        geom::PolylineEntity::Rings rings;
        if (!geom::PolylineEntity::decodeRings(fields[0], rings)) return nullptr;
        if (rings.outer.size() < 3) return nullptr;
        auto e = std::make_unique<ParcelEntity>(std::move(rings.outer));
        for (auto& hole : rings.holes) e->addHole(std::move(hole));
        auto& props = e->properties();
        if (fields.size() > 1) props.setString("cadastre.section",      decodeField(fields[1]));
        if (fields.size() > 2) props.setString("cadastre.numero",       decodeField(fields[2]));
        if (fields.size() > 3) props.setString("cadastre.contenance",   decodeField(fields[3]));
        if (fields.size() > 4) props.setString("cadastre.commune",      decodeField(fields[4]));
        if (fields.size() > 5) props.setString("cadastre.proprietaire", decodeField(fields[5]));
        if (fields.size() > 6) props.setEnum("cadastre.nature", std::stoi(fields[6]));
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

} // namespace

bool registerParcelSerializer(bcad::plugin::PluginRegistry& registry) {
    return registry.registerSerializer(std::make_unique<ParcelSerializer>());
}

} // namespace bcad::cadastre
