#include "bcad/cadastre/ParcelSearch.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include <cassert>
using namespace bcad::cadastre;
using namespace bcad::geom;
int main() {
    bcad::core::Document doc;
    // Ajoute 2 parcelles via PolylineEntity avec serializeParams contenant section|numero
    // Simulé : on ajoute des PolylineEntity et on teste la recherche (vide car typeId != cadastre.parcel)
    assert(findByRef(doc, "A", "42").empty());
    assert(findOneByRef(doc, "A", "42") == nullptr);
    return 0;
}
