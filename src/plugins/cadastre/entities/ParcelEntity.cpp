#include "ParcelEntity.h"
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
        std::ostringstream ss;
        ss.precision(17);
        ss << (p.closed() ? 1 : 0);
        for (const auto& v : p.vertices()) ss << ',' << v.x_ << ',' << v.y_;
        ss << '|' << props.getString("cadastre.section")
           << '|' << props.getString("cadastre.numero")
           << '|' << props.getString("cadastre.contenance")
           << '|' << props.getString("cadastre.commune")
           << '|' << props.getString("cadastre.proprietaire")
           << '|' << props.getEnum("cadastre.nature");
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

        std::vector<double> v;
        {
            std::stringstream ss(fields[0]);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (!token.empty()) v.push_back(std::stod(token));
            }
        }
        if (v.size() < 7) return nullptr;
        std::vector<geom::Point2> verts;
        for (std::size_t i = 1; i + 1 < v.size(); i += 2) {
            verts.emplace_back(v[i], v[i + 1]);
        }
        auto e = std::make_unique<ParcelEntity>(std::move(verts));
        auto& props = e->properties();
        if (fields.size() > 1) props.setString("cadastre.section", fields[1]);
        if (fields.size() > 2) props.setString("cadastre.numero", fields[2]);
        if (fields.size() > 3) props.setString("cadastre.contenance", fields[3]);
        if (fields.size() > 4) props.setString("cadastre.commune", fields[4]);
        if (fields.size() > 5) props.setString("cadastre.proprietaire", fields[5]);
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