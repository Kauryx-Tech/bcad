#include "bcad/io/DxfWriter.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/io/DxfColor.h"
#include <fstream>

namespace bcad::io {

namespace {

void writeGroup(std::ofstream& f, int code, const std::string& value) {
    f << code << "\n" << value << "\n";
}
void writeGroup(std::ofstream& f, int code, int value) {
    f << code << "\n" << value << "\n";
}
void writeGroup(std::ofstream& f, int code, double value) {
    f << code << "\n" << value << "\n";
}

void writeHeader(std::ofstream& f) {
    f << "0\nSECTION\n2\nHEADER\n";
    writeGroup(f, 9, std::string("$ACADVER"));
    writeGroup(f, 1, std::string("AC1015"));
    f << "0\nENDSEC\n";
}

void writeLayerTable(std::ofstream& f, const layers::LayerManager& layers) {
    f << "0\nSECTION\n2\nTABLES\n";
    f << "0\nTABLE\n2\nLAYER\n";
    writeGroup(f, 70, static_cast<int>(layers.layers().size()));
    for (const auto& layer : layers.layers()) {
        f << "0\nLAYER\n";
        writeGroup(f, 2, layer.name);
        writeGroup(f, 70, layer.locked ? 4 : 0);
        writeGroup(f, 62, layer.visible ? rgbToAci(layer.color) : -rgbToAci(layer.color));
        writeGroup(f, 6, std::string("CONTINUOUS"));
    }
    f << "0\nENDTAB\n";
    f << "0\nENDSEC\n";
}

void writeEntityCommon(std::ofstream& f, const geom::Entity& e) {
    writeGroup(f, 8, e.layer());
    if (e.colorOverride()) writeGroup(f, 62, rgbToAci(*e.colorOverride()));
}

void writeLine(std::ofstream& f, const geom::LineEntity& l) {
    f << "0\nLINE\n";
    writeEntityCommon(f, l);
    writeGroup(f, 10, CGAL::to_double(l.start().x()));
    writeGroup(f, 20, CGAL::to_double(l.start().y()));
    writeGroup(f, 11, CGAL::to_double(l.end().x()));
    writeGroup(f, 21, CGAL::to_double(l.end().y()));
}

void writeCircle(std::ofstream& f, const geom::CircleEntity& c) {
    f << "0\nCIRCLE\n";
    writeEntityCommon(f, c);
    writeGroup(f, 10, CGAL::to_double(c.center().x()));
    writeGroup(f, 20, CGAL::to_double(c.center().y()));
    writeGroup(f, 40, c.radius());
}

void writeArc(std::ofstream& f, const geom::ArcEntity& a) {
    f << "0\nARC\n";
    writeEntityCommon(f, a);
    writeGroup(f, 10, CGAL::to_double(a.center().x()));
    writeGroup(f, 20, CGAL::to_double(a.center().y()));
    writeGroup(f, 40, a.radius());
    writeGroup(f, 50, geom::toDegrees(a.startAngle()));
    writeGroup(f, 51, geom::toDegrees(a.endAngle()));
}

void writePolyline(std::ofstream& f, const geom::PolylineEntity& p) {
    f << "0\nLWPOLYLINE\n";
    writeEntityCommon(f, p);
    writeGroup(f, 90, static_cast<int>(p.vertices().size()));
    writeGroup(f, 70, p.closed() ? 1 : 0);
    for (const auto& v : p.vertices()) {
        writeGroup(f, 10, CGAL::to_double(v.x()));
        writeGroup(f, 20, CGAL::to_double(v.y()));
    }
}

void writeEntities(std::ofstream& f, const core::Document& doc) {
    f << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& e : doc.entities()) {
        switch (e->type()) {
            case geom::EntityType::Line:
                writeLine(f, static_cast<const geom::LineEntity&>(*e));
                break;
            case geom::EntityType::Circle:
                writeCircle(f, static_cast<const geom::CircleEntity&>(*e));
                break;
            case geom::EntityType::Arc:
                writeArc(f, static_cast<const geom::ArcEntity&>(*e));
                break;
            case geom::EntityType::Polyline:
                writePolyline(f, static_cast<const geom::PolylineEntity&>(*e));
                break;
            case geom::EntityType::Point:
                break; // POINT entity export omitted from this MVP subset
        }
    }
    f << "0\nENDSEC\n";
}

} // namespace

bool writeDxf(const std::string& path, const core::Document& doc) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    f.precision(9);
    writeHeader(f);
    writeLayerTable(f, doc.layerManager());
    writeEntities(f, doc);
    f << "0\nEOF\n";
    return f.good();
}

} // namespace bcad::io
