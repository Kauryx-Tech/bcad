#pragma once

#include "bcad/plugin/Workbench.h"

namespace bcad::cadastre {

// Workbench cadastral : declare les panneaux et commandes que l'hote doit
// exposer. Le plugin decide du metier, l'hote ne contient aucun nom de domaine
// (ADR-003, ADR-005, ADR-016).
class CadastreWorkbench final : public plugin::IWorkbench {
public:
    std::string id() const override { return "cadastre"; }
    std::string label() const override { return "Cadastre"; }
    std::string description() const override {
        return "Parcelles, limites, bornes et plans cadastraux";
    }
    std::vector<plugin::WorkbenchPanel> panels() const override;
};

} // namespace bcad::cadastre
