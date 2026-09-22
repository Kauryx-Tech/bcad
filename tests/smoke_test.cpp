// Test de fumée léger à base d'assertions (pas de framework de test externe)
// couvrant un comportement de chaque module : calculs géométriques, entités,
// opérations booléennes, triangulation, calques, le pipeline
// Document/quadtree, et les E/S DXF/SQLite.
// Ne remplace pas de vrais tests unitaires, mais suffit pour détecter une
// compilation cassée.

#include "bcad/app/CoordinateInput.h"
#include "bcad/geometry/GeometryEngine.h"
#include "bcad/core/Document.h"
#include "bcad/io/Database.h"
#include "bcad/io/DxfReader.h"
#include "bcad/io/DxfWriter.h"
#include "bcad/render/Grid.h"
#include "bcad/serialization/Serializer.h"
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace {
int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}
} // namespace

using namespace bcad;

void testGeometryUtils() {
    geom::Point2 a(0, 0), b(3, 4);
    check(std::abs(geom::distance(a, b) - 5.0) < 1e-9, "distance(3-4-5 triangle) == 5");
    check(std::abs(geom::angleOf(a, geom::Point2(1, 0))) < 1e-9, "angleOf along +x is 0");
}

void testEntities() {
    geom::LineEntity line(geom::Point2(0, 0), geom::Point2(10, 0));
    check(std::abs(line.length() - 10.0) < 1e-9, "LineEntity length");

    geom::CircleEntity circle(geom::Point2(0, 0), 5.0);
    auto pts = circle.tessellate(0.01);
    check(pts.size() > 12, "CircleEntity tessellate produces enough segments");
    check(std::abs(circle.distanceTo(geom::Point2(5, 0)) ) < 1e-9, "point on circle has zero distanceTo");

    line.applyTransform(geom::Transform2D::translation(1, 1));
    check(std::abs(line.start().x_ - 1.0) < 1e-9, "translation moves entity");

    // Le symétrique de (3,4) par rapport à l'axe X (y=0) doit être (3,-4).
    auto mirrorX = geom::Transform2D::mirrorAcrossLine(geom::Point2(0, 0), geom::Point2(1, 0));
    geom::Point2 mirrored = mirrorX.transform(geom::Point2(3, 4));
    check(std::abs(mirrored.x_ - 3.0) < 1e-9 && std::abs(mirrored.y_ + 4.0) < 1e-9,
          "mirrorAcrossLine across the X axis negates y");
}

void testBooleanOpsAndTriangulation() {
    geom::PolylineEntity squareA({ geom::Point2(0, 0), geom::Point2(2, 0), geom::Point2(2, 2), geom::Point2(0, 2) }, true);
    geom::PolylineEntity squareB({ geom::Point2(1, 1), geom::Point2(3, 1), geom::Point2(3, 3), geom::Point2(1, 3) }, true);

    double areaA = geom::polygonArea(squareA);
    check(std::abs(areaA - 4.0) < 1e-9, "unit-square polygon area");

    auto unioned = geom::booleanOp(squareA, squareB, geom::BooleanOp::Union);
    check(!unioned.empty(), "union of overlapping squares is non-empty");
    if (!unioned.empty()) {
        double ua = std::abs(geom::polygonArea(unioned[0]));
        check(ua > 4.0 && ua < 8.0, "union area is between one square and the sum of both");
    }

    auto tris = geom::delaunayTriangulate({ geom::Point2(0, 0), geom::Point2(2, 0), geom::Point2(2, 2), geom::Point2(0, 2) });
    check(tris.size() == 2, "Delaunay triangulation of a square yields 2 triangles");
}

