# BCAD Command Pattern

Ce document décrit comment écrire une nouvelle commande dans BCAD, dans le respect de l'ADR-009 (commandes et transactions pures C++) et de l'architecture Core + SDK + plugins.

## 1. Rôle d'une commande dans BCAD

Une commande (`bcad::commands::Command`) représente une **action métier annulable et refaisable** sur un document :

- Ajouter, supprimer, transformer une entité.
- Changer un calque, une couleur, une propriété géométrique, etc.

Caractéristiques :

- **Pure C++** : aucune dépendance à Qt ou à l'UI dans le core.
- Utilisée via :
  - `CommandStack` : pile undo/redo.
  - `Transaction` : regroupement de plusieurs commandes en une seule opération atomique.
- Conçue pour être **clonable** et **fusionnable** (optionnellement) pour supporter l'undo/redo et l'optimisation de l'historique.

---

## 2. Structure type d'une commande

Une commande concrète hérite de `bcad::commands::Command` :

```cpp
namespace bcad::commands {

class MyCommand : public Command {
public:
    // Constructeur public "métier"
    MyCommand(/* paramètres */);

    // Méthodes virtuelles
    std::string_view text() const override;
    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;
    bool mergeWith(const Command& other) override;
    std::unique_ptr<Command> clone() const override;

private:
    // Champs internes
};

} // namespace bcad::commands
```

### Membres typiques

- `int entityId_` : identifiant de l'entité ciblée (jamais de `Entity*` dans l'interface publique).
- Données métier :
  - `std::string oldLayer_, newLayer_;`
  - `std::optional<geom::Color> oldColor_, newColor_;`
  - `geom::Transform2D transform_, inverse_;`
  - etc.
- `std::string text_` : texte descriptif pour l'UI (ex. "Change Layer", "Transform").
- Éventuellement un snapshot / état capturé (ex. `restoredEntity_`, `oldWidth_`, etc.).

**Règle importante :**  
Aucun pointeur nu `geom::Entity*` ne doit apparaître dans l'interface publique d'une commande. On travaille uniquement avec des `entityId` et des valeurs.

---

## 3. Constructeurs

### 3.1. Constructeur public "métier"

C'est le constructeur utilisé par le code applicatif pour créer la commande.

Il prend :

- Un `int entityId` (et non un `Entity*`).
- Les données nécessaires à l'opération (nouveau calque, nouvelle couleur, transformation, etc.).
- Éventuellement un texte descriptif par défaut.

Exemple :

```cpp
class ChangeLineWidthCommand : public Command {
public:
    ChangeLineWidthCommand(int entityId, double oldWidth, double newWidth, std::string text = "Change Line Width");
    // ...
};
```

### 3.2. Constructeur de clone

Pour supporter `clone()`, chaque commande expose un **second constructeur public** qui prend **tout l'état interne** nécessaire pour reconstruire exactement la même commande.

Exemple :

```cpp
class ChangeLineWidthCommand : public Command {
public:
    // Constructeur métier
    ChangeLineWidthCommand(int entityId, double oldWidth, double newWidth, std::string text = "Change Line Width");

    // Constructeur de clone (utilisé par clone())
    ChangeLineWidthCommand(int entityId, double oldWidth, double newWidth, std::string text);

    // ...
};
```

Ce constructeur est appelé depuis `clone()` :

```cpp
std::unique_ptr<Command> clone() const override {
    return std::make_unique<ChangeLineWidthCommand>(entityId_, oldWidth_, newWidth_, text_);
}
```

---

## 4. Méthodes à implémenter

Toute commande doit implémenter :

### 4.1. `text()`

Retourne un texte descriptif utilisé par l'UI (menus, historique undo/redo, etc.).

```cpp
std::string_view text() const override {
    return text_;
}
```

### 4.2. `execute(Document&)`

Exécute la commande (action principale et "redo").

Règles :

