#pragma once

// Registre expose au plugin (ADR-005 : `bcad_plugin_init(PluginRegistry&)`).
// L'enregistrement est mediatise par l'hote (libbcad_plugin) : les extensions
// sont enregistrees dans les registres globaux depuis le DSO hote, ce qui
// garantit une seule instance des registres quel que soit le DSO.

#include "bcad/geometry/TypeId.h"
#include "bcad/geometry/Entity.h"
#include "bcad/commands/Command.h"
#include "bcad/serialization/Serializer.h"
#include "bcad/plugin/Api.h"
#include "bcad/plugin/StyleProvider.h"
#include "bcad/plugin/FileExporter.h"
#include "bcad/plugin/FileImporter.h"
#include "bcad/plugin/Workbench.h"
#include "bcad/plugin/Validator.h"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::plugin {

// Version of the plugin ABI - increment on every breaking change.
// v1 : factories std::function -> v2 : factories pointeurs de fonction bruts
// (voir aliases ci-dessous) -> v3 : extension UI `registerWorkbench` (layout
// de PluginRegistry etendu) -> v4 : extension de verification
// `registerValidator` (layout de PluginRegistry a nouveau etendu) -> v5 :
// export fichier `registerFileExporter` (layout etendu une troisieme fois) ->
// v6 : `WorkbenchAction` porte deux champs de plus (question de saisie, action
// qui ne modifie pas le dessin) et `WorkbenchParams` une strategie de plus :
// la structure traverse `panels()` entre les deux DSO, donc son layout est de
// l'ABI -> v7 : `PluginRegistry` porte les repertoires de donnees du module
// (`addDataDirectory`, `resolveDataFile`), layout une cinquieme fois etendu ->
// v8 : `Document` tient ses feuilles de mise en page. Un `std::vector` de plus
// traverse la frontiere et deplace l'offset des membres qui le suivent : une
// commande de module qui recoit un `Document&` cree par l'hote ne peut plus le
// lire avec l'ancien plan (ADR-017). Le
// v9 : mise en page déclarative (ADR-017 décisions 3+7) — `Cartouche`,
// `ParcelTable`, échelles FR et `applySuggestedScale` quittent l'API publique.
// v10 : validateurs de document (ADR-017 décision 5) — `registerDocumentValidator`
// et ses identifiants étendent à nouveau le layout de PluginRegistry.
// Le
// PluginManager refuse tout plugin dont apiVersion != PLUGIN_API_VERSION (gate
// strict, cf. ADR-011 : pas de garantie ABI inter-versions, plugins recompiles a
// chaque changement d'ABI).
constexpr int PLUGIN_API_VERSION = 11;

// Plugin metadata (remplie par le plugin dans PluginRegistry::info())
struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    int apiVersion = PLUGIN_API_VERSION;
};

