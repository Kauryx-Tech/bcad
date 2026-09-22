# Exemple 4 : Ajouter une entité (architecture cible)

> Comment ajouter une nouvelle entité dans l'architecture **cible** (après la migration).

> ⚠️ **Cet exemple décrit l'architecture cible.** Le code actuel utilise un `enum class EntityType` figé. Voir `docs/ARCHITECTURE_ROADMAP.md` Phase 4.

## Architecture cible

Dans l'architecture cible, les entités sont enregistrées via `EntityRegistry` :

```cpp
// Dans bcad_plugin_init()
extern "C" bool bcad_plugin_init(PluginRegistry& reg) {
    return reg.registerEntityType(bcad::geom::TypeId{"arch.wall"}, /* factory */);
}
```

## Définir une nouvelle entité

```cpp
// my_entity.h
#pragma once
#include <bcad/geometry/Point2.h>
#include <bcad/geometry/BoundingBox.h>

namespace my {

class WallEntity : public bcad::IEntity {
public:
    WallEntity(Point2 start, Point2 end, double thickness = 0.2);
    
    bcad::TypeId typeId() const override;
    BoundingBox boundingBox() const override;
    void applyTransform(const Transform2& t) override;
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

std::unique_ptr<bcad::IEntity> WallEntity::clone() const {
    return std::make_unique<WallEntity>(start_, end_, thickness_);
}

void WallEntity::applyTransform(const Transform2& t) {
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

## Ce qui change par rapport au code actuel

| Aspect | Actuel | Cible |
|--------|--------|-------|
| Identification | `enum EntityType` | `TypeId` (string + GUID) |
| Dispatch | `switch(type())` | `EntityRegistry::create()` |
| Enregistrement | Modifier enum | Plugin appelle `registerType<T>()` |
| Fichiers à modifier | Database.cpp, DxfReader, etc. | Aucun (plugin) |

## État actuel

Le système de registry n'existe pas encore dans le code. Il sera implémenté dans la Phase 4 de la roadmap.

En attendant :
1. Tu peux ajouter une classe `WallEntity` dans `include/bcad/geometry/`
2. Tu dois l'ajouter à l'enum `EntityType`
3. Tu dois l'ajouter au `switch` dans `Database.cpp`, `DxfReader.cpp`, etc.

## Voir aussi

- `docs/ENTITY_MODEL.md` — modèle d'entité complet
- `docs/ARCHITECTURE_ROADMAP.md` — Phase 4
- `docs/EXTENDING_BCAD.md` — guide d'extension