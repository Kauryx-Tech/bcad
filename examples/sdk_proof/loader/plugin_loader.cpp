#include "bcad/plugin/Plugin.h"
#include <cstdlib>
#include <iostream>
#include <string>

// Chargeur minimal (Phase 10) : charge le plugin externe via PluginManager
// (dlopen), verifie ses metadonnees puis le decharge. Le chargement echoue
// si bcad_plugin_init() renvoie false (echec d'enregistrement du type).
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bcad_plugin_loader <plugin.so>\n";
        return 2;
    }

    bcad::plugin::PluginManager& mgr = bcad::plugin::pluginManager();
    bcad::plugin::PluginHandle* handle = mgr.loadPlugin(argv[1]);
    if (!handle || !handle->loaded) {
        std::cerr << "FAIL: loadPlugin\n" << std::flush;
        return 1;
    }
    if (handle->info.name != "hello" || handle->info.version != "1.0.0") {
        std::cerr << "FAIL: plugin info (" << handle->info.name << ")\n";
        return 1;
    }
    if (handle->info.apiVersion != bcad::plugin::PLUGIN_API_VERSION) {
        std::cerr << "FAIL: api version\n";
        return 1;
    }
    if (!mgr.unloadPlugin(handle)) {
        std::cerr << "FAIL: unloadPlugin\n";
        return 1;
    }
    std::cout << "OK: plugin externe charge, enregistre et decharge\n";
    return 0;
}