#pragma once

#include "bcad/geometry/Point.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/geometry/Entity.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace bcad::registry {

// Fabrique par défaut (sans argument) pour créer une entité vide du type.
using EntityDefaultFactory = std::function<std::unique_ptr<geom::Entity>()>;

// Fabrique à partir des paramètres sérialisés (même format que serializeParams()).
using EntityParamsFactory = std::function<std::unique_ptr<geom::Entity>(std::string_view params)>;

// Métadonnées d'un type d'entité enregistré.
struct EntityMetadata {
    geom::TypeId typeId;
    std::string displayName;
    EntityDefaultFactory factory;
    // Facultatif : reconstruction depuis les paramètres sérialisés (CSV).
    // Utilisé pour la désérialisation sans passer par un serializer dédié.
    EntityParamsFactory paramsFactory;
};

// Auto-registration helper for plugins/native entities.
// Usage: BCAD_REGISTER_ENTITY(MyEntity, "My Entity", geom::TypeId_MyEntity);
// Place in a .cpp file to auto-register at library load.
#define BCAD_REGISTER_ENTITY(EntityClass, DisplayName, TypeId) \
    namespace { \
    struct EntityClass##_Registrar { \
        EntityClass##_Registrar() { \
            bcad::registry::EntityRegistry::registerType( \
                TypeId, DisplayName, \
                [] { return std::make_unique<EntityClass>(); }); \
        } \
    }; \
    static EntityClass##_Registrar g_##EntityClass##_registrar; \
    }

// Registre central des types d'entités. Permet aux plugins d'enregistrer
// leurs propres types d'entités sans modifier le Core.
class EntityRegistry {
public:
    using FactoryFn = std::function<std::unique_ptr<geom::Entity>()>;

    // Enregistre un type d'entité natif ou plugin.
    // Doit être appelé une seule fois par TypeId (au démarrage).
    static void registerType(geom::TypeId typeId, std::string_view displayName, FactoryFn factory);

    // Enregistre un type avec en plus une fabrique de reconstruction depuis
    // les paramètres sérialisés (utilisé par la désérialisation).
    static void registerType(geom::TypeId typeId, std::string_view displayName,
                             FactoryFn factory, EntityParamsFactory paramsFactory);

    // Enregistre un type uniquement via sa fabrique à paramètres sérialisés.
    static void registerType(geom::TypeId typeId, std::string_view displayName,
                             EntityParamsFactory paramsFactory);

    // Enregistre tous les types d'entités natives BCAD.
    // Utile pour forcer l'enregistrement en liaison statique.
    static void registerNativeTypes();

    // Recherche les métadonnées d'un type par son TypeId.
    static const EntityMetadata* find(geom::TypeId typeId);

    // Recherche les métadonnées d'un type par son nom d'affichage.
    static const EntityMetadata* findByName(std::string_view displayName);

    // Crée une nouvelle instance d'entité pour le TypeId donné.
    // Retourne nullptr si le type n'est pas enregistré.
    static std::unique_ptr<geom::Entity> create(geom::TypeId typeId);

    // Crée une instance depuis les paramètres sérialisés (CSV serializeParams).
    // Retourne nullptr si le type n'est pas enregistré ou si les params sont invalides.
    static std::unique_ptr<geom::Entity> create(geom::TypeId typeId, std::string_view params);

    // Retourne la liste de tous les types enregistrés.
    static std::vector<EntityMetadata> all();

    // Retourne la liste de tous les TypeIds enregistrés.
    static std::vector<geom::TypeId> allTypeIds();

    // Vérifie si un type est enregistré.
    static bool contains(geom::TypeId typeId);

    // Retire un type enregistré par un plugin avant dlclose().
    static bool unregisterType(geom::TypeId typeId);

    // Alias de contains() pour l'API plugin.
    static bool hasType(geom::TypeId typeId);

private:
    static std::unordered_map<std::string, EntityMetadata>& map();
};

} // namespace bcad::registry