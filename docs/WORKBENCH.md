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
enum class WorkbenchParams { None, SelectionIds, BoxSplit, Vertices };

struct WorkbenchAction {
    std::string commandName, label, tooltip;
    WorkbenchParams params = WorkbenchParams::None;
    std::vector<std::string> selectedTypes;  // le plugin connait ses types
    int minSelected = 0, maxSelected = 0;     // 0 max = pas de maximum
    bool modal = false;                       // l'hote attend l'execution
};

struct WorkbenchPanel { std::string title; std::vector<WorkbenchAction> actions; };

class IWorkbench {
public:
    virtual std::string id() const = 0;         // "cadastre"
    virtual std::string label() const = 0;      // "Cadastre"
    virtual std::vector<WorkbenchPanel> panels() const = 0;
};
```

L'hôte ne connaît que ces quatre stratégies de construction d'arguments
(`WorkbenchParams`) : aucune logique métier n'est écrite côté application. Le
plugin choisit celle qui convient à sa commande.

Le dépôt est médiatisé comme les autres registres (ADR-005) : `WorkbenchRegistry`
est un singleton porté par l'exécutable hôte, `PluginRegistry::registerWorkbench`
y enrôle l'instance. L'objet est construit dans le DSO du plugin mais **détenu par
l'hôte**, qui le retire au déchargement **avant** `dlclose` (même règle que les
serializers). Toute cassure de ce layout d'ABI incrémente `PLUGIN_API_VERSION`
(v3 depuis l'extension UI).

### Dessin de la phase 11 (non implémenté)

### IWorkbench

```cpp
class IWorkbench {
public:
    virtual ~IWorkbench() = default;
    
    std::string id() const;
    std::string name() const;
    std::string icon() const;
    
    std::vector<const Command*> commands() const;
    std::vector<const Tool*> tools() const;
    
    void activate();
    void deactivate();
    
    bool supports(const Document& doc) const;
};
```

### WorkbenchRegistry

```cpp
class WorkbenchRegistry {
public:
    void registerWorkbench(std::unique_ptr<IWorkbench> wb);
    void unregisterWorkbench(std::string_view id);
    
    IWorkbench* get(std::string_view id) const;
    std::vector<IWorkbench*> all() const;
    
    IWorkbench* defaultWorkbench() const;
    void setDefaultWorkbench(std::string_view id);
};
```

### IWorkbenchListener

```cpp
class IWorkbenchListener {
public:
    virtual void onWorkbenchActivated(const IWorkbench& wb) = 0;
    virtual void onWorkbenchDeactivated(const IWorkbench& wb) = 0;
};
```

## Workbenches intégrés

### 1. Draft ( Esquisse )
Outils de dessin 2D de base.
- Line, Polyline, Arc, Circle, Rectangle
- Move, Copy, Rotate, Scale
- Snap, Grid

### 2. Architecture
Outils architecturaux.
- Wall, Column, Beam, Slab
- Dimension, Annotation
- Section, Elevation

### 3. Part ( Mécanique )
Opérations 3D de base.
- Extrude, Revolve, Loft
- Boolean (Union, Cut, Common)
- Chamfer, Fillet

### 4. Render
Configuration du rendu.
- Materials, Lighting
- Camera presets
- Export image

## Implémentation

### WorkbenchManager

```cpp
class WorkbenchManager : public IWorkbenchListener {
public:
    static WorkbenchManager& instance();
    
    void switchTo(std::string_view id);
    void switchToDefault();
    
    IWorkbench* current() const { return current_; }
    
    void addListener(IWorkbenchListener* l);
    void removeListener(IWorkbenchListener* l);
    
private:
    WorkbenchRegistry registry_;
    IWorkbench* current_ = nullptr;
    std::vector<IWorkbenchListener*> listeners_;
};
```

### Enregistrement par plugin

```cpp
extern "C" bool bcad_plugin_init(PluginRegistry& reg) {
    reg.registerWorkbench(std::make_unique<MyWorkbench>());
}
```

## UI

### Toolbar
Outils actifs du Workbench courant. Changement dynamique.

### Menu Workbench
Menu déroulant pour basculer.

### Panel latéral
Ressources spécifiques au Workbench (calques, styles, matériaux).

### Status bar
Indique le Workbench actif et le mode.

## Intégration avec Document

```cpp
class Document {
public:
    // Filtre les Workbenches disponibles pour ce document
    std::vector<IWorkbench*> availableWorkbenches() const;
    
    // Suggestion basée sur le contenu
    IWorkbench* suggestedWorkbench() const;
};
```

## Migration

Phase 11 de la roadmap : Workbench System.

## Voir aussi

- `PLUGIN_DOMAINS.md` — quels domaines métiers utiliseront ce mécanisme
- `PLUGIN_ARCHITECTURE.md` — plugins
- `COMMAND_SYSTEM.md` — commandes
- `SDK_ARCHITECTURE.md` — SDK
- `ARCHITECTURE_BENCHMARK.md` — comparaison FreeCAD