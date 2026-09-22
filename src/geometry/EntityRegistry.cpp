#include "bcad/geometry/EntityRegistry.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Point.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace bcad::geom {

namespace detail {

// Split CSV params into parts
static std::vector<std::string> splitParams(std::string_view params) {
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

} // namespace detail

void EntityRegistry::registerType(TypeId typeId, EntityFactory factory) {
    factories_[typeId] = {std::move(factory), nullptr};
}

void EntityRegistry::registerTypeParsed(TypeId typeId, EntityFactoryParsed factory) {
    factories_[typeId] = {nullptr, std::move(factory)};
}

bool EntityRegistry::hasType(TypeId typeId) const {
    return factories_.find(typeId) != factories_.end();
}

std::unique_ptr<Entity> EntityRegistry::create(TypeId typeId, std::string_view params) const {
    auto it = factories_.find(typeId);
    if (it == factories_.end()) return nullptr;
    
    if (it->second.factory) {
        return it->second.factory(params);
    }
    // If only parsed factory exists, parse params first
    if (it->second.factoryParsed) {
        std::vector<std::string> parts = detail::splitParams(params);
        return it->second.factoryParsed(parts);
    }
    return nullptr;
}

std::unique_ptr<Entity> EntityRegistry::createParsed(TypeId typeId, const std::vector<std::string>& parts) const {
    auto it = factories_.find(typeId);
    if (it == factories_.end() || !it->second.factoryParsed) return nullptr;
    return it->second.factoryParsed(parts);
}

std::vector<TypeId> EntityRegistry::getRegisteredTypes() const {
    std::vector<TypeId> types;
    types.reserve(factories_.size());
    for (const auto& [typeId, _] : factories_) {
        types.push_back(typeId);
    }
    return types;
}

EntityRegistry& EntityRegistry::instance() {
    static EntityRegistry registry;
    return registry;
}

// Helper: parse a double from string, returns NaN on failure
static double parseDouble(const std::string& s) {
    try {
        return std::stod(s);
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

// Helper: parse a bool from string
static bool parseBool(const std::string& s) {
    return s == "1" || s == "true" || s == "TRUE";
}

// Split CSV params into parts
static std::vector<std::string> splitParams(std::string_view params) {
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

// Native entity factories - these are registered at startup
namespace detail {

std::unique_ptr<Entity> makePointEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 2) return nullptr;
    double x = parseDouble(parts[0]);
    double y = parseDouble(parts[1]);
    if (std::isnan(x) || std::isnan(y)) return nullptr;
    return std::make_unique<PointEntity>(Point2{x, y});
}

std::unique_ptr<Entity> makeLineEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 4) return nullptr;
    double x1 = parseDouble(parts[0]);
    double y1 = parseDouble(parts[1]);
    double x2 = parseDouble(parts[2]);
    double y2 = parseDouble(parts[3]);
    if (std::isnan(x1) || std::isnan(y1) || std::isnan(x2) || std::isnan(y2)) return nullptr;
    return std::make_unique<LineEntity>(Point2{x1, y1}, Point2{x2, y2});
}

std::unique_ptr<Entity> makeCircleEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 3) return nullptr;
    double cx = parseDouble(parts[0]);
    double cy = parseDouble(parts[1]);
    double r = parseDouble(parts[2]);
    if (std::isnan(cx) || std::isnan(cy) || std::isnan(r)) return nullptr;
    return std::make_unique<CircleEntity>(Point2{cx, cy}, r);
}

std::unique_ptr<Entity> makeArcEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() != 5) return nullptr;
    double cx = parseDouble(parts[0]);
    double cy = parseDouble(parts[1]);
    double r = parseDouble(parts[2]);
    double sa = parseDouble(parts[3]);
    double ea = parseDouble(parts[4]);
    if (std::isnan(cx) || std::isnan(cy) || std::isnan(r) || std::isnan(sa) || std::isnan(ea)) return nullptr;
    return std::make_unique<ArcEntity>(Point2{cx, cy}, r, sa, ea);
}

std::unique_ptr<Entity> makePolylineEntity(std::string_view params) {
    std::vector<std::string> parts = splitParams(params);
    if (parts.size() < 1) return nullptr;
    bool closed = parseBool(parts[0]);
    if ((parts.size() - 1) % 2 != 0) return nullptr;
    std::vector<Point2> vertices;
    for (size_t i = 1; i + 1 < parts.size(); i += 2) {
        double x = parseDouble(parts[i]);
        double y = parseDouble(parts[i + 1]);
        if (std::isnan(x) || std::isnan(y)) return nullptr;
        vertices.emplace_back(x, y);
    }
    return std::make_unique<PolylineEntity>(std::move(vertices), closed);
}

} // namespace detail

// Register all native entity types
void registerNativeEntityTypes() {
    auto& reg = EntityRegistry::instance();
    
    reg.registerType(TypeId_Point, detail::makePointEntity);
    reg.registerType(TypeId_Line, detail::makeLineEntity);
    reg.registerType(TypeId_Circle, detail::makeCircleEntity);
    reg.registerType(TypeId_Arc, detail::makeArcEntity);
    reg.registerType(TypeId_Polyline, detail::makePolylineEntity);
}

} // namespace bcad::geom