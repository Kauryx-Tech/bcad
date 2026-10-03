# Modèle de Document BCAD

> [!IMPORTANT]
>
> ## Statut : IMPLEMENTE — Document sur ISpatialIndex + EventBus
>
> `bcad::core::Document` (`include/bcad/core/Document.h`) est en place : entités
> 2D, calques, `index::ISpatialIndex` (ADR-008) et EventBus typé. Les extensions
> décrites ici (namespace `bcad::document`, `SelectionSet`, transactions)
> restent des pistes futures.

> Le Document possède le modèle de données mais ne dépend pas de Qt, OpenGL, ou d'un format de fichier.

## 1. Responsabilités

- Collection d'entités (ownership)
- Cycle de vie des entités (création, modification, suppression)
- Calques (LayerManager)
- Index spatial (ISpatialIndex)
- Sélection (SelectionSet)
- État "dirty" (modifié non sauvegardé)
- Émission d'événements (EventBus)
- Thread-safety (shared_mutex)

## 2. Architecture cible

> Cible : `bcad::document::Document` (module `bcad_document`). Aujourd'hui la classe réelle est
> `bcad::core::Document` (`include/bcad/core/Document.h`) — l'architecture cible **déplace** la
> classe du namespace `core` vers le module `document`. C'est la seule différence de strate.

```cpp
namespace bcad::document {

class Document {
public:
    // Construction
    Document();
    ~Document();

    // Entity management
    Entity* addEntity(std::unique_ptr<Entity> entity);
    void removeEntity(EntityId id);
    Entity* findEntity(EntityId id) const;
    const std::vector<std::unique_ptr<Entity>>& entities() const;
    void notifyEntityChanged(Entity* entity);

    // Layer
    layers::LayerManager& layerManager();

    // Selection
    selection::SelectionSet& selection();

    // Spatial index
    // Pas de setSpatialIndex : l'index est cree par le constructeur via
    // index::createDefaultSpatialIndex, et seule la lecture est exposee.
    const index::ISpatialIndex& spatialIndex() const;
    std::vector<Entity*> entitiesInRegion(const geom::BoundingBox3& region) const;
    Entity* pickEntity(const geom::Point3& p, double tolerance) const;

    // State
    bool isDirty() const;
    void markClean();
    void markDirty();

    // Transactions
    transaction::Transaction* beginTransaction(const std::string& name);
    void commitTransaction();
    void rollbackTransaction();

    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // Events
    events::EventBus& eventBus();

    // Persistence (via IDocumentSerializer)
    void save(const std::string& filePath) const;
    void load(const std::string& filePath);

    geom::BoundingBox3 extents() const;
};

}
```

## 3. Ownership

Le Document possède les entités :

```cpp
std::vector<std::unique_ptr<Entity>> entities_;
std::unordered_map<EntityId, Entity*> byId_;  // Pointeur non-propriétaire
```

**Règle :** un `Entity*` retourné par le Document est valide tant que l'entité n'est pas supprimée ou que le Document n'est pas détruit.

## 4. Thread-safety

Modèle : **read-write lock**.

```cpp
mutable std::shared_mutex mutex_;

// Lectures : shared_lock (multiples lecteurs)
Entity* findEntity(EntityId id) const {
    std::shared_lock lock(mutex_);
    // ...
}

// Écritures : unique_lock (exclusif)
Entity* addEntity(std::unique_ptr<Entity> entity) {
    std::unique_lock lock(mutex_);
    // ...
}
```

## 5. Cycle de vie d'une entité

```
Création (plugin/Core)
    → Document::addEntity(unique_ptr<Entity>) → Entity*
    → Modification via Command (transaction)
    → Document::notifyEntityChanged(entity) → met à jour l'index spatial
    → Suppression Document::removeEntity(id)
    → unique_ptr<Entity> détruit → ~Entity()
```

## 6. Sélection

La sélection est **distincte** des entités :

