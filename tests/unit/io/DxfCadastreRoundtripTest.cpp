#include "bcad/io/DxfWriter.h"
#include "bcad/io/DxfReader.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/cadastre/ParcelOps.h"
#include <cassert>
#include <filesystem>
#include <string>

using namespace bcad;
using namespace bcad::geom;
using namespace bcad::cadastre;

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
    assert(e.typeId().value == "cadastre.parcel");

    auto* p = dynamic_cast<PolylineEntity*>(&e);
    assert(p != nullptr);

    assert(p->properties().getString("cadastre.section") == "A");
    assert(p->properties().getString("cadastre.numero") == "42");
    assert(p->properties().getString("cadastre.contenance") == "500m2");
    assert(p->properties().getString("cadastre.commune") == "Severin");

    std::filesystem::remove(path);

    return 0;
}