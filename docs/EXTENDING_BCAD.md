> **Note :** Ce document décrit l'architecture cible (après migration). Le système de plugins n'existe pas encore dans le code actuel. Voir `ARCHITECTURE_ROADMAP.md` Phase 11.


> Comment étendre BCAD : créer une entité, une commande, un plugin.

## Prérequis

- BCAD installé ou compilé (voir `BUILD_AND_PACKAGING.md`)
- `find_package(BCAD)` dans votre CMake
- Include path vers `include/bcad/`

## 1. Créer une entité

### 1.1 Définir le type

```cpp
// my_wall.h
#include <bcad/geometry/Point2.h>
#include <bcad/entity/IEntity.h>

namespace my {

class WallEntity : public bcad::IEntity {
public:
    WallEntity(Point2 start, Point2 end, double thickness = 0.2);
    
    TypeId typeId() const override;
    BoundingBox2 boundingBox() const override;
    void transform(const Transform2& t) override;
    std::unique_ptr<IEntity> clone() const override;
    
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

### 1.2 Implémenter

```cpp
// my_wall.cpp
#include "my_wall.h"

namespace my {

WallEntity::WallEntity(Point2 start, Point2 end, double thickness)
    : start_(start), end_(end), thickness_(thickness) {}

TypeId WallEntity::typeId() const {
    static const auto id = TypeId{ "architecture", "wall", ... };
    return id;
}

BoundingBox2 WallEntity::boundingBox() const {
    return { std::min(start_.x, end_.x), 
             std::min(start_.y, end_.y),
             std::max(start_.x, end_.x), 
             std::max(start_.y, end_.y) };
}

void WallEntity::transform(const Transform2& t) {
    start_ = t.apply(start_);
    end_ = t.apply(end_);
}

std::unique_ptr<IEntity> WallEntity::clone() const {
    return std::make_unique<WallEntity>(start_, end_, thickness_);
}

} // namespace my
```

### 1.3 Enregistrer le type

```cpp
// my_plugin.cpp
extern "C" void bcad_plugin_init(bcad::PluginRegistry& reg) {
    reg.registerEntityType<my::WallEntity>(
        "architecture", "wall", bcad::Guid::fromString("..."));
}
```

## 2. Créer une commande

### 2.1 Définir

```cpp
// cmd_create_wall.h
#include <bcad/commands/Command.h>
#include <bcad/geometry/Point2.h>

namespace my {

class CreateWallCommand : public bcad::Command {
public:
    CreateWallCommand(bcad::Document& doc, Point2 start, Point2 end);
    
    void execute() override;
    void undo() override;
    std::string name() const override { return "CreateWall"; }
    
private:
    bcad::Document& doc_;
    bcad::EntityId wallId_;
    Point2 start_;
    Point2 end_;
};

} // namespace my
```

### 2.2 Implémenter

```cpp
void CreateWallCommand::execute() {
    auto wall = std::make_unique<WallEntity>(start_, end_);
    wallId_ = doc_.addEntity(std::move(wall));
}

void CreateWallCommand::undo() {
    doc_.removeEntity(wallId_);
}
```

### 2.3 Enregistrer

```cpp
reg.registerCommand<CreateWallCommand>("architecture.create_wall");
```

## 3. S'abonner aux événements

```cpp
reg.eventBus().subscribe<bcad::events::EntityAddedEvent>(
    [](const bcad::events::EntityAddedEvent& evt) {
        bcad::log::info("Entity added: {}", evt.entityId);
    }
);
```

## 4. Propriétés dynamiques

```cpp
struct WallProperties {
    static constexpr auto thickness = 
        bcad::PropertyKey<double>::make("thickness");
    static constexpr auto material = 
        bcad::PropertyKey<std::string>::make("material");
};

wall.properties().set(WallProperties::thickness, 0.3);
double t = wall.properties().get(WallProperties::thickness);
```

## 5. Plugin complet

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyWallPlugin)

find_package(BCAD REQUIRED)

add_library(my_wall_plugin SHARED)
target_sources(my_wall_plugin PRIVATE
    src/my_plugin.cpp src/my_wall.cpp
    src/cmd_create_wall.cpp)
target_link_libraries(my_wall_plugin PRIVATE BCAD::SDK)

install(TARGETS my_wall_plugin 
    LIBRARY DESTINATION lib/bcad/plugins)
```

### Point d'entrée

```cpp
extern "C" const char* bcad_plugin_version() { return "1.0.0"; }

extern "C" void bcad_plugin_init(bcad::PluginRegistry& reg) {
    reg.registerEntityType<my::WallEntity>(...);
    reg.registerCommand<my::CreateWallCommand>(...);
    reg.registerSerializer(std::make_unique<my::WallSerializer>());
}
```

## 6. Tests

```cpp
#include <bcad/testing/TestDocument.h>

TEST_CASE("Wall bounds") {
    bcad::testing::TestDocument doc;
    auto wall = std::make_unique<my::WallEntity>(
        Point2{0,0}, Point2{10,0});
    REQUIRE(wall->boundingBox().maxX == 10);
}
```

## Voir aussi

- `PLUGIN_ARCHITECTURE.md` — architecture détaillée
- `SDK_ARCHITECTURE.md` — surface SDK
- `ENTITY_MODEL.md` — modèle d'entité
- `COMMAND_SYSTEM.md` — commandes
- `EVENT_SYSTEM.md` — événements