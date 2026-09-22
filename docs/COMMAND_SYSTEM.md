# Système de commandes BCAD

> [!IMPORTANT]
>
> ## Statut : ARCHITECTURE CIBLE — non implémentée
>
> `bcad::commands` (Command, Transaction, CommandRegistry) **n'existe pas**. L'undo/redo actuel
> repose sur Qt : commandes héritant de `QUndoCommand` dans `src/app/Commands.cpp` et
> `QUndoStack` dans `MainWindow`. Voir `ARCHITECTURE_REVIEW.md`.

> Système de commandes indépendant de l'UI. Les plugins peuvent enregistrer des commandes.

## 1. Principe

Une **Command** est une unité atomique de travail qui peut être exécutée, annulée (undo), et réexécutée (redo). Les commandes complexes peuvent être regroupées en **Transaction**.

## 2. Architecture

```cpp
namespace bcad::commands {

class Command {
public:
    virtual ~Command() = default;

    std::string name() const;           // "CreateLine", "MoveWall"
    std::string description() const;    // "Creates a new line entity"
    bool isRepeatable() const;          // si vrai, peut être répétée avec Enter

    // Exécution
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual bool canExecute() const;    // préconditions satisfaites

    // Transaction
    virtual bool isTransaction() const;  // faux par défaut
    virtual void begin() {}
    virtual void commit() {}
    virtual void rollback() {}
};

}
```

## 3. Transaction

Une transaction regroupe plusieurs commandes en une unité atomique :

```cpp
class Transaction : public Command {
public:
    bool isTransaction() const override { return true; }

    void add(std::unique_ptr<Command> cmd);
    size_t size() const;

    void begin() override;
    void commit() override;
    void rollback() override;

    void execute() override;   // appelle execute() de chaque commande
    void undo() override;      // appelle undo() de chaque commande en reverse
};

class TransactionManager {
public:
    Transaction* begin(const std::string& name);
    void commit();
    void rollback();
    bool canUndo() const;
    bool canRedo() const;
    void undo();
    void redo();
    size_t undoStackSize() const;
    size_t redoStackSize() const;
};
```

## 4. Document intégré

```cpp
// document/Document.h
class Document {
    // ...
    Transaction* beginTransaction(const std::string& name);
    void commitTransaction();
    void rollbackTransaction();
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    // ...
};
```

## 5. CommandRegistry

```cpp
namespace bcad::registry {

class CommandRegistry {
public:
    template<typename T>
    void registerCommand(const std::string& name);

    Command* create(const std::string& name) const;
    std::vector<std::string> listCommands() const;
    bool has(const std::string& name) const;
};

}
```

## 6. Exemple : commande de création de mur

```cpp
class CreateWallCommand : public Command {
public:
    explicit CreateWallCommand(document::Document& doc, const WallEntity& wall)
        : doc_(doc), wall_(wall.clone()) {}

    std::string name() const override { return "CreateWall"; }
    std::string description() const override { return "Creates a wall entity"; }
    bool isRepeatable() const override { return true; }

    void execute() override {
        doc_.addEntity(std::move(wall_));
    }

    void undo() override {
        doc_.removeEntity(wall_->id());
    }

    bool canExecute() const override { return wall_ != nullptr; }

private:
    document::Document& doc_;
    std::unique_ptr<document::Entity> wall_;
};

// Enregistrement
extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    return reg.registerCommand("CreateWall", &makeCreateWallCommand);
}
```

## 7. Exemple : transaction complexe

```cpp
auto* tx = doc.beginTransaction("CreateRoom");
tx->add(std::make_unique<CreateWallCommand>(doc, wall1));
tx->add(std::make_unique<CreateWallCommand>(doc, wall2));
tx->add(std::make_unique<CreateWallCommand>(doc, wall3));
tx->add(std::make_unique<CreateDoorCommand>(doc, door1));
doc.commitTransaction();  // undo annule les 4 commandes
```

## 8. Découplage avec Qt

Le Core utilise des commandes pures C++. L'application Qt utilise un adaptateur :

```cpp
// app/commands/QCommandAdapter.h
class QCommandAdapter : public QUndoCommand {
public:
    explicit QCommandAdapter(std::unique_ptr<commands::Command> cmd);

    void undo() override;
    void redo() override;

private:
    std::unique_ptr<commands::Command> cmd_;
};

// Viewport.cpp
void Viewport::createLine() {
    auto cmd = std::make_unique<CreateLineCommand>(doc_, p1_, p2_);
    undoStack_.pushAndExecute(new QCommandAdapter(std::move(cmd)));
}
```

## 9. Commandes de plugin

```cpp
extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    reg.registerCommand("CreateWall", &makeCreateWallCommand);
    reg.registerCommand("CreateDoor", &makeCreateDoorCommand);
    reg.registerCommand("CreateWindow", &makeCreateWindowCommand);
    return true;
}
```

L'interface GUI peut alors exécuter `CreateWall` depuis un bouton ou un raccourci.

## 10. Règles

1. Command est pur C++, pas de Qt
2. Transaction = une ou plusieurs Command
3. undo/redo sont explicites
4. Les plugins peuvent enregistrer des commandes
5. QCommandAdapter fait le pont Qt ↔ Core