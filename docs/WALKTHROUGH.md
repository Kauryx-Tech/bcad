# Visite guidée du code BCAD

> [!IMPORTANT]
>
> ## Statut : code actuel, sections 4-6 = flux pédagogique (API réelle notée)
>
> Les sections **1 à 3, 7 et 8** décrivent le code **actuel**. Les sections
> **4 à 6** présentent le flux *outil → commande → document* de façon
> pédagogique : l'API exacte a **divergé** depuis (commandes pures C++
> `bcad::commands::Command` via `CommandRegistry`, `Document::addEntity`
> retourne un `geom::Entity*`, pas d'`eventBus()` sur le Document) — chaque
> section indique les fichiers/signatures réels.

> Promenade pas-à-pas à travers le code, du main() à la géométrie.

## Sommaire

1. [Point d'entrée](#1-point-dentrée)
2. [La fenêtre principale](#2-la-fenêtre-principale)
3. [Le Viewport](#3-le-viewport)
4. [L'outil Ligne](#4-loutil-ligne)
5. [La création de l'entité](#5-la-création-de-lentité)
6. [Le Document](#6-le-document)
7. [Le rendu OpenGL](#7-le-rendu-opengl)
8. [Le test smoke](#8-le-test-smoke)
9. [Résumé visuel](#9-résumé-visuel)

---

## 1. Point d'entrée

**Fichier :** `src/app/main.cpp`

```cpp
int main(int argc, char** argv) {
    QApplication app(argc, argv);  // 1. Initialise Qt
    MainWindow window;               // 2. Crée la fenêtre
    window.show();                   // 3. Affiche
    return app.exec();               // 4. Boucle événements
}
```

**Questions :**
- Que se passe-t-il avant la boucle d'événements ?
- Quel est le rôle de `QApplication` ?

---

## 2. La fenêtre principale

**Fichiers :** `src/app/MainWindow.cpp` et ses quatre voisins — `MainWindowTools.cpp`
(outils et leur table), `MainWindowMenus.cpp` (menus et ruban), `MainWindowPlugins.cpp`
(workbenches, validateurs, exporteurs), `MainWindowDocument.cpp` (document, fichiers,
autosauvegarde, impression). Une seule classe, cinq unités de traduction par
responsabilité ; l'en-tête `Q_OBJECT` reste unique.

La fenêtre principale contient :
- Le menu (`QMenuBar`)
- Le ruban (`RibbonBar`)
- Le viewport central
- Le panneau de calques
- La barre de statut

```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    document_ = std::make_unique<core::Document>();
    viewport_ = new Viewport(this);
    ribbon_ = new RibbonBar(this);
    // le ruban et le canevas partagent le widget central de QMainWindow
    setCentralWidget(central);

    buildToolActions();       // une QAction par outil, partagée menu/ruban
    buildMenusAndRibbon();    // ne fait que référencer ces QAction
    buildDockWidgets();       // Calques, Propriétés, Vérifications
    buildCommandLine();
    applyDarkTheme();
    // … puis découverte et chargement des modules metiers, dont les menus
    // s'inserent avant « Aide » (buildPluginMenus)
}
```

Les `setup*` n'existent pas : les outils sont construits avant les menus parce
que ces derniers ne font que référencer les mêmes objets `QAction`.

---

## 3. Le Viewport

**Fichier :** `src/app/Viewport.cpp`

Le Viewport est un `QOpenGLWidget` qui dessine la scène.

```cpp
void Viewport::mousePressEvent(QMouseEvent* e) {
    Point2 worldPos = camera_.screenToWorld(e->pos());
    if (activeTool_) {
        activeTool_->onMouseDown(worldPos);
    }
}
```

**Architecture :**
- Hérite de `QOpenGLWidget`
- Possède une `Camera2D`
- Délègue les événements souris à un `Tool*` actif
- Dessine via OpenGL dans `paintGL()`

---

## 4. L'outil Ligne

**Fichier réel :** pas de `src/app/tools/` ; les outils sont un `enum ToolMode`
piloter par le `Viewport` (`src/app/Viewport.cpp`) et la ruban (`RibbonBar`).
Le flux ci-dessous est le flux conceptuel :

```cpp
void LineTool::onMouseDown(const Point2& worldPos) {
    if (!hasStart_) {
        start_ = worldPos;
        hasStart_ = true;
    } else {
        auto cmd = std::make_unique<CreateLineCommand>(start_, worldPos);
        registry.execute(std::move(cmd), document_);
        hasStart_ = false;
    }
}
```

---

## 5. La création de l'entité

**Fichier réel :** `src/commands/CommandsModule.cpp` + reg. par `CommandRegistry`
(`include/bcad/commands/CommandRegistry.h`, via `registerCommand`). Une
`bcad::commands::Command` a `execute(doc)` et `undo(doc)` (ADR-009, pures C++) :

```cpp
class CreateLineCommand : public bcad::commands::Command {
public:
    void execute(bcad::core::Document& document) override {
        auto line = std::make_unique<LineEntity>(start_, end_);
        lineId_ = document.addEntity(std::move(line))->id();
    }

    void undo(bcad::core::Document& document) override {
        if (lineId_ >= 0) document.removeEntity(lineId_);
    }
};
```

**Pattern :** chaque modification passe par une Command pour l'undo/redo.

---

## 6. Le Document

**Fichier :** `include/bcad/core/Document.h`

Le Document est le cœur de BCAD (signatures réelles) :

```cpp
class Document {
public:
    geom::Entity* addEntity(std::unique_ptr<geom::Entity> entity);
    void removeEntity(int id);
    void notifyEntityChanged(geom::Entity* entity);
    geom::Entity* findEntity(int id) const;
    geom::Entity* pickEntity(const geom::Point2& p, double tolerance) const;

    layers::LayerManager& layerManager();
    const index::ISpatialIndex& spatialIndex() const;
    TessellationResult buildTessellation(...) const;

private:
    std::vector<std::unique_ptr<geom::Entity>> entities_;
    layers::LayerManager layers_;
    std::unique_ptr<index::ISpatialIndex> index_;   // QuadtreeIndex
    std::shared_mutex mutex_;                        // lecture parallèle / écriture ex.
};
```

> Le Document n'a pas `execute/undo/redo` : l'undo/redo vit dans
> `bcad::commands` (`CommandRegistry`), l'historique applicatif dans
> `src/app/Commands.cpp`. Les événements document (ajout/suppression/
> modification) sont publiés sur `bcad::events::EventBus::instance()`.

---

## 7. Le rendu OpenGL

**Fichier :** `src/render/GlRenderer.cpp` (OpenGL 3.3)

> Note : le pipeline `SceneExtractor → Tessellator` ci-dessous est l'architecture **cible**.
> Aujourd'hui la tessellation est produite par `Document::buildTessellation` et rendue via
> `GlRenderer`, avec `TessellationWorker` sur un thread dédié.

```cpp
void Viewport::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    auto scene = sceneExtractor_.extract(
        document_, camera_, lod_);

    glBufferData(GL_ARRAY_BUFFER, ...);
    glDrawElements(GL_TRIANGLES, count, ...);
}
```

**Pipeline :**
```
Document → SceneExtractor → Tessellator → OpenGL
```

---

## 8. Le test smoke

**Fichier :** `tests/smoke_test.cpp`

```cpp
bool test_geometry_module() {
    geom::Point2 a(0, 0), b(3, 4);
    return std::abs(geom::distance(a, b) - 5.0) < 1e-9;
}

bool test_core_module() {
    Document doc;
    auto line = std::make_unique<LineEntity>(
        Point2{0, 0}, Point2{10, 0});
    EntityId id = doc.addEntity(std::move(line));
    return doc.findEntity(id) != nullptr;
}
```

---

## 9. Résumé visuel

```
main.cpp
   │
   ▼
MainWindow ─────► MenuBar / RibbonBar
   │
   ▼
Viewport ─────► Tool (LineTool)
   │              │
   │              ▼
   │          Command (CreateLineCommand)
   │              │
   ▼              ▼
paintGL()      Document
   │              │
   ▼              ▼
Renderer ◄──── LayerManager / Quadtree
```

---

## Pour aller plus loin

| Thème | Document |
|-------|----------|
| Géométrie détaillée | [GEOMETRY_ARCHITECTURE.md](GEOMETRY_ARCHITECTURE.md) |
| Commandes | [COMMAND_SYSTEM.md](COMMAND_SYSTEM.md) |
| Rendu | [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) |
| Document | [DOCUMENT_MODEL.md](DOCUMENT_MODEL.md) |
| Vue d'ensemble | [VISUAL_ARCHITECTURE.md](VISUAL_ARCHITECTURE.md) |

---

## Voir aussi

- [ARCHITECTURE.md](ARCHITECTURE.md)
- [GETTING_STARTED.md](GETTING_STARTED.md)