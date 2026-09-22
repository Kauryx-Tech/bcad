#include "bcad/plugin/Plugin.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/commands/CommandRegistry.h"
#include <cstdlib>
#include <iostream>
#include <string>

// Chargeur minimal (Phase 10) : charge le plugin externe via PluginManager
// (dlopen), verifie ses metadonnees PUIS que les enregistrements du plugin
// ont bien abouti dans les registres globaux portes par l'executable
// (mediation par l'hote : une seule instance des registres, partagee entre
// l'executable et le DSO charge). Enfin, decharge le plugin. Le chargement
// echoue si bcad_plugin_init() renvoie false.
static int fail(const char* msg) {
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bcad_plugin_loader <plugin.so>\n";
        return 2;
    }

    bcad::plugin::PluginManager& mgr = bcad::plugin::pluginManager();
    bcad::plugin::PluginHandle* handle = mgr.loadPlugin(argv[1]);
    if (!handle || !handle->loaded) {
        return fail("loadPlugin");
    }
    if (handle->info.name != "hello" || handle->info.version != "1.0.0") {
        std::cerr << "FAIL: plugin info (" << handle->info.name << ")\n";
        return 1;
    }
    if (handle->info.apiVersion != bcad::plugin::PLUGIN_API_VERSION) {
        return fail("api version");
    }

    // Les registrations du plugin doivent etre visibles dans les registres
    // globaux de l'hote (mediatises par libbcad_plugin).
    const bcad::geom::TypeId marker{"hello.marker"};
    if (!bcad::registry::EntityRegistry::contains(marker)) {
        return fail("entite hello.marker absente du registre global");
    }
    auto entity = bcad::registry::EntityRegistry::create(marker);
    if (!entity) {
        return fail("creation de hello.marker");
    }

    if (!bcad::commands::CommandRegistry::instance().hasCommand("hello.greet")) {
        return fail("commande hello.greet absente du registre global");
    }
    auto cmd = bcad::commands::CommandRegistry::instance().createCommand("hello.greet", {});
    if (!cmd) {
        return fail("creation de hello.greet");
    }

    if (!mgr.unloadPlugin(handle)) {
        return fail("unloadPlugin");
    }
    std::cout << "OK: plugin externe charge, enregistrements effectifs cote hote, decharge\n";
    return 0;
}