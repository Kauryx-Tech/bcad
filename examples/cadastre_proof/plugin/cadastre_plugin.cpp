#include "bcad/plugin/PluginRegistry.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/commands/Command.h"
#include "bcad/serialization/Serializer.h"
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// MODULE METIER CADASTRE (Phase 13) : squelette minimal, calqué sur la
// recette hello_plugin (Phase 10, ADR-005). L'entite `cadastre.parcel` et
// la commande `cadastre.create_parcel` sont INCONNUES du Core.
// ---------------------------------------------------------------------------
namespace {

// Fabrique d'entite : polygone ferme (4 coins rectangle 10x5).
// Le TypeId "cadastre.parcel" est inconnu du Core.
std::unique_ptr<bcad::geom::Entity> makeParcel(std::string_view) {
    std::vector<bcad::geom::Point2> vertices = {
        {0.0, 0.0}, {10.0, 0.0}, {10.0, 5.0}, {0.0, 5.0}
    };
    return std::make_unique<bcad::geom::PolylineEntity>(std::move(vertices), /*closed=*/true);
}

// Commande no-op de demonstration : prouve le contrat commande sans
// dependre de l'etat du document.
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

// Serializer pour "cadastre.parcel" : format CSV simple x,y;x,y;...
class ParcelSerializer : public bcad::serialization::IEntitySerializer {
public:
    bcad::geom::TypeId typeId() const override { return bcad::geom::TypeId{"cadastre.parcel"}; }
    std::string_view formatName() const override { return "Cadastre Parcel (CSV)"; }

    std::string serialize(const bcad::geom::Entity& entity) const override {
        const auto& p = static_cast<const bcad::geom::PolylineEntity&>(entity);
        std::ostringstream ss;
        for (size_t i = 0; i < p.vertices().size(); ++i) {
            if (i) ss << ';';
            ss << p.vertices()[i].x_ << ',' << p.vertices()[i].y_;
        }
        return ss.str();
    }

    std::unique_ptr<bcad::geom::Entity> deserialize(const std::string& data) const override {
        std::vector<bcad::geom::Point2> vertices;
        std::istringstream ss(data);
        std::string v;
        while (std::getline(ss, v, ';')) {
            std::istringstream ps(v);
            double x = 0.0, y = 0.0;
            char sep = '\0';
            if (!(ps >> x >> sep >> y) || sep != ',') {
                return nullptr;
            }
            vertices.emplace_back(x, y);
        }
        if (vertices.size() < 3) return nullptr;
        return std::make_unique<bcad::geom::PolylineEntity>(std::move(vertices), /*closed=*/true);
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

// ---------------------------------------------------------------------------
// Mediation ADR-005 : le plugin expose uniquement son init, les registres
// sont portes par l'hote. Aucune bibliotheque de types n'est liee ici.
// ---------------------------------------------------------------------------
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