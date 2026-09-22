# Architecture visuelle BCAD

> Schémas et diagrammes ASCII pour comprendre BCAD en un coup d'œil.

## Table des matières

1. [Vue d'ensemble](#vue-densemble)
2. [Flux de données](#flux-de-données)
3. [Graphe de dépendances](#graphe-de-dépendances)
4. [Cycle de vie d'une entité](#cycle-de-vie-dune-entité)
5. [Architecture du rendu](#architecture-du-rendu)
6. [Système de plugins](#système-de-plugins)
7. [Interface utilisateur](#interface-utilisateur)

---

## 1. Vue d'ensemble

```
┌─────────────────────────────────────────────────────────────────┐
│                        BCAD APPLICATION                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │    Menu     │  │    Ribbon    │  │  Toolbar    │  ← UI Layer │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘             │
│         └────────────────┼────────────────┘                       │
│                          ▼                                       │
│  ┌─────────────────────────────────────────────────┐            │
│  │                  Viewport                         │            │
│  │  ┌─────────────────────────────────────────┐    │            │
│  │  │         OpenGL Canvas (3.3)              │    │            │
│  │  │    ┌──────────────────────────────┐      │    │            │
│  │  │    │  Entities (Line/Circle/...)  │      │    │            │
│  │  │    └──────────────────────────────┘      │    │            │
│  │  └─────────────────────────────────────────┘    │            │
│  └─────────────────────────────────────────────────┘            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                         BCAD CORE                                │
│  (Indépendant de Qt, OpenGL, CGAL, formats de fichier)           │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │  Geometry    │    │   Document    │    │  Commands    │     │
│  │  (Types)     │    │  (Entities)  │    │  (Undo/Redo)│     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         ▲                   │                                   │
│         │                   ▼                                   │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
---

## 2. Flux de données

### 2.1 De l'utilisateur à l'entité

```
┌─────────┐      ┌──────────┐      ┌────────────┐      ┌──────────┐
│ Clic    │ ───► │ Tool     │ ───► │ Command    │ ───► │ Document │
│ utilisateur│     │ Handler  │      │ (execute)  │      │  (add)   │
└─────────┘      └──────────┘      └────────────┘      └────┬─────┘
                                                             │
                                                             ▼
┌─────────┐      ┌──────────┐      ┌────────────┐      ┌──────────┐
│  UI     │ ◄─── │ OpenGL   │ ◄─── │ Tessellation│ ◄─── │ Entity   │
│ Update  │      │ Render   │      │ (triangles) │      │ (geom)   │
└─────────┘      └──────────┘      └────────────┘      └──────────┘
```

### 2.2 Flux undo/redo

```
┌──────────────────────────────────────────────────────────────┐
│                    Transaction Stack                            │
│   ┌─────────┐    ┌─────────┐    ┌─────────┐               │
---

## 3. Graphe de dépendances

### 3.1 Actuel (avec violations)

```
                    ┌──────────────┐
                    │     app      │
                    └──────┬───────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ▼                  ▼                  ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│    io        │    │   render     │    │    core      │ ← Dépend de
│   (DXF)      │    │  (OpenGL)    │    │ (Document)   │   render! ✗
└──────┬───────┘    └──────────────┘    └──────┬───────┘
       │                                      │
       └────────────────┬───────────────────┘
                        ▼
                 ┌──────────────┐
                 │  geometry    │
                 │   (CGAL)     │
                 └──────────────┘
```

### 3.2 Cible (sans violations)

```
┌──────────────────────────────────────────────────────────────┐
│                         app                                  │
---

## 4. Cycle de vie d'une entité

```
┌─────────────────────────────────────────────────────────────┐
│                    ENTITY LIFECYCLE                          │
│                                                              │
│  1. CRÉATION                                                │
│  ┌─────────────┐                                            │
│  │ User clicks │                                            │
│  └──────┬──────┘                                            │
│         ▼                                                    │
│  ┌─────────────┐    ┌─────────────┐                        │
│  │ Tool starts │───►│ User clicks │                        │
│  └─────────────┘    └──────┬──────┘                        │
│                            ▼                                 │
│  2. GÉOMÉTRIE                                               │
│  ┌─────────────────────────────────────────┐                │
│  │ Validate input + Calculate bounds        │                │
│  └──────────────────┬────────────────────┘                │
│                     ▼                                       │
│  3. COMMAND                                                 │
│  ┌─────────────────────────────────────────┐                │
│  │ CreateCommand(doc, geometry)             │                │
│  │   └─► doc.addEntity()                   │                │
│  │       └─► index.insert()                │                │
│  │       └─► EventBus.emit()               │                │
│  └──────────────────┬────────────────────┘                │
│                     ▼                                       │
│  4. RENDU                                                   │
│  ┌─────────────────────────────────────────┐                │
---

## 5. Système de plugins

```
┌─────────────────────────────────────────────────────────────────┐
│                     PLUGIN ARCHITECTURE                          │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                     BCAD Application                      │   │
│  │  ┌──────────────┐    ┌──────────────┐                   │   │
│  │  │ PluginManager│    │ EventBus     │                   │   │
│  │  └──────────────┘    └──────────────┘                   │   │
│  └──────────────────────────┬───────────────────────────────┘   │
│                             │ dlopen()                          │
│         ┌───────────────────┼───────────────────┐              │
│         ▼                   ▼                   ▼              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐      │
│  │ Architecture│    │   Civil     │    │ Mechanical  │      │
│  │   Plugin    │    │   Plugin    │    │   Plugin    │      │
│  │ - Wall      │    │ - Road      │    │ - Gear      │      │
│  │ - Door      │    │ - Bridge    │    │ - Bolt      │      │
│  └─────────────┘    └─────────────┘    └─────────────┘      │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ extern "C" void bcad_plugin_init(PluginRegistry& reg)    │   │
│  │ {                                                         │   │
│  │     reg.entityRegistry().registerType<MyEntity>();        │   │
│  │ }                                                         │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Interface utilisateur

### Structure de la fenêtre

```
┌─────────────────────────────────────────────────────────────────┐
│ Menu: Fichier | Édition | Affichage | Dessiner | Modifier | Aide  │
├─────────────────────────────────────────────────────────────────┤
│ Ribbon: [Dessiner]│[Modifier]│[Vue]│[Format]                    │
│         ┌────┐┌────┐┌────┐┌────┐┌────┐                         │
│         │Ligne││Cercle││Arc││Polyligne││Point│                 │
├─────────┴──────┴──────────────────────────────────────────────┤
│         │                                                        │
│ Layers  │            VIEWPORT (OpenGL Canvas)                    │
│ ┌──────┐│  ┌────────────────────────────────────────────────┐   │
│ │Calque1││  │                                                │   │
│ │[👁][🔒]││  │                  Grid                       │   │
│ ├──────┤│  │           ┌─────────────────┐                 │   │
│ │Calque2││  │           │   Entities     │                 │   │
│ └──────┘│  │           └─────────────────┘                 │   │
│         │  └────────────────────────────────────────────────┘   │
├─────────┴───────────────────────────────────────────────────────┤
│ Status: X=123.45  Y=678.90  | OSNAP: ON  | GRID: ON            │
└─────────────────────────────────────────────────────────────────┘
```

### Flux de sélection d'outil

```
┌─────────────────────────────────────────────────────────────────┐
│                 TOOL SELECTION FLOW                              │
│                                                                  │
│  User clicks tool                                               │
│       │                                                          │
│       ▼                                                          │
│  Viewport::setActiveTool(Tool*)                                 │
│   ├─► tool->activate()                                         │
│   ├─► setCursor(tool->cursor())                                │
│   └─► update()                                                 │
│       │                                                          │
│       ▼                                                          │
│  Tool events:                                                   │
│   mousePress  ──► tool->onMouseDown()                          │
│   mouseMove   ──► tool->onMouseMove()                          │
│   mouseRelease──► tool->onMouseUp()                             │
│       │                                                          │
│       ▼                                                          │
│  Tool creates Command:                                          │
│   auto cmd = std::make_unique<CreateLineCommand>(p1, p2)        │
│   document.execute(std::move(cmd))                               │
└─────────────────────────────────────────────────────────────────┘
```

---

## 7. Architecture du rendu

```
┌─────────────────────────────────────────────────────────────────┐
│                        RENDER PIPELINE                          │
│  ┌──────────────┐                                              │
│  │   Document   │                                              │
│  └──────┬───────┘                                              │
│         ▼                                                       │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    SceneExtractor                          │   │
│  │   1. Query spatial index for visible region               │   │
│  │   2. Get entities in frustum                              │   │
│  │   3. Filter by layer visibility                          │   │
│  └──────────────────────────┬─────────────────────────────────┘   │
│                             ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    Tessellator                            │   │
│  │   ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐ │   │
│  │   │  Line   │  │ Circle  │  │   Arc   │  │Polyline │ │   │
│  │   └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘ │   │
│  │        └─────────────┴─────────────┴─────────────┘       │   │
│  │   ┌─────────────────────────────────────────────┐        │   │
│  │   │          Triangles + Indices + Colors       │        │   │
│  │   └─────────────────────────────────────────────┘        │   │
│  └──────────────────────────┬─────────────────────────────────┘   │
│                             ▼                                    │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                 IRenderBackend (OpenGL)                    │   │
│  │   Upload to GPU  ──►  VBOs                              │   │
│  │   Set uniforms    ──►  Matrices, Colors                  │   │
│  │   Draw           ──►  glDrawElements                    │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Voir aussi

| Document | Description |
|----------|-------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Documentation textuelle |
| [ARCHITECTURE_DECISIONS.md](ARCHITECTURE_DECISIONS.md) | ADR |
| [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md) | Détails plugins |
| [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) | Détails rendu |
| `docs/schemas/` | Fichiers Draw.io sources |
