#include "CadastreWorkbench.h"

#include "Templates.h"

namespace bcad::cadastre {

namespace {

// Les icones sont des donnees du module (share/bcad/plugins/cadastre/icons),
// cherchees dans les repertoires que l'hote lui a proposes. Introuvables, le
// bouton garde son libelle seul : une installation incomplete n'est pas une
// erreur.
std::string icone(const char* nom) {
    return trouverFichierDeDonnees(std::string("cadastre/icons/") + nom + ".svg");
}

} // namespace

std::vector<plugin::WorkbenchPanel> CadastreWorkbench::panels() const {
    constexpr const char* kParcel = "cadastre.parcel";

    // Les actions sont initialisees par nom de champ : WorkbenchAction a douze
    // champs, les remplir par position laisserait passer une permutation.
    // Quatre panneaux, dans l'ordre du travail : saisir, retoucher, produire,
    // controler. Chacun a au plus une action majeure (grand bouton).
    plugin::WorkbenchPanel parcels;
    parcels.title = "Parcelles";
    parcels.actions = {
        {.commandName = "cadastre.create_parcel", .label = "Nouvelle parcelle",
         .tooltip = "Ajoute une parcelle cadastrale rectangulaire",
         .icon = icone("parcel-new"), .prominent = true},
        {.commandName = "cadastre.find_parcel", .label = "Rechercher...",
         .tooltip = "Sélectionne les parcelles d'une section et d'un numéro donnés",
         .params = plugin::WorkbenchParams::PromptText,
         .prompt = "Section et numéro (ex. A 007) :",
         .selectedTypes = {kParcel},
         .modifiesDocument = false,
         .icon = icone("parcel-find")},
    };

    plugin::WorkbenchPanel edition;
    edition.title = "Édition";
    edition.actions = {
        {.commandName = "cadastre.split_parcel", .label = "Scinder",
         .tooltip = "Scinde la parcelle sélectionnée selon une ligne verticale médiane",
         .params = plugin::WorkbenchParams::BoxSplit,
         .selectedTypes = {kParcel}, .minSelected = 1, .maxSelected = 1,
         .icon = icone("parcel-split")},
        {.commandName = "cadastre.merge_parcels", .label = "Fusionner",
         .tooltip = "Fusionne exactement deux parcelles sélectionnées",
         .params = plugin::WorkbenchParams::SelectionIds,
         .selectedTypes = {kParcel}, .minSelected = 2, .maxSelected = 2,
         .icon = icone("parcel-merge")},
        {.commandName = "cadastre.edit_parcel_boundary", .label = "Modifier la limite",
         .tooltip = "Déplace un sommet de la parcelle sélectionnée",
         .params = plugin::WorkbenchParams::Vertices,
         .selectedTypes = {kParcel}, .minSelected = 1, .maxSelected = 1,
         .modal = true,
         .icon = icone("parcel-edit-boundary")},
    };

    plugin::WorkbenchPanel livrables;
    livrables.title = "Livrables";
    livrables.actions = {
        {.commandName = "cadastre.generate_plan_sheet", .label = "Plan cadastral...",
         .tooltip = "Crée un PDF cadastral à partir du document courant",
         .modal = true,
         // Le PDF est un livrable exterieur : le dessin, lui, n'a pas change.
         .modifiesDocument = false,
         .icon = icone("plan-sheet"), .prominent = true},
        // L'hote demande une chaine et ne sait pas ce qu'un profil designe : le
        // libelle, la question et le refus d'un nom inconnu sont du module.
        // Pas `modal` : la saisie est deja finie quand la commande part, et le
        // choix du profil est une donnee du dossier, donc annulable.
        {.commandName = "cadastre.set_profile", .label = "Profil du dossier...",
         .tooltip = "Règles d'identification appliquées au dossier : le nom du profil "
                    "est celui du fichier de gabarit livré avec le module",
         .params = plugin::WorkbenchParams::PromptText,
         .prompt = "Nom du profil (ex. cadastre_togo) :",
         .icon = icone("cadastre-profile")},
    };

    // Controle qualite : aucune commande. L'hote execute les validateurs
    // enregistres par le plugin et affiche leurs diagnostics. Pas de filtre de
    // types ici : chaque validateur declare les siens (applicableTypes).
    plugin::WorkbenchPanel controle;
    controle.title = "Contrôle";
    controle.actions = {
        {.label = "Vérifier le document",
         .tooltip = "Contrôle la topologie, les emprises communes et l'identification des parcelles",
         .params = plugin::WorkbenchParams::RunValidators,
         .icon = icone("cadastre-check"), .prominent = true},
    };

    return {std::move(parcels), std::move(edition), std::move(livrables), std::move(controle)};
}

} // namespace bcad::cadastre
