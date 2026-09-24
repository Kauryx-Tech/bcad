// Cycle de vie des workbenches declares par un plugin (WORKBENCH.md lot A).
// Le point delicat n'est pas l'enregistrement mais la DESTRUCTION : l'objet est
// construit dans le DSO du plugin, l'hote doit donc le detruire pendant le
// dechargement, avant dlclose. Les compteurs de construction/destruction
// verifient precisement cette regle.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo la
// macro NDEBUG est definie et assert() n'evalue pas son argument.

#include "bcad/plugin/PluginRegistry.h"
#include "bcad/plugin/Workbench.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

using namespace bcad::plugin;

namespace {

int liveCount = 0;

class FakeWorkbench : public IWorkbench {
public:
    explicit FakeWorkbench(std::string id, std::vector<std::string> panelTitles = {"Panneau"})
        : id_(std::move(id)) {
        ++liveCount;
        for (const auto& title : panelTitles) {
            WorkbenchPanel panel;
            panel.title = title;
            WorkbenchAction action;
            action.commandName = id_ + "." + title;
            action.label = title;
            panel.actions.push_back(action);
            panels_.push_back(panel);
        }
    }

    ~FakeWorkbench() override { --liveCount; }

    std::string id() const override { return id_; }
    std::string label() const override { return "Fake " + id_; }
    std::vector<WorkbenchPanel> panels() const override { return panels_; }

private:
    std::string id_;
    std::vector<WorkbenchPanel> panels_;
};

} // namespace

int main() {
    WorkbenchRegistry& registry = WorkbenchRegistry::instance();
    registry.clear();
    assert(registry.size() == 0);
    assert(registry.find("absent") == nullptr);
    assert(liveCount == 0);

    // --- Enregistrement via le PluginRegistry passe au bcad_plugin_init ---
    PluginRegistry pluginRegistry;
    const bool nullRejected = pluginRegistry.registerWorkbench(nullptr);
    PluginRegistry emptyIdRegistry;
    auto emptyId = std::make_unique<FakeWorkbench>("");
    const bool emptyIdRejected = emptyIdRegistry.registerWorkbench(std::move(emptyId));
    assert(!nullRejected && !emptyIdRejected);
    assert(liveCount == 0); // rejetes : l'hote n'en detient aucun
    assert(emptyIdRegistry.registeredWorkbenchIds().empty());

    const bool firstOk = pluginRegistry.registerWorkbench(std::make_unique<FakeWorkbench>("cadastre"));
    const bool secondOk = pluginRegistry.registerWorkbench(std::make_unique<FakeWorkbench>("architecture"));
    assert(firstOk && secondOk);
    assert(liveCount == 2);
    assert(registry.size() == 2);
    assert(pluginRegistry.registeredWorkbenchIds().size() == 2);

    // Identifiant deja pris : refuse, la premiere instance reste en place.
    const bool duplicateRejected =
        pluginRegistry.registerWorkbench(std::make_unique<FakeWorkbench>("cadastre"));
    assert(!duplicateRejected);
    assert(liveCount == 2 && registry.size() == 2);

    const IWorkbench* found = registry.find("cadastre");
    assert(found != nullptr);
    assert(found->id() == "cadastre" && found->label() == "Fake cadastre");

    // L'ordre d'enregistrement est l'ordre d'affichage (menus, ruban).
    const auto all = registry.workbenches();
    assert(all.size() == 2);
    assert(all[0]->id() == "cadastre" && all[1]->id() == "architecture");

    // --- Panneaux : declares par le plugin, copies par l'hote ---
    const auto panels = found->panels();
    assert(panels.size() == 1);
    assert(panels[0].title == "Panneau");
    assert(panels[0].actions.size() == 1);
    assert(panels[0].actions[0].commandName == "cadastre.Panneau");

    // --- Dechargement : destruction AVANT dlclose ---
    registry.unregisterWorkbench("inconnu");
    assert(registry.size() == 2 && liveCount == 2);

    registry.unregisterWorkbench("cadastre");
    assert(registry.size() == 1);
    assert(liveCount == 1); // l'instance ne vit plus dans le DSO qui va etre decharge
    assert(registry.find("cadastre") == nullptr);
    assert(registry.find("architecture") != nullptr);

    registry.clear();
    assert(registry.size() == 0 && liveCount == 0);

    // --- Rechargement idempotent apres dechargement ---
    const bool reloadOk = registry.registerWorkbench(std::make_unique<FakeWorkbench>("cadastre"));
    const bool reloadDuplicate =
        registry.registerWorkbench(std::make_unique<FakeWorkbench>("cadastre"));
    assert(reloadOk && !reloadDuplicate);
    assert(registry.size() == 1 && liveCount == 1);
    registry.clear();
    assert(liveCount == 0);

    return 0;
}