void testSnapGeometry() {
    // Deux segments qui se croisent en (5,5).
    geom::LineEntity a(geom::Point2(0, 0), geom::Point2(10, 10));
    geom::LineEntity b(geom::Point2(0, 10), geom::Point2(10, 0));
    auto hits = geom::entityIntersections(a, b);
    check(hits.size() == 1, "line-line intersection finds exactly one point");
    if (!hits.empty()) {
        check(std::abs(hits[0].x_ - 5.0) < 1e-6 &&
                  std::abs(hits[0].y_ - 5.0) < 1e-6,
              "line-line intersection lands at the expected crossing point");
    }

    // Une ligne horizontale traversant un cercle centré à l'origine : deux points à x = +-5.
    geom::LineEntity horiz(geom::Point2(-10, 0), geom::Point2(10, 0));
    geom::CircleEntity circle(geom::Point2(0, 0), 5.0);
    auto circHits = geom::entityIntersections(horiz, circle);
    check(circHits.size() == 2, "line-circle intersection finds two points");

    // Le pied de la perpendiculaire de (5,5) sur le segment de l'axe X doit être (5,0).
    geom::LineEntity xAxis(geom::Point2(-10, 0), geom::Point2(10, 0));
    auto foot = geom::perpendicularFoot(xAxis, geom::Point2(5, 5), geom::Point2(5, 0));
    check(foot.has_value(), "perpendicularFoot finds a foot on a line");
    if (foot) {
        check(std::abs(foot->x_ - 5.0) < 1e-9 && std::abs(foot->y_) < 1e-9,
              "perpendicular foot from (5,5) onto the X axis is (5,0)");
    }
}

void testCoordinateInput() {
    auto abs1 = app::parseCoordinateInput("12,7", std::nullopt);
    check(abs1.has_value(), "parseCoordinateInput accepts absolute cartesian");
    if (abs1) check(abs1->x_ == 12.0 && abs1->y_ == 7.0, "absolute cartesian value is correct");

    geom::Point2 ref(10, 10);
    auto rel = app::parseCoordinateInput("@5,-3", ref);
    check(rel.has_value(), "parseCoordinateInput accepts relative cartesian with a reference");
    if (rel) check(rel->x_ == 15.0 && rel->y_ == 7.0, "relative cartesian is offset from the reference");

    check(!app::parseCoordinateInput("@5,3", std::nullopt).has_value(),
          "relative form is rejected without a reference point");

    auto polar = app::parseCoordinateInput("@10<90", ref);
    check(polar.has_value(), "parseCoordinateInput accepts relative polar");
    if (polar) {
        check(std::abs(polar->x_ - 10.0) < 1e-9 && std::abs(polar->y_ - 20.0) < 1e-9,
              "relative polar @10<90 from (10,10) lands at (10,20)");
    }

    check(!app::parseCoordinateInput("garbage", std::nullopt).has_value(), "malformed input is rejected");
}

void testLayerManager() {
    layers::LayerManager lm;
    check(lm.find("0") != nullptr, "default layer 0 exists");
    lm.createLayer("Walls", geom::Color::fromRgb255(200, 50, 50));
    check(lm.find("Walls") != nullptr, "createLayer adds a findable layer");
    check(!lm.removeLayer("0"), "layer 0 cannot be removed");
    check(lm.removeLayer("Walls"), "non-default layer can be removed");
}

void testDocumentAndQuadtree() {
    core::Document doc;
    auto* line = doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(10, 10)));
    check(line->id() > 0, "addEntity assigns a positive id");

    auto inRegion = doc.entitiesInRegion(geom::BoundingBox{ -1, -1, 11, 11 });
    check(inRegion.size() == 1, "entitiesInRegion finds the inserted entity");

    geom::Entity* picked = doc.pickEntity(geom::Point2(5, 5), 0.5);
    check(picked == line, "pickEntity finds a point near the line");

    doc.removeEntity(line->id());
    check(doc.entities().empty(), "removeEntity empties the document");
}

void testDxfRoundTrip() {
    core::Document doc;
    doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(5, 5)));
    doc.addEntity(std::make_unique<geom::CircleEntity>(geom::Point2(1, 1), 2.5));

    std::filesystem::path path = std::filesystem::temp_directory_path() / "bcad_smoke_test.dxf";
    check(io::writeDxf(path.string(), doc), "writeDxf succeeds");

    core::Document loaded;
    check(io::readDxf(path.string(), loaded), "readDxf succeeds");
    check(loaded.entities().size() == 2, "readDxf recovers both entities");

    std::filesystem::remove(path);
}

