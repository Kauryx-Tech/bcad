#include "SurveyMarkEntity.h"
#include "SerializerRegistration.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/serialization/Serializer.h"
#include <memory>
#include <sstream>
#include <string_view>

namespace bcad::cadastre {

namespace {

class SurveyMarkSerializer : public serialization::IEntitySerializer {
public:
    geom::TypeId typeId() const override { return TypeId_SurveyMark; }
    std::string_view formatName() const override { return "Cadastre SurveyMark (CSV)"; }

    std::string serialize(const geom::Entity& entity) const override {
        const auto& s = static_cast<const SurveyMarkEntity&>(entity);
        const auto& props = s.properties();
        std::ostringstream ss; ss.precision(17);
        ss << s.position().x_ << ',' << s.position().y_;
        ss << '|' << props.getEnum("cadastre.mark_type")
           << '|' << props.getString("cadastre.reference")
           << '|' << props.getDouble("cadastre.precision");
        return ss.str();
    }

    std::unique_ptr<geom::Entity> deserialize(const std::string& data) const override {
        std::vector<std::string> fields; std::string cur;
        for (char c : data) { if (c == '|') { fields.push_back(cur); cur.clear(); } else cur += c; }
        fields.push_back(cur);
        if (fields.size() < 3) return nullptr;
        double x = std::stod(fields[0]), y = std::stod(fields[1]);
        auto e = std::make_unique<SurveyMarkEntity>(geom::Point2{x,y});
        if (fields.size()>2) e->properties().setEnum("cadastre.mark_type", std::stoi(fields[2]));
        if (fields.size()>3) e->properties().setString("cadastre.reference", fields[3]);
        if (fields.size()>4) e->properties().setDouble("cadastre.precision", std::stod(fields[4]));
        return e;
    }

    void writeToStream(std::ostream& out, const geom::Entity& e) const override { out << serialize(e); }
    std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const override { std::string l; std::getline(in,l); return deserialize(l); }
};

} // namespace

bool registerSurveyMarkSerializer(bcad::plugin::PluginRegistry& registry) {
    return registry.registerSerializer(std::make_unique<SurveyMarkSerializer>());
}

} // namespace bcad::cadastre