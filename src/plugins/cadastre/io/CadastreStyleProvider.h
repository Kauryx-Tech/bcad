#pragma once

// Fournisseur de styles cadastraux (couches, tracés, textes) — implémente
// `plugin::IStyleProvider`. Les données viennent des fichiers JSON installés
// dans le répertoire de données du module (`cadastre/templates/`).

#include "bcad/plugin/StyleProvider.h"
#include <string>
#include <vector>

namespace bcad::cadastre {

class CadastreStyleProvider final : public plugin::IStyleProvider {
public:
    std::string id() const override { return "cadastre.styles"; }
    std::string label() const override { return "Styles cadastraux (Togo)"; }

    std::vector<plugin::LayerStyle> layerStyles() const override;
    std::vector<plugin::PlotStyle> plotStyles() const override;
    std::vector<plugin::TextStyle> textStyles() const override;
};

} // namespace bcad::cadastre