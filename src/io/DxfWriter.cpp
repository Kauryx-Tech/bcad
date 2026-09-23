#include "bcad/core/Document.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include <fstream>

namespace bcad::io {

namespace {
constexpr const char* kCadastreAppId = "BCAD_CADASTRE";

void writeAppIdTable(std::ofstream& f) {
    f << "0\nTABLE\n2\nAPPID\n";
    f << "70\n1\n";
    f << "0\nAPPID\n";
    f << "2\nBCAD_CADASTRE\n";
    f << "70\n0\n";
    f << "0\nENDTAB\n";
}

void writeCadastreXData(std::ofstream& f, const geom::PolylineEntity& poly) {
    const auto& props = poly.properties();
    auto getStr = [&](const std::string& key) -> std::string {
        if (auto* p = props.get(key)) return p->asString();
        return "";
    };

    f << "1001\nBCAD_CADASTRE\n";
    f << "1002\n{\n";
    // section
    f << "1000\nsection\n";
    f << "1000\n" << getStr("cadastre.section") << "\n";
    // numero
    f << "1000\nnumero\n";
    f << "1000\n" << getStr("cadastre.numero") << "\n";
    // contenance
    f << "1000\ncontenance\n";
    f << "1000\n" << getStr("cadastre.contenance") << "\n";
    // commune
    f << "1000\ncommune\n";
    f << "1000\n" << getStr("cadastre.commune") << "\n";
    f << "1002\n}\n";
}

bool isCadastreParcel(const geom::Entity* e) {
    return e->typeId().value == "cadastre.parcel";
}

} // namespace

bool writeDxf(const std::string& path, const core::Document& doc) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) return false;
    f.precision(9);

    f << "0\nSECTION\n2\nHEADER\n";
    f << "9\n$ACADVER\n1\nAC1015\n";
    f << "0\nENDSEC\n";

    f << "0\nSECTION\n2\nTABLES\n";
    f << "0\nTABLE\n2\nLAYER\n";
    auto& layers = doc.layerManager().layers();
    f << "70\n" << layers.size() << "\n";
    for (const auto& layer : layers) {
        f << "0\nLAYER\n";
        f << "2\n" << layer.name << "\n";
        f << "70\n" << (layer.locked ? 4 : 0) << "\n";
        int r = static_cast<int>(layer.color.r * 255);
        int g = static_cast<int>(layer.color.g * 255);
        int b = static_cast<int>(layer.color.b * 255);
        int aci = (r == g && g == b) ? std::clamp(r / 8, 1, 255) : 7;
        f << "62\n" << (layer.visible ? aci : -aci) << "\n";
        f << "6\nCONTINUOUS\n";
    }
    f << "0\nENDTAB\n";
    // APPID table
    writeAppIdTable(f);
    f << "0\nENDSEC\n";

    f << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& e : doc.entities()) {
        e->writeDxf(f, e->layer(), e->colorOverride());
        // XDATA pour parcelles cadastre
        if (isCadastreParcel(e.get())) {
            const auto& poly = static_cast<const geom::PolylineEntity&>(*e);
            writeCadastreXData(f, poly);
        }
    }
    f << "0\nENDSEC\n";

    f << "0\nEOF\n";

    return f.good();
}

} // namespace bcad::io