// Type aliases for plugin callbacks.
// Pointeurs de fonction BRUTS (pas std::function) : l'ABI ne traverse pas un
// std::function entre DSO, sinon le manager interne est emis chez le APPELANT
// (le plugin) et le registre hote detruirait a la sortie un std::function
// pointant vers le plugin (SEGV apres dlclose). L'hote re-emballe le pointeur
// dans un std::function defini cote hote (manager hote, jamais decharge).
using EntityFactory = std::unique_ptr<bcad::geom::Entity> (*)(std::string_view params);
using CommandFactory = std::unique_ptr<bcad::commands::Command> (*)(const std::vector<std::string>& args);

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

    // Enregistre un workbench metier (menu + panneaux de ruban declares par le
    // plugin). L'hote prend la propriete de l'objet. Retourne false si l'identifiant
    // est deja pris.
    bool registerWorkbench(std::unique_ptr<IWorkbench> workbench);

    // Enregistre un validateur (regles de verification declarees par le plugin).
    // L'hote prend la propriete de l'objet. Retourne false si l'identifiant est
    // deja pris.
    bool registerValidator(std::unique_ptr<IValidator> validator);

    // Enregistre un exporteur de fichier (format d'echange declare par le plugin).
    // L'hote prend la propriete de l'objet. Retourne false si l'identifiant est
    // deja pris.
    bool registerFileExporter(std::unique_ptr<IFileExporter> exporter);

    // Enregistre un importeur de fichier (lecture d'un format externe dans le
    // Document courant). L'hote prend la propriete de l'objet. Retourne false
    // si l'identifiant est deja pris.
    bool registerFileImporter(std::unique_ptr<IFileImporter> importer);

    // Enregistre un validateur de document (regles a l'echelle du document :
    // feuilles, vues, attributs du dossier — ADR-017 decision 5). L'hote prend
    // la propriete de l'objet. Retourne false si l'identifiant est deja pris.
    bool registerDocumentValidator(std::unique_ptr<IDocumentValidator> validator);

    // Enregistre un fournisseur de styles (couches, tracés, textes). L'hote prend
    // la propriete de l'objet. Retourne false si l'identifiant est deja pris.
    bool registerStyleProvider(std::unique_ptr<IStyleProvider> provider);

    // --- Donnees livrees avec le module ---
    // L'hote remplit ces repertoires avant bcad_plugin_init (voir
    // PluginManager::addDataDirectory). Un module n'y ajoute que ce qu'il sait
    // deja nommer : il ignore ou il est installe, et ignore ou l'hote range les
    // donnees (ADR-016 : aucune règle decheminage dans le plugin).
    void addDataDirectory(const std::string& directory);

    // Chemin absolu du premier fichier `relativePath` rencontre dans les
    // repertoires de donnees, ou chaine vide. `relativePath` est de la forme
    // "<module>/templates/<profil>.json" : le prefixe est choisi par le module,
    // pas par l'hote.
    std::string resolveDataFile(const std::string& relativePath) const;

    // Les repertoires eux-memes, dans l'ordre de priorite. Un module qui nomme
    // un fichier seulement plus tard — quand l'operateur designe son profil —
    // ne peut pas capter `resolveDataFile` : les factories de commande sont des
    // pointeurs de fonction et cet objet meurt a la fin de bcad_plugin_init. Il
    // copie donc la liste pendant l'initialisation. La regle de recherche reste
    // celle de l'hote : premier repertoire qui contient le fichier.
    const std::vector<std::string>& dataDirectories() const { return dataDirs_; }

    // Ce que CE plugin a enregistre : l'hote retire ces entrees avant dlclose,
    // leur code et leurs vtables vivant dans le DSO du plugin.
    const std::vector<std::string>& registeredSerializerTypeIds() const {
        return serializerTypeIds_;
    }
    const std::vector<std::string>& registeredEntityTypeIds() const {
        return entityTypeIds_;
    }
    const std::vector<std::string>& registeredCommandNames() const {
        return commandNames_;
    }
    const std::vector<std::string>& registeredWorkbenchIds() const {
        return workbenchIds_;
    }
    const std::vector<std::string>& registeredValidatorIds() const {
        return validatorIds_;
    }
    const std::vector<std::string>& registeredDocumentValidatorIds() const {
        return documentValidatorIds_;
    }
    const std::vector<std::string>& registeredFileExporterIds() const {
        return fileExporterIds_;
    }
    const std::vector<std::string>& registeredFileImporterIds() const {
        return fileImporterIds_;
    }
    const std::vector<std::string>& registeredStyleProviderIds() const {
        return styleProviderIds_;
    }

private:
    PluginInfo info_;
    std::vector<std::string> serializerTypeIds_;
    std::vector<std::string> entityTypeIds_;
    std::vector<std::string> commandNames_;
    std::vector<std::string> workbenchIds_;
    std::vector<std::string> validatorIds_;
    std::vector<std::string> documentValidatorIds_;
    std::vector<std::string> fileExporterIds_;
    std::vector<std::string> fileImporterIds_;
    std::vector<std::string> styleProviderIds_;
    std::vector<std::string> dataDirs_;
};

} // namespace bcad::plugin