#include "EasementEntity.h"
#include "FieldEncoding.h"
#include "SerializerRegistration.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <memory>
#include <sstream>
#include <string_view>

namespace bcad::cadastre {

namespace {

class EasementSerializer : public serialization::IEntitySerializer {
public:
    geom::TypeId typeId() const override { return TypeId_Easement; }
    std::string_view formatName() const override { return "Cadastre Easement (CSV)"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& es = static_cast<const EasementEntity&>(entity);
        const auto& props = es.properties();
        std::string ss = geom::PolylineEntity::encodeRings(
            geom::PolylineEntity::Rings{es.closed(), es.vertices(), es.holes()});
        ss += '|' + std::to_string(props.getEnum("cadastre.easement_type"))
            + '|' + encodeField(props.getString("cadastre.beneficiaire"))
            + '|' + encodeField(props.getString("cadastre.reference"));
        return ss;
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<std::string> fields; std::string cur;
        for (char c : data) { if (c == '|') { fields.push_back(cur); cur.clear(); } else cur += c; }
        fields.push_back(cur);
        if (fields.size() < 1) return nullptr;
        geom::PolylineEntity::Rings rings;
        if (!geom::PolylineEntity::decodeRings(fields[0], rings)) return nullptr;
        if (rings.outer.empty()) return nullptr;
        auto e = std::make_unique<EasementEntity>(std::move(rings.outer));
        for (auto& hole : rings.holes) e->addHole(std::move(hole));
        if (fields.size()>1) e->properties().setEnum("cadastre.easement_type", std::stoi(fields[1]));
        if (fields.size()>2) e->properties().setString("cadastre.beneficiaire", decodeField(fields[2]));
        if (fields.size()>3) e->properties().setString("cadastre.reference",    decodeField(fields[3]));
        return e;
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override { std::string l; std::getline(in,l); return deserialize(l); }
};

} // namespace

bool registerEasementSerializer(bcad::plugin::PluginRegistry& registry) {
    return registry.registerSerializer(std::make_unique<EasementSerializer>());
}

} // namespace bcad::cadastre