#include "BoundaryEntity.h"
#include "SerializerRegistration.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <memory>
#include <sstream>
#include <string_view>

namespace bcad::cadastre {

namespace {

class BoundarySerializer : public serialization::IEntitySerializer {
public:
    geom::TypeId typeId() const override { return TypeId_Boundary; }
    std::string_view formatName() const override { return "Cadastre Boundary (CSV)"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& b = static_cast<const BoundaryEntity&>(entity);
        const auto& props = b.properties();
        std::string ss = geom::PolylineEntity::encodeRings(
            geom::PolylineEntity::Rings{b.closed(), b.vertices(), b.holes()});
        ss += '|' + std::to_string(props.getEnum("cadastre.boundary_type"))
            + '|' + props.getString("cadastre.reference");
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
        // Une limite reste ouverte : c'est son statut metier, pas la grammaire.
        auto e = std::make_unique<BoundaryEntity>(std::move(rings.outer), false);
        for (auto& hole : rings.holes) e->addHole(std::move(hole));
        if (fields.size()>1) e->properties().setEnum("cadastre.boundary_type", std::stoi(fields[1]));
        if (fields.size()>2) e->properties().setString("cadastre.reference", fields[2]);
        return e;
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override { std::string l; std::getline(in,l); return deserialize(l); }
};

} // namespace

bool registerBoundarySerializer(bcad::plugin::PluginRegistry& registry) {
    return registry.registerSerializer(std::make_unique<BoundarySerializer>());
}

} // namespace bcad::cadastre