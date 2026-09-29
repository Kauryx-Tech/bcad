> Comment étendre BCAD : créer une entité, une commande, un plugin, publier des
> règles de vérification et une interface déclarée.
>
> Le système de plugins **existe** (ADR-005, ABI v9) : voir
> `PLUGIN_ARCHITECTURE.md`, un exemple complet qui compile
> (`src/plugins/cadastre/`, `examples/cadastre_proof/`) et les règles de contrat
> §13 de `PLUGIN_ARCHITECTURE.md`. Les codes ci-dessous sont des motifs
> d'écriture ; l'API exacte fait foi dans les headers `include/bcad/` et les
> fiches `ENTITY_MODEL.md` / `COMMAND_SYSTEM.md` / `WORKBENCH.md`.

## Prérequis

- BCAD installé ou compilé (voir `BUILD_AND_PACKAGING.md`)
- `find_package(BCAD)` dans votre CMake
- Include path vers `include/bcad/`

## 1. Créer une entité

### 1.1 Définir le type

```cpp
// my_wall.h
#include <bcad/geometry/Entity.h>
#include <bcad/geometry/Point.h>

namespace my {

// Entite plugin : herite de la classe SDK bcad::geom::Entity (type partage
// avec l'hote). Ne PAS dupliquer de classes SDK (contrat PLUGIN_ARCHITECTURE
// §13) ; definir uniquement ses types propres dans SON namespace.
//
// `Entity` est une interface pure : heriter d'elle directement oblige a
// implementer toutes ses virtuelles pures (typeId, boundingBox,
// applyTransform, clone, tessellate, distanceTo, serializeParams, writeDxf,
// geometryInfo, properties x2, doAddSnapCandidates) et a porter un
// PropertyMap. Quand la geometrie s'y prete, heriter d'une entite concrete du
// SDK (`PolylineEntity`, `PointEntity`, `TextEntity`, ...) ne coute qu'un
// typeId()/clone()/serializeParams() : c'est ce que fait le module cadastral.
class WallEntity : public bcad::geom::Entity {
public:
    WallEntity(bcad::geom::Point2 start, bcad::geom::Point2 end, double thickness = 0.2);
    
    bcad::geom::TypeId typeId() const override;
    bcad::geom::BoundingBox boundingBox() const override;
    void applyTransform(const bcad::geom::Transform2D& t) override;
    std::unique_ptr<bcad::geom::Entity> clone() const override;
    // + tessellate, distanceTo, serializeParams, writeDxf, geometryInfo,
    //   properties(), doAddSnapCandidates() : signatures dans
    //   include/bcad/geometry/Entity.h. `type()` (enum `EntityType`,
    //   déprécié) est pure elle aussi : elle doit être surchargée même si
    //   rien ne la lit plus (voir TYPEID_STABILITY.md).
    
    bcad::geom::Point2 start() const { return start_; }
    bcad::geom::Point2 end() const { return end_; }
    double thickness() const { return thickness_; }
    
private:
    bcad::geom::Point2 start_;
    bcad::geom::Point2 end_;
    double thickness_;
};

} // namespace my
```

### 1.2 Implémenter

```cpp
// my_wall.cpp
#include "my_wall.h"

namespace my {

WallEntity::WallEntity(bcad::geom::Point2 start, bcad::geom::Point2 end, double thickness)
    : start_(start), end_(end), thickness_(thickness) {}

bcad::geom::TypeId WallEntity::typeId() const {
    return bcad::geom::TypeId{"my.wall"};
}

bcad::geom::BoundingBox WallEntity::boundingBox() const {
    return { std::min(start_.x_, end_.x_), 
             std::min(start_.y_, end_.y_),
             std::max(start_.x_, end_.x_), 
             std::max(start_.y_, end_.y_) };
}

void WallEntity::applyTransform(const bcad::geom::Transform2D& t) {
    start_ = t.transform(start_);
    end_ = t.transform(end_);
}

std::unique_ptr<bcad::geom::Entity> WallEntity::clone() const {
    return std::make_unique<WallEntity>(start_, end_, thickness_);
}

} // namespace my
```

### 1.3 Enregistrer le type

```cpp
// my_plugin.cpp
extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    return reg.registerEntityType(bcad::geom::TypeId{"arch.wall"}, /* factory */);
}
```

## 2. Créer une commande

### 2.1 Définir

