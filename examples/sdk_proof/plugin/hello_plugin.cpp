#include "bcad/plugin/Plugin.h"
#include "bcad/geometry/PointEntity.h"
#include <memory>

// Plugin externe minimal (Phase 10) : charge via dlopen, enregistre un type
// d'entite non natif `hello.marker` au demarrage via l'interface PluginManager.
namespace {

const bcad::plugin::PluginInfo g_info{
    "hello",                 // name
    "1.0.0",                 // version
    "Minimal BCAD plugin",   // description
    "bcad",                  // author
    bcad::plugin::PLUGIN_API_VERSION
};

} // namespace

extern "C" const bcad::plugin::PluginInfo* bcad_plugin_info() {
    return &g_info;
}

extern "C" int bcad_plugin_api_version() {
    return bcad::plugin::PLUGIN_API_VERSION;
}

extern "C" bool bcad_plugin_init(bcad::plugin::PluginManager& manager) {
    bcad::plugin::EntityFactory factory = [](std::string_view) -> std::unique_ptr<bcad::geom::Entity> {
        return std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{0, 0});
    };
    return manager.registerEntityType(bcad::geom::TypeId{"hello.marker"}, factory);
}

extern "C" void bcad_plugin_shutdown() {}