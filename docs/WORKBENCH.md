# Workbench BCAD

> [!IMPORTANT]
>
> ## Statut : partiellement implémenté
>
> **Implémenté (lot A)** — la déclaration statique : `IWorkbench`,
> `WorkbenchPanel`, `WorkbenchAction`, `WorkbenchParams`, `WorkbenchRegistry`
> (`include/bcad/plugin/Workbench.h`), `PluginRegistry::registerWorkbench`, et la
> traduction hôte en menus + panneaux de ruban (`MainWindow::buildPluginMenus()`).
> Le plugin cadastre l'utilise : il n'y a plus un seul littéral métier dans
> `src/app/`. Cycle de vie vérifié par `workbench_test`.
>
> **Non implémenté** — tout ce qui est *dynamique* dans cette fiche :
> `WorkbenchManager`, activation/désactivation, `IWorkbenchListener`, bascule
> d'onglet à l'exécution, `Document::availableWorkbenches()`. Un workbench ne peut
> pas encore changer de forme à chaud : l'hôte **copie** les panneaux au
> chargement. Les signatures ci-dessous sont donc un dessin de phase 11 ; la
> contract en vigueur est `include/bcad/plugin/Workbench.h`.

> Modèle Workbench pour organiser les outils par domaine métier (inspiré FreeCAD/KEEP).

## Concept

Un **Workbench** est un regroupement d'outils, commandes, et ressources adaptés à un domaine métier. L'utilisateur bascule entre Workbenches pour changer de contexte (architecture, mécanique, esquisse, rendu...).

## Comparaison

| Plateforme | Concept | Notes |
|------------|---------|-------|
| FreeCAD | Workbench | Très flexible, scripts Python |
| Kicad | Non, barre d'outils contextuelle | Plus simple |
| Blender | Workspace | Layouts multiples, tabs |
| QCAD | Non, menus contextuels | Limité |
| BCAD (cible) | Workbench dynamique | Plugins enregistrent |

## Architecture

### Contrat en vigueur (lot A) — déclaration statique

```cpp
// include/bcad/plugin/Workbench.h
enum class WorkbenchParams {
    None, SelectionIds, BoxSplit, Vertices, RunValidators, PromptText
};

struct WorkbenchAction {
    std::string commandName, label, tooltip;
    WorkbenchParams params = WorkbenchParams::None;
    std::string prompt;                       // question affichee pour PromptText
    std::vector<std::string> selectedTypes;  // le plugin connait ses types
    int minSelected = 0, maxSelected = 0;     // 0 max = pas de maximum
    bool modal = false;                       // l'hote attend l'execution
    bool modifiesDocument = true;             // faux = ni modification ni undo
};

struct WorkbenchPanel { std::string title; std::vector<WorkbenchAction> actions; };

class IWorkbench {
public:
    virtual std::string id() const = 0;         // "cadastre"
    virtual std::string label() const = 0;      // "Cadastre"
    virtual std::vector<WorkbenchPanel> panels() const = 0;
};
```

L'hôte ne connaît que ces six stratégies de construction d'arguments
(`WorkbenchParams`) : aucune logique métier n'est écrite côté application. Le
plugin choisit celle qui convient à sa commande.

`RunValidators` est la seule qui ne déclenche pas de commande : l'hôte exécute
les `IValidator` enregistrés (voir `PLUGIN_ARCHITECTURE.md` §4) sur la sélection
courante, ou sur tout le document si rien n'est sélectionné, et filtre par
`applicableTypes()`. Le libellé de l'action, comme les messages des diagnostics,
reste écrit par le plugin ; l'hôte ne fait que les afficher dans le dock
« Vérifications », où un double-clic sélectionne les entités en cause.

`PromptText` est la stratégie des commandes qui ont besoin d'une valeur que seul
l'utilisateur connaît — la référence d'une parcelle à rechercher, par exemple.
L'hôte ouvre une saisie de **texte libre** libellée par `prompt` (rédigé par le
plugin, pour que la question elle-même ne soit pas un littéral métier de
l'application), transmet la valeur comme unique argument, et ne la valide pas :
une saisie refusée se traduit par une factory rendant `nullptr`, donc par un
message de statut. C'est le prix d'un canal générique — l'hôte ne peut pas
conseiller la forme attendue avant l'envoi, seulement après rejet.

`modifiesDocument` sépare les actions qui changent le dessin de celles qui ne
font que déplacer la sélection ou produire un livrable extérieur. Une recherche
qui mettrait le document « modifié » proposerait de l'enregistrer pour rien, et
`Ctrl+Z` déferait la recherche au lieu du dernier tracé : une action à `false`
n'entre donc pas dans la pile d'annulation et ne marque pas le document. Le
champ est écrit par le plugin, qui seul sait ce que sa commande touche.

Le dépôt est médiatisé comme les autres registres (ADR-005) : `WorkbenchRegistry`
est un singleton porté par l'exécutable hôte, `PluginRegistry::registerWorkbench`
y enrôle l'instance. L'objet est construit dans le DSO du plugin mais **détenu par
l'hôte**, qui le retire au déchargement **avant** `dlclose` (même règle que les
serializers, les validateurs et les exporteurs de fichier). Toute cassure de ce
layout d'ABI incrémente
`PLUGIN_API_VERSION` (v3 depuis l'extension UI, v4 depuis l'extension de
vérification, v5 depuis l'extension d'export, v6 depuis les deux champs de
`WorkbenchAction` — `prompt` et `modifiesDocument`). L'historique complet est tenu
dans `API_ABI_POLICY.md` §5.2.

### Ce que le lot A ne fait PAS

Rien de ce qui suit n'est implémenté, et aucune de ces APIs n'existe dans le
dépôt (vérifié dans `include/bcad/plugin/Workbench.h`) :

- pas d'`activate()` / `deactivate()`, pas de workbench « courant » ni de
  `WorkbenchManager`, pas d'`IWorkbenchListener` ;
- pas de `Tool` : un panneau déclare des `WorkbenchAction` référencant une
  **commande** déjà enregistrée, ou une action sans commande (`RunValidators`) ;
- pas de filtrage par document (`supports(doc)`, `suggestedWorkbench()`) ;
- pas d'icônes : `WorkbenchAction` n'a pas de champ icône, le ruban est textuel.

Le dessin initial prévoyait aussi des workbenches « Draft », « Architecture »,
« Part (mécanique 3D) » et « Render ». Ces noms ne sont pas des engagements :
ADR-016 fixe une cible 2D hors-ligne dont le livrable est un document imprimable,
et le seul workbench déclaré dans le dépôt est celui du module cadastral. Les
domaines candidats sont listés dans `PLUGIN_DOMAINS.md`.

## Voir aussi

- `PLUGIN_ARCHITECTURE.md` — points d'extension du plugin (§4), dont les validateurs
- `PLUGIN_DOMAINS.md` — quels domaines métiers utiliseront ce mécanisme
- `COMMAND_SYSTEM.md` — commandes
- `SDK_ARCHITECTURE.md` — SDK
- `ARCHITECTURE_BENCHMARK.md` — comparaison FreeCAD
