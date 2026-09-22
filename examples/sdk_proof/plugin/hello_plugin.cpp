#include "bcad/plugin/PluginRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/commands/Command.h"
#include <memory>
#include <string>
#include <vector>

// Plugin externe minimal (Phase 10, ADR-005) : charge via dlopen. Le point
// d'entree est bcad_plugin_init(PluginRegistry&) : le plugin remplit ses
// metadonnees puis enregistre une entite et une commande non natives via le
// registre (mediation par l'hote libbcad_plugin).
namespace {

// Fabrique de l'entite plugin : retourne une entite de demonstration.
// Le TypeId "hello.marker" est inconnu du Core.
std::unique_ptr<bcad::geom::Entity> makeMarker(std::string_view) {
    return std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{0, 0});
}

// Commande no-op de demonstration : prouve le contrat commande sans
// dependre de l'etat du document.
class HelloCommand : public bcad::commands::Command {
public:
    std::string_view text() const override { return "hello"; }
    void execute(bcad::core::Document&) override {}
    void undo(bcad::core::Document&) override {}
    std::unique_ptr<Command> clone() const override {
        return std::make_unique<HelloCommand>();
    }
};

std::unique_ptr<bcad::commands::Command> makeHelloCommand(const std::vector<std::string>&) {
    return std::make_unique<HelloCommand>();
}

} // namespace

extern "C" int bcad_plugin_api_version() {
    return bcad::plugin::PLUGIN_API_VERSION;
}

extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& registry) {
    registry.info().name = "hello";
    registry.info().version = "1.0.0";
    registry.info().description = "Minimal BCAD plugin";
    registry.info().author = "bcad";

    bool ok = registry.registerEntityType(bcad::geom::TypeId{"hello.marker"}, makeMarker);
    ok = registry.registerCommand("hello.greet", makeHelloCommand) && ok;
    return ok;
}

extern "C" void bcad_plugin_shutdown() {}