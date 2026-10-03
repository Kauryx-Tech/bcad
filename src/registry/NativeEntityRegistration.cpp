#include "bcad/registry/EntityRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include <memory>

namespace bcad::registry {

// Enregistre toutes les entités natives BCAD.
// Cette fonction est appelée automatiquement au chargement de la bibliothèque.
// La désérialisation depuis le disque passe par SerializerRegistry, pas par ces fabriques.
void registerNativeEntities() {
    EntityRegistry::registerType(geom::TypeId_Point, "Point",
        [] { return std::make_unique<geom::PointEntity>(); });
    EntityRegistry::registerType(geom::TypeId_Line, "Line",
        [] { return std::make_unique<geom::LineEntity>(); });
    EntityRegistry::registerType(geom::TypeId_Circle, "Circle",
        [] { return std::make_unique<geom::CircleEntity>(); });
    EntityRegistry::registerType(geom::TypeId_Arc, "Arc",
        [] { return std::make_unique<geom::ArcEntity>(); });
    EntityRegistry::registerType(geom::TypeId_Polyline, "Polyline",
        [] { return std::make_unique<geom::PolylineEntity>(); });
    EntityRegistry::registerType(geom::TypeId_Text, "Text",
        [] { return std::make_unique<geom::TextEntity>(); });
}

// Force l'enregistrement au chargement de la bibliothèque
struct NativeEntityRegistrar {
    NativeEntityRegistrar() { registerNativeEntities(); }
};
static NativeEntityRegistrar g_nativeEntityRegistrar;

} // namespace bcad::registry
