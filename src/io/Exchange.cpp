#include "bcad/io/Exchange.h"
#include "JsonText.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/properties/PropertyMap.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace bcad::io {

namespace {

std::string point(const geom::Point2& p) {
    return "[" + jsonNumber(p.x_) + "," + jsonNumber(p.y_) + "]";
}

std::string ring(const std::vector<geom::Point2>& points) {
    std::string out = "[";
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i) out += ',';
        out += point(points[i]);
    }
    return out + "]";
}

std::string lineString(const std::vector<geom::Point2>& points) {
    return "{\"type\":\"LineString\",\"coordinates\":" + ring(points) + "}";
}

// Le noyau ne connaît que deux géométries. Tout le reste — cercles, arcs, et
// les types d'un module qui n'hérite pas de ceux-là — passe par tessellate(),
// donc une entité ne peut pas disparaître de l'export sans que le fichier le
// dise.
std::string geometryOf(const geom::Entity& entity) {
    if (const auto* pointEntity = dynamic_cast<const geom::PointEntity*>(&entity))
        return "{\"type\":\"Point\",\"coordinates\":" + point(pointEntity->position()) + "}";

    if (const auto* polyline = dynamic_cast<const geom::PolylineEntity*>(&entity)) {
        std::vector<geom::Point2> vertices = polyline->vertices();
        if (vertices.empty()) return "null";
        if (!polyline->closed()) return lineString(vertices);
        if (vertices.size() < 3) return lineString(vertices);
        vertices.push_back(vertices.front());
        // GeoJSON Polygon : premier anneau = extérieur, suivants = trous.
        // Un anneau de trou se ferme aussi (premier sommet répété en dernier).
        std::string coords = "[" + ring(vertices);
        for (const auto& hole : polyline->holes()) {
            if (hole.size() < 3) continue;
            std::vector<geom::Point2> hClosed = hole;
            hClosed.push_back(hClosed.front());
            coords += "," + ring(hClosed);
        }
        coords += "]";
        return "{\"type\":\"Polygon\",\"coordinates\":" + coords + "}";
    }

    const auto points = entity.tessellate(0.01);
    if (points.empty()) return "null";
    if (points.size() == 1) return "{\"type\":\"Point\",\"coordinates\":" + point(points.front()) + "}";
    return lineString(points);
}

// Valeur d'une propriété rendue selon son type déclaré. Un enum sort sous son
// libellé (c'est ce que l'utilisateur lit dans le panneau de propriétés), un
// color sous sa forme hexadécimale.
std::string propertyValue(const properties::Property& property) {
    switch (property.type()) {
    case properties::PropertyType::Double:
        return jsonNumber(property.asDouble());
    case properties::PropertyType::Int:
        return std::to_string(property.asInt());
    case properties::PropertyType::Bool:
        return property.asBool() ? "true" : "false";
    case properties::PropertyType::Color: {
        const auto color = property.asColor();
        auto byte = [](float channel) {
            const int value = static_cast<int>(
                std::clamp(channel, 0.0f, 1.0f) * 255.0f + 0.5f);
            return value;
        };
        std::ostringstream out;
        out << "\"#" << std::hex << std::setw(2) << std::setfill('0')
            << byte(color.r) << byte(color.g) << byte(color.b)
            << std::dec << std::setfill(' ') << "\"";
        return out.str();
    }
    case properties::PropertyType::Enum: {
        const int index = property.asEnum();
        const auto& labels = property.enumValues();
        if (index >= 0 && static_cast<std::size_t>(index) < labels.size())
            return jsonString(labels[static_cast<std::size_t>(index)]);
        return std::to_string(index);
    }
    case properties::PropertyType::String:
        return jsonString(property.asString());
    }
    return "null";
}

std::string propertiesOf(const geom::Entity& entity) {
    std::string out = "{\"bcad:id\":" + std::to_string(entity.id())
        + ",\"bcad:type\":" + jsonString(std::string(entity.typeId().value))
        + ",\"bcad:layer\":" + jsonString(entity.layer());

    const auto& map = entity.properties();
    // Le PropertyMap est une table de hachage : trier pour qu'un même document
    // produise le même octet d'un export à l'autre.
    auto names = map.listNames();
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        const auto* property = map.get(name);
        if (!property) continue;
        out += ',' + jsonString(name) + ':' + propertyValue(*property);
    }
    return out + "}";
}

std::string featureOf(const geom::Entity& entity) {
    return "{\"type\":\"Feature\",\"geometry\":" + geometryOf(entity)
        + ",\"properties\":" + propertiesOf(entity) + "}";
}

} // namespace

std::string toGeoJson(const core::Document& document) {
    std::string out = "{\"type\":\"FeatureCollection\",\"features\":[";
    bool first = true;
    for (const auto& entity : document.entities()) {
        if (!entity) continue;
        if (!first) out += ',';
        first = false;
        out += featureOf(*entity);
    }
    return out + "]}\n";
}

std::string toCoordinateCsv(const core::Document& document) {
    std::ostringstream out;
    out << "entity_id,vertex_index,x,y\n" << std::setprecision(17);
    for (const auto& entity : document.entities()) {
        if (!entity) continue;
        std::vector<geom::Point2> points;
        if (const auto* pointEntity = dynamic_cast<const geom::PointEntity*>(entity.get()))
            points.push_back(pointEntity->position());
        else
            points = entity->tessellate(0.01);
        for (std::size_t i = 0; i < points.size(); ++i)
            out << entity->id() << ',' << i << ',' << points[i].x_ << ',' << points[i].y_ << '\n';
    }
    return out.str();
}

bool writeTextFile(const std::string& path, const std::string& text, std::string* error) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        if (error) *error = "écriture impossible : " + path;
        return false;
    }
    output << text;
    output.close();
    if (!output) {
        if (error) *error = "erreur pendant l'écriture de : " + path;
        return false;
    }
    return true;
}

} // namespace bcad::io
