#pragma once

// Registre expose au plugin (ADR-005 : `bcad_plugin_init(PluginRegistry&)`).
// L'enregistrement est mediatise par l'hote (libbcad_plugin) : les extensions
// sont enregistrees dans les registres globaux depuis le DSO hote, ce qui
// garantit une seule instance des registres quel que soit le DSO.

#include "bcad/geometry/TypeId.h"
#include "bcad/geometry/Entity.h"
#include "bcad/commands/Command.h"
#include "bcad/serialization/Serializer.h"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::plugin {

// Export macro pour la bibliotheque hote (libbcad_plugin) : seul l'API
// publique est exportee (le reste est compile avec -fvisibility=hidden).
#if defined(_WIN32)
#  if defined(BCAD_PLUGIN_BUILDING)
#    define BCAD_PLUGIN_API __declspec(dllexport)
#  else
#    define BCAD_PLUGIN_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define BCAD_PLUGIN_API __attribute__((visibility("default")))
#else
#  define BCAD_PLUGIN_API
#endif

// Version of the plugin API - increment on breaking changes
constexpr int PLUGIN_API_VERSION = 1;

// Plugin metadata (remplie par le plugin dans PluginRegistry::info())
struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    int apiVersion = PLUGIN_API_VERSION;
};

// Type aliases for plugin callbacks
using EntityFactory = std::function<std::unique_ptr<bcad::geom::Entity>(std::string_view params)>;
using CommandFactory = std::function<std::unique_ptr<bcad::commands::Command>(const std::vector<std::string>& args)>;

// Objet passe a `bcad_plugin_init`. Expose les metadonnees du plugin et les
// points d'enregistrement des extensions (entites, commandes, serializers).
// Les methodes d'enregistrement sont implementees cote hote (non-inline)
// afin que toutes les registrations aboutissent dans les registres globaux
// portes par libbcad_plugin.
class BCAD_PLUGIN_API PluginRegistry {
public:
    // Metadonnees du plugin, a remplir pendant bcad_plugin_init()
    PluginInfo& info() { return info_; }

    // Enregistre un type d'entite. Retourne false si le TypeId est deja pris.
    bool registerEntityType(bcad::geom::TypeId typeId, const EntityFactory& factory);

    // Enregistre une commande. Retourne false si le nom est deja pris.
    bool registerCommand(std::string_view commandName, const CommandFactory& factory);

    // Enregistre un serializer d'entite. Retourne false si le TypeId est deja traite.
    bool registerSerializer(std::unique_ptr<bcad::serialization::IEntitySerializer> serializer);

private:
    PluginInfo info_;
};

} // namespace bcad::plugin