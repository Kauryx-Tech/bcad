// Cotations enregistrees et relues (A-01), et nombres lus d'un fichier bornes.
//
// La hauteur de texte d'une cotation vit dans la propriete
// `dimension.text_height` : elle doit survivre a l'enregistrement. Un .bcad
// portant un infini, un NaN ou une coordonnee demesuree figeait l'application
// (boucle de grille sans fin) : l'entite est maintenant ecartee a la lecture.

#include "bcad/core/Document.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/io/Database.h"
#include "bcad/serialization/Serializer.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>

using namespace bcad;

int main() {
    serialization::SerializerRegistry::initializeNativeSerializers();

    // --- Enregistrement et relecture : geometrie, direction, hauteur, calque ---
    core::Document doc;
    doc.layerManager().createLayer("Cotations", geom::Color::fromRgb255(0, 200, 230));
    auto dim = std::make_unique<geom::LinearDimensionEntity>(geom::Point2(0, 0), geom::Point2(10, 3),
                                                             geom::Point2(15, 1), M_PI / 2);
    dim->setLayer("Cotations");
    dim->properties().setDouble(geom::kDimensionTextHeightProperty, 0.75);
    doc.addEntity(std::move(dim));

    const auto path = std::filesystem::temp_directory_path() / "bcad_dimension_persistence.bcad";
    std::filesystem::remove(path);
    assert(io::Database::save(path.string(), doc));
    core::Document loaded;
    assert(io::Database::load(path.string(), loaded));
    std::filesystem::remove(path);
    assert(loaded.entities().size() == 1);
    const auto* back = dynamic_cast<const geom::LinearDimensionEntity*>(loaded.entities()[0].get());
    assert(back && back->layer() == "Cotations");
    assert(std::abs(back->measuredValue() - 3.0) < 1e-9);                  // verticale gardee
    assert(std::abs(geom::dimensionTextHeight(*back) - 0.75) < 1e-12);

    // --- Nombres hors limites : l'entite est ecartee, rien ne fige ---
    const auto* linear = serialization::SerializerRegistry::find(geom::TypeId_LinearDimension);
    assert(linear);
    assert(linear->deserialize("0,0,10,0,5,2,0,Standard"));
    for (const char* bad : {"inf,0,10,0,5,2,0,Standard", "0,nan,10,0,5,2,0,Standard",
                            "0,0,1e22,0,5,2,0,Standard", "0,0,10,0,5,2,-1e300,Standard"})
        assert(!linear->deserialize(bad));

    // --- Hauteur de texte absurde : bornee, l'emprise reste finie ---
    geom::AngularDimensionEntity angular({0, 0}, {5, 0}, {0, 5}, {3, 4});
    angular.properties().setDouble(geom::kDimensionTextHeightProperty, 1e22);
    assert(geom::dimensionTextHeight(angular) == geom::kMaxDimensionTextHeight);
    angular.properties().setDouble(geom::kDimensionTextHeightProperty, -3.0);
    assert(geom::dimensionTextHeight(angular) == geom::kDefaultDimensionTextHeight);

    // --- Coordonnee non finie (entite construite en memoire) : rien a dessiner ---
    geom::AngularDimensionEntity broken({0, 0}, {NAN, 0}, {0, 5}, {3, 4});
    assert(geom::dimensionGraphics(broken).path.empty());

    std::printf("Cotations enregistrees : tests PASSED\n");
    return 0;
}
