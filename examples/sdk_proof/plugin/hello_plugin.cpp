#include "bcad/plugin/PluginRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include <memory>

// Plugin externe minimal (Phase 10, ADR-005) : charge via dlopen. Le point
// d'entree est bcad_plugin_init(PluginRegistry&) : le plugin remplit ses
// metadonnees et enregistre un type d'entite non natif via le registre.
namespace {

// Fabrique de l'entite plugin : retourne une entite de demonstration.
std::unique_ptr<bcad::geom::Entity> makeMarker(std::string_view) {
    return std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{0, 0});
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

    bcad::plugin::EntityFactory factory = makeMarker;
    return registry.registerEntityType(bcad::geom::TypeId{"hello.marker"}, factory);
}

extern "C" void bcad_plugin_shutdown() {}