- Modifie le document uniquement via son API publique (`addEntity`, `removeEntity`, `findEntity`, `notifyEntityChanged`, etc.).
- Capture l'état nécessaire pour `undo()` si besoin (ex. ancienne largeur, ancien calque, snapshot d'entité).
- Peut être appelé plusieurs fois (redo après undo).

Exemple :

```cpp
void execute(core::Document& doc) override {
    if (auto* e = doc.findEntity(entityId_)) {
        oldWidth_ = e->lineWidth();   // capture pour undo
        e->setLineWidth(newWidth_);        // applique la nouvelle largeur
        doc.notifyEntityChanged(e);        // notification (EventBus, UI, etc.)
    }
}
```

### 4.3. `undo(Document&)`

Annule l'effet de `execute()`.

Règles :

- Utilise l'état capturé dans `execute()` (ou dans le constructeur si pertinent) pour restaurer l'état précédent.
- Ne doit pas dépendre de l'ordre d'appel autre que "execute → undo → redo → undo…".

Exemple :

```cpp
void undo(core::Document& doc) override {
    if (auto* e = doc.findEntity(entityId_)) {
        e->setLineWidth(oldWidth_);
        doc.notifyEntityChanged(e);
    }
}
```

### 4.4. `mergeWith(const Command&)`

Optionnel, mais recommandé pour certaines commandes (ex. transformations successives, changements de propriété sur la même entité).

Signature :

```cpp
bool mergeWith(const Command& other) override;
```

Comportement typique :

- Retourne `true` si la commande courante peut "absorber" l'autre commande (ex. deux transformations sur la même entité).
- Met à jour son état interne pour représenter la fusion.
- Retourne `false` si aucune fusion n'est possible.

Exemple (fusion de deux changements de largeur sur la même entité) :

```cpp
bool mergeWith(const Command& other) override {
    auto* o = dynamic_cast<const ChangeLineWidthCommand*>(&other);
    if (!o) return false;
    if (o->entityId_ != entityId_) return false;

    // On garde la dernière largeur demandée
    newWidth_ = o->newWidth_;
    return true;
}
```

### 4.5. `clone() const`

Crée une copie indépendante de la commande, utilisée par `Transaction` et `CommandStack`.

Exemple :

```cpp
std::unique_ptr<Command> clone() const override {
    return std::make_unique<ChangeLineWidthCommand>(entityId_, oldWidth_, newWidth_, text_);
}
```

---

## 5. Exemple complet : `ChangeLineWidthCommand`

Voici un exemple complet de commande qui change la largeur de trait d'une entité.

### 5.1. Déclaration

```cpp
// include/bcad/commands/ChangeLineWidthCommand.h
#pragma once

#include "bcad/commands/Command.h"
#include "bcad/core/Document.h"
#include <memory>
#include <string>

namespace bcad::commands {

class ChangeLineWidthCommand : public Command {
public:
    // Constructeur métier
    ChangeLineWidthCommand(int entityId, double oldWidth, double newWidth, std::string text = "Change Line Width");

    // Constructeur de clone
    ChangeLineWidthCommand(int entityId, double oldWidth, double newWidth, std::string text);

    std::string_view text() const override;
    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;
    bool mergeWith(const Command& other) override;
    std::unique_ptr<Command> clone() const override;

private:
    int entityId_;
    double oldWidth_;
    double newWidth_;
    std::string text_;
};

} // namespace bcad::commands
```

### 5.2. Implémentation

