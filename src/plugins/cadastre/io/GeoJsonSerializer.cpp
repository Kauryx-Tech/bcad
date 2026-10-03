#include "GeoJsonSerializer.h"
#include "CsvCoordinateExporter.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"

#include <iomanip>
#include <regex>
#include <sstream>

namespace bcad::cadastre {

namespace {
std::string coordinates(const std::vector<geom::Point2>& points) {
    std::ostringstream out;
    out << std::setprecision(17) << '[';
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i) out << ',';
        out << '[' << points[i].x_ << ',' << points[i].y_ << ']';
    }
    return out.str() + ']';
}
}

std::string GeoJsonSerializer::serialize(const geom::Entity& entity) const {
    if (const auto* point = dynamic_cast<const geom::PointEntity*>(&entity)) {
        return "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":["
            + std::to_string(point->position().x_) + ","
            + std::to_string(point->position().y_) + "]},\"properties\":{}}";
    }
    const auto* polyline = dynamic_cast<const geom::PolylineEntity*>(&entity);
    if (!polyline) return {};

    if (!polyline->closed())
        return std::string("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":")
            + coordinates(polyline->vertices()) + "},\"properties\":{}}";

    // GeoJSON Polygon : premier anneau = extérieur, suivants = trous.
    std::string rings = "[" + coordinates(polyline->vertices());
    for (const auto& hole : polyline->holes()) {
        if (hole.size() >= 3) rings += "," + coordinates(hole);
    }
    rings += "]";
    return std::string("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Polygon\",\"coordinates\":")
        + rings + "},\"properties\":{}}";
}

std::unique_ptr<geom::Entity> GeoJsonSerializer::deserialize(
    const std::string& json) const {
    const auto geometry = json.find("\"geometry\"");
    if (geometry == std::string::npos) return nullptr;
    const auto pointType = json.find("\"Point\"", geometry);
    const auto lineType = json.find("\"LineString\"", geometry);
    const auto polygonType = json.find("\"Polygon\"", geometry);
    std::regex numberPattern(R"(-?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)");
    std::vector<double> numbers;
    for (auto it = std::sregex_iterator(json.begin() + static_cast<std::ptrdiff_t>(geometry),
                                         json.end(), numberPattern);
         it != std::sregex_iterator(); ++it) {
        numbers.push_back(std::stod(it->str()));
    }
    if (pointType != std::string::npos && numbers.size() >= 2) {
        return std::make_unique<geom::PointEntity>(
            geom::Point2{numbers[0], numbers[1]});
    }
    if ((lineType != std::string::npos || polygonType != std::string::npos)
        && numbers.size() >= 6) {
        std::vector<geom::Point2> points;
        for (std::size_t i = 0; i + 1 < numbers.size(); i += 2)
            points.emplace_back(numbers[i], numbers[i + 1]);
        return std::make_unique<geom::PolylineEntity>(
            std::move(points), polygonType != std::string::npos);
    }
    return nullptr;
}

std::string CsvCoordinateExporter::exportCoordinates(
    const std::vector<geom::Entity*>& entities) const {
    std::ostringstream out;
    out << "entity_id,vertex_index,x,y\n";
    for (const auto* entity : entities) {
        if (!entity) continue;
        const auto points = entity->tessellate(0.01);
        for (std::size_t i = 0; i < points.size(); ++i) {
            out << entity->id() << ',' << i << ','
                << std::setprecision(17) << points[i].x_ << ','
                << points[i].y_ << '\n';
        }
    }
    return out.str();
}

} // namespace bcad::cadastre
