// Les ecrivains d'echange du noyau : une entite d'un module doit sortir avec ses
// proprietes sans qu'aucun nom metier n'apparaisse ici (ADR-016). Les cles sont
// donc inventees (`exotic.*`) : si ce test passait avec `cadastre.*`, il ne
// prouverait plus la gener icite.
#include "bcad/core/Document.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/io/Exchange.h"
#include "bcad/properties/PropertyMap.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

int countChar(const std::string& text, char needle) {
    return static_cast<int>(std::count(text.begin(), text.end(), needle));
}

std::string readFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// Une propriete que l'hote n'a jamais vue doit quand meme sortir : c'est le
// seul garant qu'un export ne soit pas reserve aux types du noyau.
void geoJsonCarriesEveryPropertyType() {
    bcad::core::Document document;
    auto* point = document.addEntity(
        std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{12.5, 40.25}));
    assert(point);
    auto& properties = point->properties();
    properties.addString("exotic.code", "AB-12");
    properties.addDouble("exotic.surface", 1024.5);
    properties.addInt("exotic.repetitions", 3);
    properties.addBool("exotic.valide", true);
    properties.addColor("exotic.couleur", bcad::geom::Color::fromRgb255(0x12, 0x34, 0x56));
    properties.addEnum("exotic.rang", 1, {"bas", "milieu", "haut"});

    const std::string json = bcad::io::toGeoJson(document);
    assert(contains(json, "\"exotic.code\":\"AB-12\""));
    assert(contains(json, "\"exotic.surface\":1024.5"));
    assert(contains(json, "\"exotic.repetitions\":3"));
    assert(contains(json, "\"exotic.valide\":true"));
    // Un color sort en hexadecimal, un enum sous son libelle : ce que lit
    // l'utilisateur dans le panneau de proprietes, pas un index opaque.
    assert(contains(json, "\"exotic.couleur\":\"#123456\""));
    assert(contains(json, "\"exotic.rang\":\"milieu\""));
    // De quoi raccrocher la feature a son entite d'origine.
    assert(contains(json, "\"bcad:id\":1"));
    assert(contains(json, "\"bcad:type\":\"" + std::string(bcad::geom::TypeId_Point.value) + "\""));
    assert(contains(json, "\"bcad:layer\""));
    std::printf("Exchange: GeoJSON emet toutes les proprietes\n");
}

// Un nom ou une valeur non JSON-sure ne doit pas casser le fichier.
void geoJsonEscapesStrings() {
    bcad::core::Document document;
    auto* point = document.addEntity(
        std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{1, 2}));
    point->properties().addString("exotic.note", "ligne1\nligne2 \"cite\" \\ dos");
    const std::string json = bcad::io::toGeoJson(document);
    assert(contains(json, "ligne1\\nligne2"));
    assert(contains(json, "\\\"cite\\\""));
    assert(contains(json, "\\\\ dos"));
    // Sinon le texte brut d'un guillemet aurait ferme la chaine plus tot.
    assert(!contains(json, "\"cite\" "));
    std::printf("Exchange: GeoJSON echappe les chaines\n");
}

// Une polyligne fermee est un polygone, et GeoJSON exige le sommet de fermeture
// repete ; l'ouvrir doit donner une LineString.
void geoJsonClosesPolygons() {
    bcad::core::Document document;
    const std::vector<bcad::geom::Point2> square{{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    document.addEntity(std::make_unique<bcad::geom::PolylineEntity>(square, true));
    const std::string closed = bcad::io::toGeoJson(document);
    assert(contains(closed, "\"Polygon\""));
    assert(contains(closed, "[[0,0],[10,0],[10,10],[0,10],[0,0]]"));

    document.clear();
    document.addEntity(std::make_unique<bcad::geom::PolylineEntity>(square, false));
    const std::string open = bcad::io::toGeoJson(document);
    assert(contains(open, "\"LineString\""));
    assert(!contains(open, "\"Polygon\""));
    std::printf("Exchange: GeoJSON ferme les polygones\n");
}

// Un cercle n'a pas d'equivalent GeoJSON direct : il doit tesseler plutot que
// disparaitre, sinon l'export est silencieusement faux.
void unknownGeometryIsTessellatedNotDropped() {
    bcad::core::Document document;
    document.addEntity(std::make_unique<bcad::geom::CircleEntity>(
        bcad::geom::Point2{5, 5}, 2.5));
    const std::string json = bcad::io::toGeoJson(document);
    assert(contains(json, "\"geometry\":{\"type\":\"LineString\""));
    assert(!contains(json, "\"geometry\":null"));
    std::printf("Exchange: geometrie inconnue tessellee\n");
}

void csvListsEveryVertex() {
    bcad::core::Document document;
    document.addEntity(std::make_unique<bcad::geom::PointEntity>(
        bcad::geom::Point2{1.5, 2.5}));
    document.addEntity(std::make_unique<bcad::geom::PolylineEntity>(
        std::vector<bcad::geom::Point2>{{0, 0}, {4, 0}, {4, 3}}));
    const std::string csv = bcad::io::toCoordinateCsv(document);
    assert(csv.rfind("entity_id,vertex_index,x,y\n", 0) == 0);
    assert(contains(csv, "1,0,1.5,2.5"));
    assert(contains(csv, "2,2,4,3"));
    // entete + 1 point + 3 sommets
    assert(countChar(csv, '\n') == 5);
    std::printf("Exchange: CSV liste chaque sommet\n");
}

void writeFailureIsReported() {
    std::string error;
    assert(!bcad::io::writeTextFile("/tmp/bcad-exchange-absent/introuvable.json", "{}", &error));
    assert(!error.empty());

    const std::string path = "/tmp/bcad-exchange-test.json";
    std::remove(path.c_str());
    error.clear();
    assert(bcad::io::writeTextFile(path, "{\"a\":1}\n", &error));
    assert(error.empty());
    assert(contains(readFile(path), "\"a\":1"));
    std::remove(path.c_str());
    std::printf("Exchange: ecriture impossible signalee\n");
}

} // namespace

int main() {
    geoJsonCarriesEveryPropertyType();
    geoJsonEscapesStrings();
    geoJsonClosesPolygons();
    unknownGeometryIsTessellatedNotDropped();
    csvListsEveryVertex();
    writeFailureIsReported();
    std::printf("Ecrivains d'echange : tests PASSED\n");
    return 0;
}
