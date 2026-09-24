#include "GeoJsonSerializer.h"
#include "CsvCoordinateExporter.h"
#include "GeoPackageSerializer.h"
#include "entities/ParcelEntity.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"

#include <cassert>
#include <string>
#include <vector>
#include <filesystem>

int main() {
    using namespace bcad::cadastre;
    using namespace bcad::geom;

    GeoJsonSerializer geojson;
    PointEntity point(Point2{12.5, -3.25});
    point.setId(7);
    const auto pointJson = geojson.serialize(point);
    assert(pointJson.find("\"Point\"") != std::string::npos);
    auto pointCopy = geojson.deserialize(pointJson);
    assert(pointCopy);
    const auto* restoredPoint = dynamic_cast<const PointEntity*>(pointCopy.get());
    assert(restoredPoint);
    assert(restoredPoint->position().x_ == 12.5);
    assert(restoredPoint->position().y_ == -3.25);

    PolylineEntity parcel({Point2{0, 0}, Point2{10, 0}, Point2{10, 5},
                           Point2{0, 5}}, true);
    parcel.setId(42);
    const auto polygonJson = geojson.serialize(parcel);
    assert(polygonJson.find("\"Polygon\"") != std::string::npos);
    auto polygonCopy = geojson.deserialize(polygonJson);
    assert(polygonCopy);
    const auto* restoredPolygon =
        dynamic_cast<const PolylineEntity*>(polygonCopy.get());
    assert(restoredPolygon);
    assert(restoredPolygon->closed());
    assert(restoredPolygon->vertices().size() == 4);

    assert(!geojson.deserialize("{}"));
    assert(!geojson.deserialize(
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Circle\","
        "\"coordinates\":[0,0,1]}}"));

    CsvCoordinateExporter csv;
    std::vector<Entity*> entities{&point, &parcel};
    const auto csvText = csv.exportCoordinates(entities);
    assert(csvText.find("entity_id,vertex_index,x,y\n") == 0);
    assert(csvText.find("7,0,") != std::string::npos);
    assert(csvText.find("42,0,") != std::string::npos);
    assert(csv.exportCoordinates({nullptr}) ==
           "entity_id,vertex_index,x,y\n");

    bcad::core::Document document;
    auto sourceParcel = std::make_unique<ParcelEntity>(
        std::vector<Point2>{{0, 0}, {20, 0}, {20, 10}, {0, 10}});
    sourceParcel->properties().setString("cadastre.section", "B");
    sourceParcel->properties().setString("cadastre.numero", "12");
    sourceParcel->properties().setString("cadastre.contenance", "200m2");
    document.addEntity(std::move(sourceParcel));
    const auto packagePath =
        (std::filesystem::temp_directory_path() / "bcad-cadastre-test.gpkg").string();
    std::filesystem::remove(packagePath);
    GeoPackageSerializer geopackage;
    assert(geopackage.write(packagePath, document));
    bcad::core::Document restored;
    assert(geopackage.read(packagePath, restored));
    assert(restored.entities().size() == 1);
    const auto* restoredParcel =
        dynamic_cast<const ParcelEntity*>(restored.entities().front().get());
    assert(restoredParcel);
    assert(restoredParcel->vertices().size() == 4);
    assert(restoredParcel->properties().getString("cadastre.section") == "B");
    assert(!geopackage.read(packagePath + ".missing", restored));
    std::filesystem::remove(packagePath);
}
