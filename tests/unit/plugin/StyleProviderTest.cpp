// Point d'extension IStyleProvider : vérifie que les calques déclarés par un
// fournisseur de styles sont bien créés dans un LayerManager, et que le registre
// gère l'enregistrement/désenregistrement sans fuite.

#include "bcad/layers/LayerManager.h"
#include "bcad/plugin/StyleProvider.h"

#include <cassert>
#include <string>
#include <vector>

using namespace bcad::plugin;
using namespace bcad::layers;

namespace {

// Fournisseur fictif — déclare deux calques avec des attributs distincts.
class StubStyleProvider : public IStyleProvider {
public:
    explicit StubStyleProvider(std::string id) : id_(std::move(id)) {}

    std::string id()    const override { return id_; }
    std::string label() const override { return "stub"; }

    std::vector<LayerStyle> layerStyles() const override {
        return {
            {"stub.active",   "Active",  2, "CONTINUOUS", 0.25, true,  false, true},
            {"stub.inactive", "Inactive", 8, "DASHED",    0.18, false, true,  false},
        };
    }
    std::vector<PlotStyle>  plotStyles()  const override { return {}; }
    std::vector<TextStyle>  textStyles()  const override { return {}; }

private:
    std::string id_;
};

// Simule ce que MainWindow::applyStyleProvidersToDocument fait.
void applyToLayerManager(LayerManager& lm) {
    for (const auto* provider : StyleProviderRegistry::instance().providers()) {
        for (const auto& ls : provider->layerStyles()) {
            auto& layer = lm.createLayer(ls.name);
            layer.visible = ls.visible;
            layer.locked  = ls.locked;
        }
    }
}

} // namespace

int main() {
    auto& reg = StyleProviderRegistry::instance();
    reg.clear();

    // Enregistrement
    bool ok = reg.registerProvider(std::make_unique<StubStyleProvider>("stub.styles"));
    assert(ok && "first register must succeed");

    bool dup = reg.registerProvider(std::make_unique<StubStyleProvider>("stub.styles"));
    assert(!dup && "duplicate id must be rejected");

    assert(reg.size() == 1);
    assert(reg.find("stub.styles") != nullptr);

    // Application aux calques
    LayerManager lm;
    applyToLayerManager(lm);

    // "0" + "Active" + "Inactive" = 3 calques
    assert(lm.layers().size() == 3);

    const Layer* active = lm.find("Active");
    assert(active != nullptr);
    assert(active->visible == true);
    assert(active->locked  == false);

    const Layer* inactive = lm.find("Inactive");
    assert(inactive != nullptr);
    assert(inactive->visible == false);
    assert(inactive->locked  == true);

    // Désenregistrement
    reg.unregisterProvider("stub.styles");
    assert(reg.size() == 0);

    // Après désenregistrement, un nouveau document ne reçoit pas les calques
    LayerManager lm2;
    applyToLayerManager(lm2);
    assert(lm2.layers().size() == 1 && "only the default '0' layer");

    reg.clear();
    return 0;
}
