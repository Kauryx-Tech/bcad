#include "EasementEntity.h"
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
        std::ostringstream ss; ss.precision(17);
        ss << (es.closed() ? 1 : 0);
        for (const auto& v : es.vertices()) ss << ',' << v.x_ << ',' << v.y_;
        ss << '|' << props.getEnum("cadastre.easement_type")
           << '|' << props.getString("cadastre.beneficiaire")
           << '|' << props.getString("cadastre.reference");
        return ss.str();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<std::string> fields; std::string cur;
        for (char c : data) { if (c == '|') { fields.push_back(cur); cur.clear(); } else cur += c; }
        fields.push_back(cur);
        if (fields.size() < 1) return nullptr;
        std::vector<double> v; std::stringstream ss(fields[0]); std::string t;
        while (std::getline(ss, t, ',')) { if (!t.empty()) v.push_back(std::stod(t)); }
        if (v.size() < 3) return nullptr;
        std::vector<geom::Point2> verts; for (size_t i=1;i+1<v.size();i+=2) verts.emplace_back(v[i],v[i+1]);
        auto e = std::make_unique<EasementEntity>(std::move(verts));
        if (fields.size()>1) e->properties().setEnum("cadastre.easement_type", std::stoi(fields[1]));
        if (fields.size()>2) e->properties().setString("cadastre.beneficiaire", fields[2]);
        if (fields.size()>3) e->properties().setString("cadastre.reference", fields[3]);
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