void testTolerance() {
    check(geom::nearlyZero(1e-15), "nearlyZero treats sub-epsilon values as zero");
    check(!geom::nearlyZero(1e-3), "nearlyZero rejects values well above epsilon");
    check(geom::nearlyEqual(1.0, 1.0 + 1e-15), "nearlyEqual tolerates float noise");
    check(!geom::nearlyEqual(1.0, 1.1), "nearlyEqual rejects genuinely different values");
}

void testGrid() {
    // À 40 px/unité avec un pas visé de 40px à l'écran, l'espacement doit
    // se rapprocher de 1 unité monde (le cas de base de la séquence "propre" 1/2/5).
    double spacing = render::adaptiveGridSpacing(/*pixelsPerUnit=*/40.0, /*targetPx=*/40.0);
    check(std::abs(spacing - 1.0) < 1e-9, "adaptiveGridSpacing picks 1.0 at matching zoom/target");

    check(std::abs(render::snapToGrid(2.3, 1.0) - 2.0) < 1e-9, "snapToGrid rounds to nearest step");
    check(std::abs(render::snapToGrid(-1.6, 1.0) - (-2.0)) < 1e-9, "snapToGrid rounds negative values correctly");
}

void testSqliteRoundTrip() {
    core::Document doc;
    doc.layerManager().createLayer("Dimensions", geom::Color::fromRgb255(0, 180, 0));
    doc.addEntity(std::make_unique<geom::ArcEntity>(geom::Point2(0, 0), 3.0, 0.0, 1.5));

    std::filesystem::path path = std::filesystem::temp_directory_path() / "bcad_smoke_test.bcad";
    std::filesystem::remove(path);
    check(io::Database::save(path.string(), doc), "Database::save succeeds");

    core::Document loaded;
    check(io::Database::load(path.string(), loaded), "Database::load succeeds");
    check(loaded.entities().size() == 1, "Database round-trip recovers the entity");
    check(loaded.layerManager().find("Dimensions") != nullptr, "Database round-trip recovers the layer");

    std::filesystem::remove(path);
}

void testArcSerialization() {
    // Ensure native serializers are registered
    serialization::SerializerRegistry::initializeNativeSerializers();
    
    // Test direct Arc serialization/deserialization
    auto arc = std::make_unique<geom::ArcEntity>(geom::Point2(0, 0), 3.0, 0.0, 1.5);
    const auto* serializer = serialization::SerializerRegistry::find(arc->typeId());
    check(serializer != nullptr, "Arc serializer registered");
    if (serializer) {
        std::string serialized = serializer->serialize(*arc);
        check(!serialized.empty(), "Arc serialization produces non-empty string");
        
        auto deserialized = serializer->deserialize(serialized);
        check(deserialized != nullptr, "Arc deserialization succeeds");
        if (deserialized) {
            auto* loadedArc = dynamic_cast<geom::ArcEntity*>(deserialized.get());
            check(loadedArc != nullptr, "Deserialized entity is ArcEntity");
            if (loadedArc) {
                check(std::abs(loadedArc->center().x_ - 0.0) < 1e-9, "Arc center x preserved");
                check(std::abs(loadedArc->center().y_ - 0.0) < 1e-9, "Arc center y preserved");
                check(std::abs(loadedArc->radius() - 3.0) < 1e-9, "Arc radius preserved");
                check(std::abs(loadedArc->startAngle() - 0.0) < 1e-9, "Arc startAngle preserved");
                check(std::abs(loadedArc->endAngle() - 1.5) < 1e-9, "Arc endAngle preserved");
            }
        }
    }
}

int main() {
    testGeometryUtils();
    testEntities();
    testBooleanOpsAndTriangulation();
    testSnapGeometry();
    testCoordinateInput();
    testLayerManager();
    testDocumentAndQuadtree();
    testTolerance();
    testGrid();
    testDxfRoundTrip();
    testSqliteRoundTrip();
    testArcSerialization();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
