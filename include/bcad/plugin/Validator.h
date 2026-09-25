#pragma once

// Extension de verification declaree par un plugin (WORKBENCH.md, ADR-003/005).
// L'hote ne connait AUCUNE regle metier : il interroge les validateurs enregistres
// et affiche les diagnostics tels qu'ils sont formule par le plugin.
//
// Contrat d'ABI (PLUGIN_ARCHITECTURE.md §13) : l'objet IValidator est cree par le
// plugin mais detenu par l'hote, comme les serializers et les workbenches. Il est
// donc detruit par l'hote PENDANT le dechargement, avant dlclose, jamais apres.

#include "bcad/geometry/Entity.h"
#include "bcad/plugin/Api.h"
#include "bcad/validation/Diagnostics.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::plugin {

// Un validateur porte sur un ensemble de types d'entites declares par son plugin.
class BCAD_PLUGIN_API IValidator {
public:
    virtual ~IValidator() = default;

    // Identifiant stable, prefixe par le plugin (ex. "cadastre.topologie").
    virtual std::string id() const = 0;

    // Libelle du lot de regles (entete de groupe dans le panneau de resultats),
    // dans la langue du plugin.
    virtual std::string label() const = 0;

    // TypeIds auxquels ce validateur s'applique (vide = toutes les entites).
    // C'est le plugin qui connait ses types, pas l'hote.
    virtual std::vector<std::string> applicableTypes() const = 0;

    // Verifie un lot d'entites et rapporte chaque probleme constate. Ne modifie
    // jamais le document.
    virtual std::vector<validation::Diagnostic> validate(
        const std::vector<bcad::geom::Entity*>& entities) const = 0;
};

// Registre des validateurs. Singleton porte par l'hote (comme WorkbenchRegistry) :
// un plugin ne fait qu'enregistrer, l'hote detient les instances.
class BCAD_PLUGIN_API ValidatorRegistry {
public:
    static ValidatorRegistry& instance();

    // Faux si l'identifiant est deja pris.
    bool registerValidator(std::unique_ptr<IValidator> validator);
    void unregisterValidator(const std::string& id);
    void clear();

    std::vector<const IValidator*> validators() const;
    const IValidator* find(std::string_view id) const;
    size_t size() const { return entries_.size(); }

    ValidatorRegistry(const ValidatorRegistry&) = delete;
    ValidatorRegistry& operator=(const ValidatorRegistry&) = delete;

private:
    ValidatorRegistry() = default;

    std::vector<std::unique_ptr<IValidator>> entries_;
};

} // namespace bcad::plugin