```cpp
// cmd_create_wall.h
#include <bcad/commands/Command.h>
#include <bcad/geometry/Point2.h>

namespace my {

class CreateWallCommand : public bcad::commands::Command {
public:
    CreateWallCommand(bcad::geom::Point2 start, bcad::geom::Point2 end);
    
    std::string_view text() const override { return "create_wall"; }
    void execute(bcad::core::Document& doc) override;
    void undo(bcad::core::Document& doc) override;
    std::unique_ptr<Command> clone() const override;
    
private:
    bcad::geom::Point2 start_;
    bcad::geom::Point2 end_;
    int wallId_ = -1;
};

} // namespace my
```

### 2.2 Implémenter

```cpp
void CreateWallCommand::execute(bcad::core::Document& doc) {
    auto wall = std::make_unique<WallEntity>(start_, end_);
    // addEntity rend l'entite stockee, pas son identifiant : le lit-on sur
    // l'objet (Document.h:23).
    wallId_ = doc.addEntity(std::move(wall))->id();
}

void CreateWallCommand::undo(bcad::core::Document& doc) {
    if (wallId_ >= 0) {
        doc.removeEntity(wallId_);
    }
}
```

### 2.3 Enregistrer

```cpp
reg.registerCommand("architecture.create_wall", &makeCreateWallCommand);
```

## 3. Observer les changements du Document

Le `bcad::core::Document` publie des événements typés sur le bus global
`bcad::events::EventBus::instance()` (`EntityAdded`, `EntityRemoved`,
`EntityModified`, `DocumentCleared`). Un composant applicatif s'abonne côté
hôte :

```cpp
#include <bcad/events/EventBus.h>

// Hook applicatif (pas dans bcad_plugin_init : le plugin n'a pas le Document).
bcad::events::EventBus::instance().subscribe<bcad::events::EntityAdded>(
    [](const bcad::events::EntityAdded& evt) {
        // evt.document, evt.entity (pointeur, jamais nullptr) ; lire
        // evt.entity->typeId() plutôt que l'enum EntityType, dépréciée.
    });
```

`subscribe` rend un `SubscriptionId` : il faut le conserver, ou utiliser
`bcad::events::SubscriptionGuard` (RAII), pour se désabonner avant la fin de
l'hôte. Un abonnement porté par du code de module et non retiré avant le
`dlclose` est appelé après la décharge du module.

> Le PluginRegistry n'expose **pas** de `eventBus()` : un module ne publie ni ne
> s'abonne d'autorité. La médiation par le registre se limite à six points
> d'extension (ADR-005) : `registerEntityType`, `registerCommand`,
> `registerSerializer`, `registerWorkbench` (UI déclarée, voir `WORKBENCH.md`),
> `registerValidator` (règles de vérification) et `registerFileExporter` (formats
> d'échange, voir `PLUGIN_ARCHITECTURE.md` §4). Les objets construits par un
> module et stockés dans un registre global sont détruits par l'hôte **avant** le
> `dlclose`.

## 4. Propriétés dynamiques

`bcad::geom::Entity` expose une `bcad::properties::PropertyMap` (types
`PropertyType`/`PropertyValue`, copyable). Une entité plugin alimente sa map :

```cpp
// dans le constructeur de WallEntity
auto* thickness = properties().addDouble("thickness", 0.3);
thickness->setUnit("m");
thickness->setDescription("Epaisseur du mur");

// lecture côté hôte
double t = wall->properties().getDouble("thickness");
```

> L'API Value-typed générique de `bcad::properties::PropertyKey` n'existe pas
> encore : on utilise les méthodes `addDouble`/`getDouble`/... de
> `PropertyMap` (`include/bcad/properties/PropertyMap.h`).

## 5. Plugin complet

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyWallPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(BCAD CONFIG REQUIRED)

add_library(my_wall_plugin MODULE
    src/my_plugin.cpp src/my_wall.cpp
    src/cmd_create_wall.cpp)
# Contrat plugin (PLUGIN_ARCHITECTURE.md §13) : NE PAS lier les bibliotheques
# de types (bcad_geometry, bcad_core, ...) — vtables/typeinfo = celles de l'hôte.
target_link_libraries(my_wall_plugin PRIVATE BCAD::bcad_plugin)

install(TARGETS my_wall_plugin
    LIBRARY DESTINATION lib/bcad/plugins)
```

### Point d'entrée

```cpp
extern "C" int bcad_plugin_api_version() {
    return bcad::plugin::PLUGIN_API_VERSION;   // gate ABI strict
}

extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    reg.info().name = "my_wall";
    reg.info().version = "1.0.0";

    // Rendre false si un enregistrement échoue : l'hôte fait alors le ménage
    // des objets déjà déclarés avant de décharger le module.
    // Pointeurs de fonction bruts (contrat ABI, PluginRegistry.h:41-47) :
    //   makeWall        : unique_ptr<Entity>(std::string_view params)
    //   makeCreateWall  : unique_ptr<Command>(const std::vector<std::string>&)
    bool ok = reg.registerEntityType(bcad::geom::TypeId{"my.wall"}, &makeWall);
    ok = reg.registerCommand("architecture.create_wall", &makeCreateWall) && ok;
    ok = reg.registerSerializer(std::make_unique<my::WallSerializer>()) && ok;

    // Optionnels : un onglet/panneau déclaré (WORKBENCH.md), des règles de
    // vérification et un format d'export (PLUGIN_ARCHITECTURE.md §4). Les objets
    // passent par unique_ptr : l'hôte les détruit, jamais le module.
    ok = reg.registerWorkbench(std::make_unique<my::WallWorkbench>()) && ok;
    ok = reg.registerValidator(std::make_unique<my::WallValidator>()) && ok;
    ok = reg.registerFileExporter(std::make_unique<my::WallFormat>()) && ok;
    return ok;
}

