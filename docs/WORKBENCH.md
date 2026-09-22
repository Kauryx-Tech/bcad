# Workbench BCAD

> [!IMPORTANT]
>
> ## Statut : ARCHITECTURE CIBLE — non implémentée
>
> `IWorkbench`, `WorkbenchRegistry`, `WorkbenchManager` et `Document::availableWorkbenches()`
> **n'existent pas** dans `include/bcad/` ni `src/`. Ce document est une fiche de conception
> (phase 11 de `ARCHITECTURE_ROADMAP.md`), pas une description du code.

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

- `PLUGIN_ARCHITECTURE.md` — plugins
- `COMMAND_SYSTEM.md` — commandes
- `SDK_ARCHITECTURE.md` — SDK
- `ARCHITECTURE_BENCHMARK.md` — comparaison FreeCAD