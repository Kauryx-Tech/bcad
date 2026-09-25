#include "CadastreWorkbench.h"

namespace bcad::cadastre {

std::vector<plugin::WorkbenchPanel> CadastreWorkbench::panels() const {
    constexpr const char* kParcel = "cadastre.parcel";

    // Les actions sont initialisees par nom de champ : WorkbenchAction a dix
    // champs, les remplir par position laisserait passer une permutation.
    plugin::WorkbenchPanel parcels;
    parcels.title = "Parcelles";
    parcels.actions = {
        {.commandName = "cadastre.create_parcel", .label = "Créer une parcelle",
         .tooltip = "Ajoute une parcelle cadastrale rectangulaire"},
        {.commandName = "cadastre.find_parcel", .label = "Rechercher une parcelle...",
         .tooltip = "Sélectionne les parcelles d'une section et d'un numéro donnés",
         .params = plugin::WorkbenchParams::PromptText,
         .prompt = "Section et numéro (ex. A 007) :",
         .selectedTypes = {kParcel},
         .modifiesDocument = false},
        {.commandName = "cadastre.split_parcel",
         .label = "Scinder la parcelle sélectionnée",
         .tooltip = "Scinde la parcelle selon une ligne verticale médiane",
         .params = plugin::WorkbenchParams::BoxSplit,
         .selectedTypes = {kParcel}, .minSelected = 1, .maxSelected = 1},
        {.commandName = "cadastre.merge_parcels",
         .label = "Fusionner les parcelles sélectionnées",
         .tooltip = "Fusionne exactement deux parcelles sélectionnées",
         .params = plugin::WorkbenchParams::SelectionIds,
         .selectedTypes = {kParcel}, .minSelected = 2, .maxSelected = 2},
        {.commandName = "cadastre.edit_parcel_boundary",
         .label = "Modifier la limite de la parcelle",
         .tooltip = "Déplace un sommet de la parcelle sélectionnée",
         .params = plugin::WorkbenchParams::Vertices,
         .selectedTypes = {kParcel}, .minSelected = 1, .maxSelected = 1,
         .modal = true},
    };

    plugin::WorkbenchPanel documents;
    documents.title = "Documents";
    documents.actions = {
        {.commandName = "cadastre.generate_plan_sheet", .label = "Générer le plan cadastral...",
         .tooltip = "Crée un PDF cadastral à partir du document courant",
         .modal = true,
         // Le PDF est un livrable exterieur : le dessin, lui, n'a pas change.
         .modifiesDocument = false},
    };

    // Controle qualite : aucune commande. L'hote execute les validateurs
    // enregistres par le plugin et affiche leurs diagnostics. Pas de filtre de
    // types ici : chaque validateur declare les siens (applicableTypes).
    plugin::WorkbenchPanel controle;
    controle.title = "Contrôle";
    controle.actions = {
        {.label = "Vérifier le document",
         .tooltip = "Contrôle la topologie, les emprises communes et l'identification des parcelles",
         .params = plugin::WorkbenchParams::RunValidators},
    };

    return {std::move(parcels), std::move(documents), std::move(controle)};
}

} // namespace bcad::cadastre