extern "C" void bcad_plugin_shutdown() {}
```

### Valeurs réglables (gabarits du module)

Un métier déplace des constantes hors du code : un motif d'identifiant, un
profil par pays, une tolérance. Le module ne les écrit pas en dur et ne devine
pas où elles sont posées ; l'hôte annonce ses répertoires de données et le
registre les transmet (`resolveDataFile`, ABI v7) :

```cpp
extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    // Chemin relatif PRE-FIXE par le nom du module : deux modules peuvent avoir
    // un fichier du même nom, ils ne doivent pas se lire l'un l'autre.
    const std::string path = reg.resolveDataFile("my_wall/profile.json");
    MyProfile profile = loadProfile(path);   // valeurs par défaut si path vide
    bool ok = reg.registerValidator(std::make_unique<my::WallValidator>(profile.tolerance));
    // ... le reste des enregistrements du module
    return ok;
}
```

Le module pose ses fichiers à côté de son binaire, dans `share/bcad/plugins/`
(arbre de build comme installation, même forme relative) :

```cmake
add_custom_command(TARGET my_wall_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/profile
        ${CMAKE_BINARY_DIR}/share/bcad/plugins/my_wall/profile)
install(DIRECTORY profile/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/bcad/plugins/my_wall/profile)
```

Un profil absent ou illisible n'est **pas** une erreur : le module garde ses
valeurs par défaut. C'est ce qui permet à un module chargé par `dlopen` de
fonctionner chez un utilisateur qui n'a rien configuré — et à
`$BCAD_PLUGIN_DATA` de suffire à un bureau qui veut imposer son profil.

## 6. Tests

Les tests du dépôt sont dans `tests/` (style assertions simples,
`smoke_test.cpp`, `tests/unit/`). Le pattern d'entité d'un plugin se teste en
général côté hôte, sans chargeur de plugin :

```cpp
#include <bcad/core/Document.h>
#include <bcad/registry/EntityRegistry.h>

int testWallBounds() {
    // enregistre la factory comme le ferait un hote applicatif
    bcad::registry::EntityRegistry::registerType(
        bcad::geom::TypeId{"my.wall"}, "my.wall", &makeWall);
    auto wall = bcad::registry::EntityRegistry::create(bcad::geom::TypeId{"my.wall"});
    if (!wall) return 1;
    // ... assertions sur boundingBox()
    return 0;
}
```

## Voir aussi

- `PLUGIN_ARCHITECTURE.md` — architecture détaillée
- `SDK_ARCHITECTURE.md` — surface SDK
- `WORKBENCH.md` — déclarer l'UI (onglets, panneaux, actions)
- `ENTITY_MODEL.md` — modèle d'entité
- `COMMAND_SYSTEM.md` — commandes
- `EVENT_SYSTEM.md` — événements
- `src/plugins/cadastre/` et `examples/cadastre_proof/` — deux modules réels
  compilés par le dépôt ; `CADASTRE_PLUGIN_STATUS.md` dit ce qu'ils font vraiment