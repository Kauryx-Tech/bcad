#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/TypeId.h"
#include <optional>
#include <string>
#include <vector>

namespace bcad::serialization {

// Interface pour la sérialisation/désérialisation d'un type d'entité.
// Permet aux plugins d'enregistrer leur propre format sans modifier le Core.
class IEntitySerializer {
public:
    virtual ~IEntitySerializer() = default;

    // Identifiant du type géré par ce sérialiseur.
    virtual geom::TypeId typeId() const = 0;

    // Nom lisible (pour debug/log).
    virtual std::string_view formatName() const = 0;

    // Sérialise une entité vers une chaîne (format interne, ex: CSV pour SQLite).
    virtual std::string serialize(const geom::Entity& entity) const = 0;

    // Désérialise une entité depuis une chaîne.
    // Retourne nullptr si le format est invalide.
    virtual std::unique_ptr<geom::Entity> deserialize(const std::string& data) const = 0;

    // Écrit l'entité vers un flux de sortie (format externe, ex: DXF).
    virtual void writeToStream(std::ostream& out, const geom::Entity& entity) const = 0;

    // Lit une entité depuis un flux d'entrée (format externe).
    // Retourne nullptr en cas d'échec.
    virtual std::unique_ptr<geom::Entity> readFromStream(std::istream& in) const = 0;
};

// Registre central des sérialiseurs d'entités.
class SerializerRegistry {
public:
    // Enregistre un sérialiseur pour un type d'entité.
    // Doit être appelé une seule fois par TypeId.
    static void registerSerializer(std::unique_ptr<IEntitySerializer> serializer);

    // Récupère le sérialiseur pour un TypeId donné.
    static const IEntitySerializer* find(geom::TypeId typeId);

    // Vérifie si un type a un sérialiseur enregistré.
    static bool contains(geom::TypeId typeId);

    // Retire un sérialiseur s'il existe (l'instance est détruite aussitôt).
    // Sert a liberer, avant dlclose, des serializers dont le code vit dans le
    // DSO d'un plugin (sinon la destruction tardive executait du plugin code
    // apres depliage -> SEGV).
    static void remove(geom::TypeId typeId);

    // Liste tous les TypeId enregistrés.
    static std::vector<geom::TypeId> registeredTypes();

    // Initialise les sérialiseurs natifs (à appeler au démarrage de l'application).
    static void initializeNativeSerializers();

private:
    static std::unordered_map<std::string, std::unique_ptr<IEntitySerializer>>& map();
    static std::mutex& mutex();
};

} // namespace bcad::serialization