# Exemple 4 : Ajouter une entité (motif d'extension)

> Comment ajouter une nouvelle entité et l'enregistrer via `EntityRegistry` /
> `PluginRegistry` (implémenté — ADR-003 : `TypeId` à la place de l'enum
> `EntityType`, déprécié).

## Enregistrer le type

Les entités s'enregistrent via `EntityRegistry` (règles hôte) ou
`PluginRegistry` (plugins, médiatisé par l'hôte) :

```cpp
// Dans bcad_plugin_init()
extern "C" bool bcad_plugin_init(PluginRegistry& reg) {
    return reg.registerEntityType(bcad::geom::TypeId{"arch.wall"}, &makeWall);
}
```

> L'API de référence et le contrat plugin sont décrits dans
> `docs/PLUGIN_ARCHITECTURE.md` §13 et `docs/ENTITY_MODEL.md` ; la preuve
> exécutable est `examples/sdk_proof`.

## Définir une nouvelle entité

```cpp
// my_entity.h
#pragma once
#include <bcad/geometry/Point2.h>
#include <bcad/geometry/BoundingBox.h>

namespace my {

class WallEntity : public bcad::geom::Entity {
public:
    WallEntity(Point2 start, Point2 end, double thickness = 0.2);
    
    bcad::TypeId typeId() const override;
    BoundingBox boundingBox() const override;
    void applyTransform(const bcad::geom::Transform2D& t) override;
    std::unique_ptr<IEntity> clone() const override;
    
    std::vector<Point2> tessellate(double maxDeviation) const override;
    double distanceTo(const Point2& p) const override;
    
    // Accesseurs
    Point2 start() const { return start_; }
    Point2 end() const { return end_; }
    double thickness() const { return thickness_; }
    
private:
    Point2 start_;
    Point2 end_;
    double thickness_;
};

} // namespace my
```

## Implémenter les méthodes requises

```cpp
// my_entity.cpp
#include "my_entity.h"

namespace my {

WallEntity::WallEntity(Point2 start, Point2 end, double thickness)
    : start_(start), end_(end), thickness_(thickness) {}

bcad::TypeId WallEntity::typeId() const {
    static const auto id = bcad::TypeId{
        "architecture",  // namespace
        "wall",         // name
        bcad::Guid::fromString("{...}")  // stable GUID
    };
    return id;
}

BoundingBox WallEntity::boundingBox() const {
    return {
        std::min(start_.x, end_.x),
        std::min(start_.y, end_.y),
        std::max(start_.x, end_.x),
        std::max(start_.y, end_.y)
    };
}

std::vector<Point2> WallEntity::tessellate(double /*maxDeviation*/) const {
    return { start_, end_ };
}

double WallEntity::distanceTo(const Point2& p) const {
    // Calcul de la distance point-segment
    // ...implémentation...
    return 0.0;
}

std::unique_ptr<bcad::geom::Entity> WallEntity::clone() const {
    return std::make_unique<WallEntity>(start_, end_, thickness_);
}

void WallEntity::applyTransform(const bcad::geom::Transform2D& t) {
    start_ = t.apply(start_);
    end_ = t.apply(end_);
}

} // namespace my
```

## Enregistrer le type

```cpp
// plugin.cpp
#include <bcad/plugin/PluginRegistry.h>
#include "my_entity.h"

extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    return reg.registerEntityType(bcad::geom::TypeId{"my.wall"}, /* factory */);
}
```

## Ce qui change par rapport à l'enum historique

| Aspect | Ancien (déprécié) | Actuel |
|--------|-------------------|--------|
| Identification | `enum EntityType` | `TypeId` (string, ADR-003) |
| Dispatch | `switch(type())` | `EntityRegistry::create(typeId())` |
| Enregistrement | Modifier l'enum | `registerEntityType(typeId, factory)` |
| Fichiers à modifier | Database.cpp, DxfReader, etc. | Aucun (règle Registry) |

## Démarrage

Les plugins et le `EntityRegistry` sont **implémentés** : la preuve exécutable
est `examples/sdk_proof` (entité externe + commande + serializer). Le motif
d'extension est détaillé dans `docs/EXTENDING_BCAD.md`.

Pour créer une entité plugin :
1. Hérite de `bcad::geom::Entity` (interface réelle : `typeId()`,
   `boundingBox()`, `applyTransform()`, `clone()`, `tessellate()`,
   `distanceTo()`, `serializeParams()`, ...)
2. Fournis une factory `std::unique_ptr<Entity>()` (fonction libre)
3. Enregistre-la dans `bcad_plugin_init()` sous un `TypeId`