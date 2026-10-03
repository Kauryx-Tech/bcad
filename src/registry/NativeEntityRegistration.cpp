#include "bcad/registry/EntityRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace bcad::registry {

namespace {

std::vector<std::string> splitParams(std::string_view params) {
    std::vector<std::string> parts;
    std::string part;
    for (char c : params) {
        if (c == ',') {
            parts.push_back(part);
            part.clear();
        } else {
            part += c;
        }
    }
    if (!part.empty()) parts.push_back(part);
    return parts;
}

double parseDouble(const std::string& s) {
    try {
        return std::stod(s);
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

// Fabriques de reconstruction depuis les paramètres sérialisés (CSV) au
// même format que Entity::serializeParams().

std::unique_ptr<geom::Entity> makePointEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 2) return nullptr;
    double x = parseDouble(parts[0]);
    double y = parseDouble(parts[1]);
    if (std::isnan(x) || std::isnan(y)) return nullptr;
    return std::make_unique<geom::PointEntity>(geom::Point2{x, y});
}

std::unique_ptr<geom::Entity> makeLineEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 4) return nullptr;
    double x1 = parseDouble(parts[0]);
    double y1 = parseDouble(parts[1]);
    double x2 = parseDouble(parts[2]);
    double y2 = parseDouble(parts[3]);
    if (std::isnan(x1) || std::isnan(y1) || std::isnan(x2) || std::isnan(y2)) return nullptr;
    return std::make_unique<geom::LineEntity>(geom::Point2{x1, y1}, geom::Point2{x2, y2});
}

std::unique_ptr<geom::Entity> makeCircleEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 3) return nullptr;
    double cx = parseDouble(parts[0]);
    double cy = parseDouble(parts[1]);
    double r = parseDouble(parts[2]);
    if (std::isnan(cx) || std::isnan(cy) || std::isnan(r)) return nullptr;
    return std::make_unique<geom::CircleEntity>(geom::Point2{cx, cy}, r);
}

std::unique_ptr<geom::Entity> makeArcEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 5) return nullptr;
    double cx = parseDouble(parts[0]);
    double cy = parseDouble(parts[1]);
    double r = parseDouble(parts[2]);
    double sa = parseDouble(parts[3]);
    double ea = parseDouble(parts[4]);
    if (std::isnan(cx) || std::isnan(cy) || std::isnan(r) || std::isnan(sa) || std::isnan(ea)) return nullptr;
    return std::make_unique<geom::ArcEntity>(geom::Point2{cx, cy}, r, sa, ea);
}

std::unique_ptr<geom::Entity> makePolylineEntity(std::string_view params) {
    // La grammaire est celle de l'entite : cette fabrique ne doit pas en tenir
    // une seconde, sous peine de desapprouver ce que l'hote ecrit.
    geom::PolylineEntity::Rings rings;
    if (!geom::PolylineEntity::decodeRings(params, rings)) return nullptr;
    auto entity = std::make_unique<geom::PolylineEntity>(std::move(rings.outer), rings.closed);
    for (auto& hole : rings.holes) entity->addHole(std::move(hole));
    return entity;
}

std::unique_ptr<geom::Entity> makeTextEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() < 5) return nullptr;
    double x = parseDouble(parts[0]);
    double y = parseDouble(parts[1]);
    double height = parseDouble(parts[2]);
    double rotation = parseDouble(parts[3]);
    if (std::isnan(x) || std::isnan(y) || std::isnan(height) || std::isnan(rotation)) return nullptr;
    return std::make_unique<geom::TextEntity>(
        geom::Point2{x, y}, parts[4], height, rotation);
}

} // namespace

// Enregistre toutes les entités natives BCAD.
// Cette fonction est appelée automatiquement au chargement de la bibliothèque.
void registerNativeEntities() {
    EntityRegistry::registerType(geom::TypeId_Point, "Point",
        [] { return std::make_unique<geom::PointEntity>(); }, makePointEntity);
    EntityRegistry::registerType(geom::TypeId_Line, "Line",
        [] { return std::make_unique<geom::LineEntity>(); }, makeLineEntity);
    EntityRegistry::registerType(geom::TypeId_Circle, "Circle",
        [] { return std::make_unique<geom::CircleEntity>(); }, makeCircleEntity);
    EntityRegistry::registerType(geom::TypeId_Arc, "Arc",
        [] { return std::make_unique<geom::ArcEntity>(); }, makeArcEntity);
    EntityRegistry::registerType(geom::TypeId_Polyline, "Polyline",
        [] { return std::make_unique<geom::PolylineEntity>(); }, makePolylineEntity);
    EntityRegistry::registerType(geom::TypeId_Text, "Text",
        [] { return std::make_unique<geom::TextEntity>(); }, makeTextEntity);
}

// Force l'enregistrement au chargement de la bibliothèque
struct NativeEntityRegistrar {
    NativeEntityRegistrar() { registerNativeEntities(); }
};
static NativeEntityRegistrar g_nativeEntityRegistrar;

} // namespace bcad::registry