# Visite guidée du code BCAD

> [!IMPORTANT]
>
> ## Statut : MIXTE — code actuel + passages cible
>
> Les sections **1 à 3, 7 et 8** décrivent le code **actuel**. Les sections **4 à 6** décrivent des
> composants de l'**architecture cible** (outils séparés `src/app/tools`, Command C++,
> `Document::execute/undo/redo/eventBus`) qui **n'existent pas encore** dans `src/`.
> Ne pas chercher ces fichiers tels quels aujourd'hui.

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

**Fichier :** `src/app/MainWindow.cpp`

La fenêtre principale contient :
- Le menu (`QMenuBar`)
- Le ruban (`RibbonBar`)
- Le viewport central
- Le panneau de calques
- La barre de statut

```cpp
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , viewport_(new Viewport(this))
{
    setCentralWidget(viewport_);
    setupMenuBar();
    setupRibbon();
    setupLayerPanel();
    setupStatusBar();
}
```

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

**Fichier :** `src/app/tools/LineTool.cpp`

L'outil Ligne attend deux clics : départ puis arrivée.

```cpp
void LineTool::onMouseDown(const Point2& worldPos) {
    if (!hasStart_) {
        start_ = worldPos;
        hasStart_ = true;
    } else {
        auto cmd = std::make_unique<CreateLineCommand>(
            document_, start_, worldPos);
        document_.execute(std::move(cmd));
        hasStart_ = false;
    }
}
```

---

## 5. La création de l'entité

**Fichier :** `src/commands/CreateLineCommand.cpp`

Une `Command` a `execute()` et `undo()` :

```cpp
class CreateLineCommand : public Command {
public:
    void execute() override {
        auto line = std::make_unique<LineEntity>(start_, end_);
        lineId_ = document_.addEntity(std::move(line));
    }

    void undo() override {
        document_.removeEntity(lineId_);
    }
};
```

**Pattern :** Chaque modification passe par une Command pour l'undo/redo.

---

## 6. Le Document

**Fichier :** `include/bcad/core/Document.h`

Le Document est le cœur de BCAD :

```cpp
class Document {
public:
    EntityId addEntity(std::unique_ptr<Entity> e);
    void removeEntity(EntityId id);
    Entity* findEntity(EntityId id) const;

    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();

    events::EventBus& eventBus();
    layers::LayerManager& layers();

private:
    std::vector<std::unique_ptr<Entity>> entities_;
    TransactionManager txManager_;
};
```

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