```cpp
namespace bcad::selection {

class SelectionSet {
public:
    void add(EntityId id);
    void remove(EntityId id);
    void clear();
    bool contains(EntityId id) const;
    std::vector<EntityId> entities() const;
    events::EventBus& onChange();
};

}
```

**Justification :** la sélection n'est pas un état de l'entité mais du Document.

## 7. Index spatial

```cpp
namespace bcad::index {

class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;

    // Insertion
    virtual void insert(const document::Entity* entity) = 0;

    // Suppression
    virtual void remove(document::EntityId id) = 0;

    // Mise à jour (suite à une transformation)
    virtual void update(const document::Entity* entity) = 0;

    // Requête par région
    virtual std::vector<document::Entity*> query(const geom::BoundingBox3& region) const = 0;

    // Picking
    virtual document::Entity* pick(const geom::Point3& p, double tolerance) const = 0;

    // Reset
    virtual void clear() = 0;

    // Stats (debug)
    virtual size_t size() const = 0;
    virtual std::string backendName() const = 0;
};

}
```

Voir `SPATIAL_INDEX.md`.

## 8. Événements

Le Document expose un EventBus (voir `EVENT_SYSTEM.md`) :

```cpp
events::EventBus& bus = document.eventBus();
bus.subscribe<EntityAddedEvent>([](const EntityAddedEvent& e) {
    // réaction
});
```

**Événements émis :**
- `EntityAddedEvent`, `EntityRemovedEvent`, `EntityModifiedEvent`
- `LayerAddedEvent`, `LayerRemovedEvent`, `LayerModifiedEvent`
- `SelectionChangedEvent`
- `DocumentSavedEvent`, `DocumentLoadedEvent`
- `TransactionCommittedEvent`, `TransactionRolledBackEvent`

## 9. Transactions

Voir `COMMAND_SYSTEM.md`. Le Document supporte les transactions explicites :

```cpp
auto* tx = document.beginTransaction("CreateWall");
tx->execute([&]() {
    document.addEntity(createWall());
});
document.commitTransaction();  // atomique, undo/redo possible
```

## 10. Persistance

Le Document n'a aucune connaissance directe du format de fichier. Voir `PERSISTENCE_ARCHITECTURE.md`.

## 11. IDs

```cpp
namespace bcad::document {

using EntityId = uint64_t;
constexpr EntityId kInvalidEntityId = 0;

} // Note : aujourd'hui l'ID réel est un simple `int` (Entity::id(), SQLite en int dans src/io/Database.cpp)
```

**Unicité, garantie par `Document::addEntity`.** Une entité arrivant avec un id
libre le garde (une annulation remet ainsi l'entité sous son id, que les
commandes suivantes de la pile visent) et fait avancer le compteur au-delà ; une
entité sans id (`-1`) ou avec un id déjà pris — le clone d'une entité présente,
par exemple Copier ou Symétrie — en reçoit un neuf. Deux entités du document ne
partagent jamais un id : sans cette règle, la commande suivante (tourner,
supprimer, propriété) visait la mauvaise. Couvert par `viewport_tools_test`.

## 12. Préparation 2D/3D

Le Document supporte 2D et 3D simultanément :
- `BoundingBox3` (au lieu de `BoundingBox2`)
- `Point3` (au lieu de `Point2`)
- `Transform3` (au lieu de `Transform2`)

Les entités 2D ont simplement `z = 0`.

## 13. Découplage avec Qt

**Aucune classe du Document n'inclut de header Qt.** Voir `COMMAND_SYSTEM.md` pour l'adaptateur QUndoCommand.

## 14. Découplage avec le renderer

**Aucun header de rendering/ dans Document.h.** L'index spatial est une interface `ISpatialIndex`.

## 15. Règles

1. Le Document ne dépend que de std + types du Core
2. Le Document n'inclut pas Qt, OpenGL, CGAL, SQLite
3. Le Document possède toutes les entités
4. Le Document est thread-safe (read-write lock)
5. Le Document expose un EventBus
6. Le Document supporte les transactions
7. Le Document supporte undo/redo
8. Le Document peut être sérialisé via IDocumentSerializer