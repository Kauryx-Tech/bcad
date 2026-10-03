#pragma once

// Extension UI declaree par un plugin (WORKBENCH.md, lot A). Un workbench
// groupe les commandes d'un domaine metier en panneaux que l'hote traduit en
// menu et en panneaux de ruban : le core ne connait ainsi AUCUN nom de metier
// (ADR-003, ADR-005, ADR-016 principe 4).
//
// Contrat d'ABI (PLUGIN_ARCHITECTURE.md §13) : l'objet IWorkbench est cree par
// le plugin mais detenu par l'hote, comme les serializers. Il est donc detruit
// par l'hote PENDANT le dechargement, avant dlclose, jamais apres.

// Le contrat d'ABI impose de recompiler le plugin avec le SDK courant
// (PLUGIN_API_VERSION, ADR-011).
#include "bcad/plugin/Api.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::plugin {

// Facon dont l'hote construit les arguments d'une action. Ces strategies sont
// generiques (aucune ne connait un metier) : le plugin choisit celle qui
// convient a sa commande, l'hote n'ecrit jamais de logique metier.
enum class WorkbenchParams {
    None,          // aucun argument
    SelectionIds,  // ids des entites selectionnees (le plugin valide le nombre)
    BoxSplit,      // id + ligne verticale mediane de l'emprise (5 arguments)
    Vertices,      // id + sommets de l'entite, apres saisie d'un index et d'une
                   // coordonnee (2 + 2*n arguments)
    // Ne lance aucune commande : l'hote execute les validateurs enregistres et
    // affiche leurs diagnostics. La regle verifiee reste donc chez le plugin.
    RunValidators,
    // Une seule valeur, saisie par l'utilisateur : l'hote la demande via
    // `prompt` et la passe comme unique argument. Il ne sait pas ce que la
    // valeur signifie et ne la valide pas — une saisie rejetee se traduit par
    // une factory qui rend nullptr.
    PromptText,
    // PromptText puis BoxSplit : une valeur saisie (question `prompt`), puis
    // l'id et la ligne verticale mediane de l'emprise de l'entite selectionnee,
    // soit 6 arguments [id, valeur, x1, y1, x2, y2]. L'hote ne sait pas ce que
    // la valeur compte ; la factory du plugin la valide.
    PromptBoxSplit
};

// Une action de workbench : un bouton que l'hote sait creer sans rien connaitre
// du metier. `commandName` est invoque via CommandRegistry, `params` indique
// comment l'hote rassemble les arguments.
struct WorkbenchAction {
    std::string commandName;
    std::string label;
    std::string tooltip;
    WorkbenchParams params = WorkbenchParams::None;
    // Question affichee pour PromptText. C'est le plugin qui la redige : l'hote
    // n'invente aucun libelle metier (ADR-016).
    std::string prompt;
    // Types d'entites auxquels l'action s'applique (vide = toute selection).
    // C'est le plugin qui connait ses types, pas l'hote.
    std::vector<std::string> selectedTypes;
    int minSelected = 0;   // validation generique du compte de selection
    int maxSelected = 0;   // 0 = pas de maximum
    // Commande interactive : l'hote attend son execution (une commande qui
    // ouvre une boite de saisie ne doit pas etre relancee en rafale).
    bool modal = false;
    // Faux quand l'action ne change pas le dessin — elle deplace la selection,
    // cadre la vue. L'hote ne marque alors pas le document modifie et ne pousse
    // rien dans la pile d'annulation : Ctrl+Z ne doit pas defaire une recherche.
    bool modifiesDocument = true;
    // Image du bouton : chemin d'un fichier (SVG ou PNG) livre avec le module et
    // resolu par lui dans ses repertoires de donnees ; vide = pas d'icone.
    // L'hote l'affiche sans savoir ce qu'elle represente.
    std::string icon;
    // Action majeure de son panneau : grand bouton dans le ruban. Simple indice
    // de presentation, sans effet sur l'execution.
    bool prominent = false;
};

// Un panneau = un groupe nomme d'actions (panneau de ruban, section de menu).
struct WorkbenchPanel {
    std::string title;
    std::vector<WorkbenchAction> actions;
};

// Workbench metier expose par un plugin.
class BCAD_PLUGIN_API IWorkbench {
public:
    virtual ~IWorkbench() = default;

    // Identifiant stable (prefixe du plugin, ex. "cadastre").
    virtual std::string id() const = 0;

    // Libelle affiche (menu, onglet de ruban), dans la langue du plugin.
    virtual std::string label() const = 0;

    // Description optionnelle (infobulle du menu).
    virtual std::string description() const { return {}; }

    // Panneaux et actions. L'hote les interroge au chargement et les copie :
    // un workbench ne peut donc pas changer de forme a chaud.
    virtual std::vector<WorkbenchPanel> panels() const = 0;
};

// Registre des workbenches. Singleton porte par l'executable hote (comme les
// registres d'entites, de commandes et de serializers) : un plugin ne fait
// qu'enregistrer, c'est l'hote qui detient les instances.
class BCAD_PLUGIN_API WorkbenchRegistry {
public:
    static WorkbenchRegistry& instance();

    // Faux si l'identifiant est deja pris.
    bool registerWorkbench(std::unique_ptr<IWorkbench> workbench);
    void unregisterWorkbench(const std::string& id);
    void clear();

    std::vector<const IWorkbench*> workbenches() const;
    const IWorkbench* find(std::string_view id) const;
    size_t size() const { return entries_.size(); }

    WorkbenchRegistry(const WorkbenchRegistry&) = delete;
    WorkbenchRegistry& operator=(const WorkbenchRegistry&) = delete;

private:
    WorkbenchRegistry() = default;

    std::vector<std::unique_ptr<IWorkbench>> entries_;
};

} // namespace bcad::plugin
