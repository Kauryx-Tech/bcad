#include "bcad/registry/EntityRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
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

bool parseBool(const std::string& s) {
    return s == "1" || s == "true" || s == "TRUE";
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
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() < 1) return nullptr;
    bool closed = parseBool(parts[0]);
    if ((parts.size() - 1) % 2 != 0) return nullptr;
    std::vector<geom::Point2> vertices;
    vertices.reserve((parts.size() - 1) / 2);
    for (std::size_t i = 1; i + 1 < parts.size(); i += 2) {
        double x = parseDouble(parts[i]);
        double y = parseDouble(parts[i + 1]);
        if (std::isnan(x) || std::isnan(y)) return nullptr;
        vertices.emplace_back(x, y);
    }
    return std::make_unique<geom::PolylineEntity>(std::move(vertices), closed);
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
}

// Force l'enregistrement au chargement de la bibliothèque
struct NativeEntityRegistrar {
    NativeEntityRegistrar() { registerNativeEntities(); }
};
static NativeEntityRegistrar g_nativeEntityRegistrar;

} // namespace bcad::registry