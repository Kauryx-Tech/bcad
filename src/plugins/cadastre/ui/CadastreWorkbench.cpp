#include "CadastreWorkbench.h"

namespace bcad::cadastre {

std::vector<plugin::WorkbenchPanel> CadastreWorkbench::panels() const {
    constexpr const char* kParcel = "cadastre.parcel";

    plugin::WorkbenchPanel parcels;
    parcels.title = "Parcelles";
    parcels.actions = {
        {"cadastre.create_parcel", "Créer une parcelle",
         "Ajoute une parcelle cadastrale rectangulaire",
         plugin::WorkbenchParams::None, {}, 0, 0, false},
        {"cadastre.split_parcel", "Scinder la parcelle sélectionnée",
         "Scinde la parcelle selon une ligne verticale médiane",
         plugin::WorkbenchParams::BoxSplit, {kParcel}, 1, 1, false},
        {"cadastre.merge_parcels", "Fusionner les parcelles sélectionnées",
         "Fusionne exactement deux parcelles sélectionnées",
         plugin::WorkbenchParams::SelectionIds, {kParcel}, 2, 2, false},
        {"cadastre.edit_parcel_boundary", "Modifier la limite de la parcelle",
         "Déplace un sommet de la parcelle sélectionnée",
         plugin::WorkbenchParams::Vertices, {kParcel}, 1, 1, true},
    };

    plugin::WorkbenchPanel documents;
    documents.title = "Documents";
    documents.actions = {
        {"cadastre.generate_plan_sheet", "Générer le plan cadastral...",
         "Crée un PDF cadastral à partir du document courant",
         plugin::WorkbenchParams::None, {}, 0, 0, true},
    };

    return {std::move(parcels), std::move(documents)};
}

} // namespace bcad::cadastre
