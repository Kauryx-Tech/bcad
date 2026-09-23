#include "bcad/plugin/Plugin.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/serialization/Serializer.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

// Chargeur minimal (Phase 10) : charge le plugin externe via PluginManager
// (dlopen), verifie ses metadonnees PUIS que les enregistrements du plugin
// ont bien abouti dans les registres globaux portes par l'executable hote
// (mediation par l'hote : registres et types partages). Les objets crees
// depuis les factories du plugin (entite, commande) sont detruits dans le
// bloc, AVANT unloadPlugin/dlclose : leur code vit dans le DSO du plugin.
//
// Le plugin ne lie AUCUNE bibliotheque de types : les vtables/typeinfo/entites
// sont celles de l'hote (exposees via --export-dynamic-symbol). Les factories
// plugins sont des pointeurs de fonction (ABI ADR-005) re-emballes cote hote :
// aucun std::function detenant du code du plugin ne survit au dechargement.
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

    // Verification de la mediation pendant que le plugin est charge.
    {
        const bcad::geom::TypeId marker{"hello.marker"};
        if (!bcad::registry::EntityRegistry::contains(marker)) {
            return fail("entite hello.marker absente du registre global");
        }
        auto entity = bcad::registry::EntityRegistry::create(marker);
        if (!entity) {
            return fail("creation de hello.marker");
        }

        // Types partages hote<->plugin : le dynamic_cast inter-DSO et le
        // typeid doivent reussir (une seule copie des typeinfo/vtables).
        if (dynamic_cast<bcad::geom::PointEntity*>(entity.get()) == nullptr) {
            return fail("typeinfo/vtables non partages (dynamic_cast inter-DSO)");
        }
        if (typeid(*entity) != typeid(bcad::geom::PointEntity)) {
            return fail("typeid inter-DSO divergent");
        }

        if (!bcad::commands::CommandRegistry::instance().hasCommand("hello.greet")) {
            return fail("commande hello.greet absente du registre global");
        }
        auto cmd = bcad::commands::CommandRegistry::instance().createCommand("hello.greet", {});
        if (!cmd) {
            return fail("creation de hello.greet");
        }

        // Mediation serializer : le serializer du plugin est porte par le
        // registre hote, ses instances/vtables vivent dans le plugin. Le
        // roundtrip via l'hote prouve le partage de types cross-DSO.
        if (!bcad::serialization::SerializerRegistry::contains(marker)) {
            return fail("serializer hello.marker absent du registre global");
        }
        const auto* ser = bcad::serialization::SerializerRegistry::find(marker);
        if (ser == nullptr || ser->formatName() != "Hello Marker") {
            return fail("serializer hello.marker invalide");
        }
        auto roundtripped = ser->deserialize("1.5,2.5");
        if (!roundtripped) {
            return fail("deserialisation hello.marker");
        }
        if (dynamic_cast<bcad::geom::PointEntity*>(roundtripped.get()) == nullptr) {
            return fail("types non partages dans le serializer (dynamic_cast inter-DSO)");
        }
        const auto& rp = static_cast<const bcad::geom::PointEntity&>(*roundtripped);
        if (ser->serialize(rp) != "1.5,2.5") {
            return fail("roundtrip serializer hello.marker");
        }
        roundtripped.reset();
    } // entity, cmd et roundtripped detruits ici, avant le dechargement

    if (!mgr.unloadPlugin(handle)) {
        return fail("unloadPlugin");
    }
    std::cout << "OK: plugin externe charge, enregistrements et types partages avec l'hote, decharge\n";
    return 0;
}