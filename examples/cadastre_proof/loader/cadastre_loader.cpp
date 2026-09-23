#include "bcad/plugin/Plugin.h"
#include "bcad/registry/EntityRegistry.h"
#include "bcad/commands/CommandRegistry.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/serialization/Serializer.h"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>

// Chargeur hôte minimal (Phase 10) : charge le module métier cadastre via
// PluginManager (dlopen), vérifie ses métadonnées PUIS que ses enregistrements
// (entité cadastre.parcel, commande cadastre.create_parcel, serializer)
// ont bien abouti dans les registres globaux portés par l'exécutable hôte
// (médiation par l'hôte : registres et types partagés). Les objets créés
// depuis les factories du module sont détruits dans le bloc, AVANT unloadPlugin/dlclose :
// leur code vit dans le DSO du module.
//
// Le module ne lie AUCUNE bibliothèque de types : les vtables/typeinfo/entités
// sont celles de l'hôte (exposées via --export-dynamic-symbol). Les factories
// du module sont des pointeurs de fonction (ABI ADR-005) re-emballées côté hôte :
// aucun std::function détenant du code du module ne survit au déchargement.
static int fail(const char* msg) {
    std::cerr << "FAIL: " << msg << "\n";
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: bcad_cadastre_loader <plugin.so>\n";
        return 2;
    }

    bcad::plugin::PluginManager& mgr = bcad::plugin::pluginManager();
    bcad::plugin::PluginHandle* handle = mgr.loadPlugin(argv[1]);
    if (!handle || !handle->loaded) {
        return fail("loadPlugin");
    }
    if (handle->info.name != "cadastre" || handle->info.version != "1.0.0") {
        std::cerr << "FAIL: plugin info (" << handle->info.name << ")\n";
        return 1;
    }
    if (handle->info.apiVersion != bcad::plugin::PLUGIN_API_VERSION) {
        return fail("api version");
    }

    // Vérification de la médiation pendant que le module est chargé.
    {
        const bcad::geom::TypeId parcelId{"cadastre.parcel"};

        // 1) Entité enregistrée dans le registre global
        if (!bcad::registry::EntityRegistry::contains(parcelId)) {
            return fail("entité cadastre.parcel absente du registre global");
        }
        auto entity = bcad::registry::EntityRegistry::create(parcelId);
        if (!entity) {
            return fail("création de cadastre.parcel");
        }

        // ParcelEntity hérite de PolylineEntity — dynamic_cast doit réussir
        if (dynamic_cast<bcad::geom::PolylineEntity*>(entity.get()) == nullptr) {
            return fail("typeinfo/vtables non partagés (dynamic_cast inter-DSO)");
        }
        if (entity->typeId().value != "cadastre.parcel") {
            return fail("typeId != cadastre.parcel");
        }

        // 2) Commande enregistrée
        if (!bcad::commands::CommandRegistry::instance().hasCommand("cadastre.create_parcel")) {
            return fail("commande cadastre.create_parcel absente du registre global");
        }
        auto cmd = bcad::commands::CommandRegistry::instance().createCommand("cadastre.create_parcel", {});
        if (!cmd) {
            return fail("création de cadastre.create_parcel");
        }

        // 3) Médiation serializer : le serializer du module est porté par le
        // registre hôte, ses instances/vtables vivent dans le module. Le
        // roundtrip via l'hôte prouve le partage de types cross-DSO.
        if (!bcad::serialization::SerializerRegistry::contains(parcelId)) {
            return fail("serializer cadastre.parcel absent du registre global");
        }
        const auto* ser = bcad::serialization::SerializerRegistry::find(parcelId);
        if (ser == nullptr || ser->formatName() != "Cadastre Parcel (CSV)") {
            return fail("serializer cadastre.parcel invalide");
        }
        auto roundtripped = ser->deserialize("0,0;10,0;10,5;0,5|A|42|500m²|");
        if (!roundtripped) {
            return fail("désérialisation cadastre.parcel");
        }
        if (dynamic_cast<bcad::geom::PolylineEntity*>(roundtripped.get()) == nullptr) {
            return fail("types non partagés dans le serializer (dynamic_cast inter-DSO)");
        }
        const auto& rp = static_cast<const bcad::geom::PolylineEntity&>(*roundtripped);
        std::string serialized = ser->serialize(rp);
        // Roundtrip : doit contenir les sommets
        if (serialized.find("0,0") == std::string::npos || serialized.find("A|42") == std::string::npos) {
            std::cerr << "FAIL: roundtrip got '" << serialized << "'\n";
            return fail("roundtrip serializer cadastre.parcel");
        }
        roundtripped.reset();
    } // entity, cmd et roundtripped détruits ici, avant le déchargement

    if (!mgr.unloadPlugin(handle)) {
        return fail("unloadPlugin");
    }
    std::cout << "OK: module métier cadastre chargé, enregistrements et types partagés avec l'hôte, déchargé\n";
    return 0;
}