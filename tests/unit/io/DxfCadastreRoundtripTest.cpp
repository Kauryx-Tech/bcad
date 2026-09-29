#include "bcad/io/DxfWriter.h"
#include "bcad/io/DxfReader.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include <cassert>
#include <filesystem>
#include <string>

using namespace bcad;
using namespace bcad::geom;

int main() {
    core::Document doc;

    PolylineEntity parcel({{0,0},{10,0},{10,5},{0,5}}, true);
    parcel.properties().setString("cadastre.section", "A");
    parcel.properties().setString("cadastre.numero", "42");
    parcel.properties().setString("cadastre.contenance", "500m2");
    parcel.properties().setString("cadastre.commune", "Severin");

    doc.addEntity(std::make_unique<PolylineEntity>(std::move(parcel)));

    std::string path = "/tmp/bcad_cadastre_dxf_test.dxf";
    assert(io::writeDxf(path, doc));

    core::Document doc2;
    assert(io::readDxf(path, doc2));

    assert(doc2.entities().size() == 1);
    auto& e = *doc2.entities().front();
    // Le DXF ne porte pas le TypeId d'une entite de plugin : une parcelle y est
    // une LWPOLYLINE fermee enrichie d'une XDATA BCAD_CADASTRE. La promotion vers
    // cadastre.parcel est faite par le plugin (commande), pas par src/io.
    assert(e.typeId() == "bcad.Polyline");

    auto* p = dynamic_cast<PolylineEntity*>(&e);
    assert(p != nullptr);
    assert(p->closed());
    assert(p->vertices().size() == 4);

    assert(p->properties().getString("cadastre.section") == "A");
    assert(p->properties().getString("cadastre.numero") == "42");
    assert(p->properties().getString("cadastre.contenance") == "500m2");
    assert(p->properties().getString("cadastre.commune") == "Severin");

    std::filesystem::remove(path);

    return 0;
}