# Modèle d'entité BCAD

> [!IMPORTANT]
>
> ## Statut : MIXTE — état actuel + architecture cible
>
> La section **1** décrit l'état **réel** (enum `EntityType`, `switch(e.type())`). Les sections **2 à 5**
> décrivent l'**architecture cible** (TypeId, EntityRegistry, capabilities, plugins) **non
> implémentée**. Ne pas confondre les deux.

> Architecture du modèle d'entité extensible. Remplace l'enum `EntityType` figé par un système de registre dynamique.

## 1. État actuel (problème)

```cpp
// include/bcad/geometry/Entity.h
enum class EntityType { Point, Line, Circle, Arc, Polyline };
```

Ce type est utilisé dans des `switch(e.type())` partout : `DxfWriter.cpp`, `Database.cpp`, `PropertiesPanel.cpp`, `SnapEngine.cpp`, `SnapGeometry.cpp` (`DxfReader` fait l'appariement par comparaison de chaînes de group codes, pas par enum). Ajouter une entité nécessite de modifier tous ces fichiers.

## 2. Modèle cible

### 2.1 Entity abstraite

> Cible : l'entité déménage vers `bcad::document` (module `bcad_document` cible).
> Aujourd'hui elle vit dans `bcad::geom::Entity` (`include/bcad/geometry/Entity.h`).
> Le namespace cible prolonge le schéma actuel `bcad::geom` / `bcad::core` par module.

```cpp
namespace bcad::document {

class Entity {
public:
    virtual ~Entity() = default;

    // Identité
    virtual EntityId id() const = 0;
    virtual TypeId typeId() const = 0;          // string + GUID

    // Géométrie
    virtual geom::BoundingBox3 boundingBox() const = 0;
    virtual void applyTransform(const geom::Transform3& t) = 0;
    virtual geom::Point3 centroid() const = 0;

    // Tessellation
    virtual render::TessellationInput tessellate(double maxDeviation) const = 0;

    // Sérialisation (via SerializerRegistry)
    virtual std::string serialize() const = 0;
    virtual void deserialize(const std::string& data) = 0;

    // Clonage
    virtual std::unique_ptr<Entity> clone() const = 0;

    // Properties
    virtual const PropertyMap& properties() const = 0;
    virtual PropertyMap& properties() = 0;

    // Capabilities
    virtual bool hasCapability(const std::string& cap) const = 0;
    virtual void* capability(const std::string& cap) = 0;
};

}
```

### 2.2 TypeId

```cpp
namespace bcad::document {

struct TypeId {
    std::string name;       // "wall", "door", "pipe"
    std::string namespace_;  // "architecture", "mechanical", "core"
    Guid guid;

    bool operator==(const TypeId& other) const;
    std::string toString() const;  // "architecture:wall"
};

}
```

### 2.3 EntityRegistry

```cpp
namespace bcad::registry {

class EntityRegistry {
public:
    template<typename T, typename... Args>
    void registerType(Args&&... args);

    std::unique_ptr<document::Entity> create(const document::TypeId& typeId) const;
    std::vector<document::TypeId> listTypes() const;
    std::optional<document::TypeId> findByName(const std::string& name) const;
};

}
```

**Implémentation côté plugin :**

```cpp
extern "C" void bcad_plugin_init(PluginRegistry& reg) {
    reg.entityRegistry().registerType<WallEntity>();
    reg.entityRegistry().registerType<DoorEntity>();
}
```

### 2.4 PropertyMap

Voir `PROPERTY_SYSTEM.md` pour le détail complet. Le `PropertyMap` permet à chaque entité d'exposer des propriétés dynamiques typées (double, int, string, bool, color, enum).

### 2.5 Capabilities

```cpp
// Capability = interface optionnelle vérifiée au runtime
class ITextEntity {
public:
    virtual std::string text() const = 0;
    virtual void setText(const std::string& t) = 0;
};
```

**Vérification :**

```cpp
Entity* e = ...;
if (e->hasCapability("text")) {
    auto* text = static_cast<ITextEntity*>(e->capability("text"));
    text->setText("Hello");
}
```

### 2.6 Hiérarchie

```cpp
namespace bcad::document {

class Entity { ... };  // base

// Entités 2D core
class LineEntity2D : public Entity { ... };
class CircleEntity2D : public Entity { ... };
class PolylineEntity2D : public Entity { ... };

// Entités 3D core
class LineEntity3D : public Entity { ... };
class SurfaceEntity3D : public Entity { ... };
class SolidEntity3D : public Entity { ... };

// Entités métier (plugins)
class WallEntity : public Entity { ... };      // architecture
class DoorEntity : public Entity { ... };      // architecture
class PipeEntity : public Entity { ... };      // mechanical

}
```

## 3. Avantages

| Aspect | Aujourd'hui | Cible |
|--------|------------|-------|
| Ajouter une entité | Modifier tous les `switch` | Déclarer dans un plugin |
| Type ID | enum int | string + GUID |
| Persistance | switch(e.type()) | SerializerRegistry |
| Properties | membres C++ fixes | PropertyMap dynamique |
| Capabilities | héritage rigide | interface runtime |

## 4. Migration depuis l'enum

Mapping de compatibilité préservé via `legacyEntityType()`.

## 5. FAQ

**Q : Comment un plugin enregistre une nouvelle entité ?**
R : `reg.entityRegistry().registerType<WallEntity>()` dans `bcad_plugin_init`.

**Q : Comment Document crée une entité par nom ?**
R : `Document::createEntity(TypeId{"architecture", "wall"})`.

**Q : Core vs plugin entité, différence ?**
R : Aucune dans le modèle. La différence est l'origine (Core ou plugin).