```cpp
// src/commands/ChangeLineWidthCommand.cpp
#include "bcad/commands/ChangeLineWidthCommand.h"
#include "bcad/geometry/Entity.h"

namespace bcad::commands {

ChangeLineWidthCommand::ChangeLineWidthCommand(int entityId, double oldWidth, double newWidth, std::string text)
    : entityId_(entityId), oldWidth_(oldWidth), newWidth_(newWidth), text_(std::move(text)) {}

ChangeLineWidthCommand::ChangeLineWidthCommand(int entityId, double oldWidth, double newWidth, std::string text)
    : entityId_(entityId), oldWidth_(oldWidth), newWidth_(newWidth), text_(std::move(text)) {}

std::string_view ChangeLineWidthCommand::text() const {
    return text_;
}

void ChangeLineWidthCommand::execute(core::Document& doc) {
    if (auto* e = doc.findEntity(entityId_)) {
        oldWidth_ = e->lineWidth();   // capture pour undo
        e->setLineWidth(newWidth_);
        doc.notifyEntityChanged(e);
    }
}

void ChangeLineWidthCommand::undo(core::Document& doc) {
    if (auto* e = doc.findEntity(entityId_)) {
        e->setLineWidth(oldWidth_);
        doc.notifyEntityChanged(e);
    }
}

bool ChangeLineWidthCommand::mergeWith(const Command& other) {
    auto* o = dynamic_cast<const ChangeLineWidthCommand*>(&other);
    if (!o) return false;
    if (o->entityId_ != entityId_) return false;

    newWidth_ = o->newWidth_;
    return true;
}

std::unique_ptr<Command> ChangeLineWidthCommand::clone() const {
    return std::make_unique<ChangeLineWidthCommand>(entityId_, oldWidth_, newWidth_, text_);
}

} // namespace bcad::commands
```

---

## 6. Utilisation depuis le code applicatif

Exemple d'utilisation dans l'application (couche `app/`, avec Qt ou autre UI) :

```cpp
// Dans un slot Qt, un gestionnaire d'action, etc.
void on_changeLineWidth_triggered() {
    int entityId = currentEntityId();  // obtenu via sélection, etc.
    auto* e = document->findEntity(entityId);
    if (!e) return;

    double oldWidth = e->lineWidth();
    double newWidth = 2.0; // par exemple

    auto cmd = std::make_unique<commands::ChangeLineWidthCommand>(entityId, oldWidth, newWidth);
    commandStack->push(std::move(cmd));
}
```

Points clés :

- L'UI ne manipule jamais directement le `Document` pour ce genre d'opération.
- Elle crée une commande et la pousse dans le `CommandStack`.
- Undo/redo sont gérés centralement par `CommandStack`.

---

## 7. Pièges à éviter

1. **Capturer un `Entity*` dans la commande**  
   - Interdit dans l'interface publique.
   - Utilise toujours `int entityId` et `doc.findEntity(entityId)` dans `execute()` / `undo()`.

2. **Modifier le document en dehors de `execute()` / `undo()`**  
   - Toute modification du `Document` doit passer par ces deux méthodes.
   - Pas de modification directe depuis le constructeur ou d'autres méthodes.

3. **Oublier de mettre à jour `entityId_` après un `addEntity()`**  
   - Pour les commandes qui ajoutent une entité (`AddEntityCommand`), s'assurer que `entityId_` est bien initialisé avec l'ID retourné par `doc.addEntity()`.

4. **Oublier de notifier le document**  
   - Si ton architecture utilise `doc.notifyEntityChanged(e)` (pour EventBus, UI, etc.), appelle-le systématiquement après une modification.

5. **Implémenter `mergeWith()` de façon incohérente**  
   - Ne fusionne que des commandes compatibles (même entité, même type d'opération).
   - Assure-toi que l'état fusionné représente correctement la séquence d'opérations.

6. **Rendre le constructeur de clone privé**  
   - `std::make_unique` doit pouvoir appeler le constructeur utilisé dans `clone()`.
   - Garde-le public (ou protected avec un ami, mais public est plus simple).

---

## 8. Références

- `include/bcad/commands/Command.h` – interface `Command`, `Transaction`, `CommandStack`.
- `include/bcad/commands/ConcreteCommands.h` – exemples de commandes existantes (`AddEntityCommand`, `SetLayerCommand`, etc.).
- ADR-009 – Commandes et transactions pures C++.
- ADR-010 – EventBus (pour l'intériorisation future des notifications).