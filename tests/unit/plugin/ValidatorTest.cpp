// Cycle de vie des validateurs declares par un plugin. Comme pour les
// workbenches, le point delicat n'est pas l'enregistrement mais la DESTRUCTION :
// l'objet est construit dans le DSO du plugin, l'hote doit donc le detruire
// pendant le dechargement, avant dlclose. Les compteurs de construction et de
// destruction verifient precisement cette regle.
//
// L'autre moitie du test verifie que le mecanisme reste generique : un
// validateur ne recoit que des geom::Entity et ne rend que des diagnostics.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo la
// macro NDEBUG est definie et assert() n'evalue pas son argument.

#include "bcad/geometry/PointEntity.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/plugin/Validator.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

using namespace bcad::plugin;
using namespace bcad::validation;

namespace {

int liveCount = 0;

// Validateur fictif : signale toute entite dont l'identifiant est pair.
// Il ne connait aucun metier, seulement le mecanisme.
class EvenIdValidator : public IValidator {
public:
    explicit EvenIdValidator(std::string id) : id_(std::move(id)) { ++liveCount; }
    ~EvenIdValidator() override { --liveCount; }

    std::string id() const override { return id_; }
    std::string label() const override { return "Ids pairs (" + id_ + ")"; }
    std::vector<std::string> applicableTypes() const override { return types_; }

    std::vector<Diagnostic> validate(const std::vector<bcad::geom::Entity*>& entities) const override {
        std::vector<Diagnostic> out;
        for (const auto* entity : entities) {
            if (entity && entity->id() % 2 == 0) {
                out.push_back(Diagnostic{Severity::Warning, "identifiant pair", {entity->id()}});
            }
        }
        return out;
    }

    void setTypes(std::vector<std::string> types) { types_ = std::move(types); }

private:
    std::string id_;
    std::vector<std::string> types_;
};

// Entites du noyau : le validateur ne recoit que la base geom::Entity, il
// ignore totally le type reel.
bcad::geom::Entity* makeEntity(std::vector<std::unique_ptr<bcad::geom::Entity>>& owner, int id) {
    auto entity = std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{id, id});
    entity->setId(id);
    auto* raw = entity.get();
    owner.push_back(std::move(entity));
    return raw;
}

} // namespace

int main() {
    ValidatorRegistry& registry = ValidatorRegistry::instance();
    registry.clear();
    assert(registry.size() == 0);
    assert(registry.find("absent") == nullptr);
    assert(liveCount == 0);

    // --- Rejets : instance nulle et identifiant vide ---
    PluginRegistry pluginRegistry;
    const bool nullRejected = pluginRegistry.registerValidator(nullptr);
    PluginRegistry emptyIdRegistry;
    auto emptyId = std::make_unique<EvenIdValidator>("");
    const bool emptyIdRejected = emptyIdRegistry.registerValidator(std::move(emptyId));
    assert(!nullRejected && !emptyIdRejected);
    assert(liveCount == 0); // rejetes : l'hote n'en detient aucun
    assert(emptyIdRegistry.registeredValidatorIds().empty());

    // --- Enregistrement via le bcad_plugin_init du plugin ---
    const bool firstOk = pluginRegistry.registerValidator(std::make_unique<EvenIdValidator>("topo"));
    const bool secondOk = pluginRegistry.registerValidator(std::make_unique<EvenIdValidator>("limites"));
    assert(firstOk && secondOk);
    assert(liveCount == 2);
    assert(registry.size() == 2);
    assert(pluginRegistry.registeredValidatorIds().size() == 2);

    // Identifiant deja pris : refuse, la premiere instance reste en place.
    const bool duplicateRejected =
        pluginRegistry.registerValidator(std::make_unique<EvenIdValidator>("topo"));
    assert(!duplicateRejected);
    assert(liveCount == 2 && registry.size() == 2);

    // L'ordre d'enregistrement est l'ordre d'execution.
    const auto all = registry.validators();
    assert(all.size() == 2);
    assert(all[0]->id() == "topo" && all[1]->id() == "limites");

    // --- Le mecanisme est generique : entites du noyau, diagnostics en sortie ---
    std::vector<std::unique_ptr<bcad::geom::Entity>> owner;
    bcad::geom::Entity* odd = makeEntity(owner, 3);
    bcad::geom::Entity* even = makeEntity(owner, 4);
    const std::vector<bcad::geom::Entity*> entities{odd, even};
    const auto diagnostics = registry.find("topo")->validate(entities);
    assert(diagnostics.size() == 1);
    assert(diagnostics[0].severity == Severity::Warning);
    assert(diagnostics[0].entityIds.size() == 1);
    assert(diagnostics[0].entityIds[0] == 4);

    // Un validateur declare les types auxquels il s'applique ; l'hote filtre dessus.
    auto scoped = std::make_unique<EvenIdValidator>("scoped");
    scoped->setTypes({bcad::geom::TypeId_Point.value});
    assert(pluginRegistry.registerValidator(std::move(scoped)));
    const auto types = registry.find("scoped")->applicableTypes();
    assert(types.size() == 1 && types[0] == bcad::geom::TypeId_Point.value);
    assert(pluginRegistry.registeredValidatorIds().size() == 3);
    // L'entite du noyau appartient bien au champ d'application declare.
    const auto scopedDiagnostics = registry.find("scoped")->validate(entities);
    assert(scopedDiagnostics.size() == 1);

    // --- Dechargement : destruction AVANT dlclose ---
    registry.unregisterValidator("inconnu");
    assert(registry.size() == 3 && liveCount == 3);

    registry.unregisterValidator("topo");
    assert(registry.size() == 2);
    assert(liveCount == 2); // l'instance ne vit plus dans le DSO qui va etre decharge
    assert(registry.find("topo") == nullptr);
    assert(registry.find("limites") != nullptr);

    registry.clear();
    assert(registry.size() == 0 && liveCount == 0);

    // --- Rechargement idempotent apres dechargement ---
    const bool reloadOk = registry.registerValidator(std::make_unique<EvenIdValidator>("topo"));
    const bool reloadDuplicate = registry.registerValidator(std::make_unique<EvenIdValidator>("topo"));
    assert(reloadOk && !reloadDuplicate);
    assert(registry.size() == 1 && liveCount == 1);
    registry.clear();
    assert(liveCount == 0);

    return 0;
}
