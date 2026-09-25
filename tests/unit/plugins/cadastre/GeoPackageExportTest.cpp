// L'exporteur GeoPackage du module, vu comme le voit l'hote : par le contrat
// `IFileExporter`. Ce que `cadastre_io_test` ne couvre pas — il appelle le
// serializer directement, alors que le chemin utilisateur passe par
// `writeDocument()` et par le libelle/extension declares par le module.
//
// Sans ce test, `GeoPackageExporter` serait a nouveau du code compile sans
// appelant : la presence du fichier .gpkg sur le disque ne prouve pas qu'il est
// atteignable depuis l'application.

#include "io/GeoPackageExporter.h"
#include "io/GeoPackageSerializer.h"
#include "entities/ParcelEntity.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Point.h"
#include "bcad/plugin/FileExporter.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

// `Document` porte un shared_mutex : il n'est ni copiable ni deplacable, donc
// le lot d'entites est construit dans le document du demandeur.
void addParcels(bcad::core::Document& document) {
    const std::vector<std::pair<std::string, std::string>> marks{{"A", "7"}, {"B", "12"}};
    for (std::size_t i = 0; i < marks.size(); ++i) {
        const double offset = static_cast<double>(i) * 30.0;
        auto parcel = std::make_unique<bcad::cadastre::ParcelEntity>(
            std::vector<bcad::geom::Point2>{{offset, 0}, {offset + 20, 0},
                                            {offset + 20, 10}, {offset, 10}});
        parcel->properties().setString("cadastre.section", marks[i].first);
        parcel->properties().setString("cadastre.numero", marks[i].second);
        document.addEntity(std::move(parcel));
    }
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    using bcad::cadastre::GeoPackageExporter;
    using bcad::cadastre::ParcelEntity;

    // --- Le contrat generique, tel que l'hote le lit ---
    const GeoPackageExporter exporter;
    assert(exporter.id() == "cadastre.geopackage");
    // Libelle et extension viennent du declarant : l'hote les affiche tels quels
    // dans le menu et dans le filtre de la boite d'enregistrement.
    assert(!exporter.label().empty());
    assert(exporter.extension() == "gpkg");
    // L'hote ne prefixe jamais l'extension : c'est le declarant qui la fournit.
    assert(exporter.extension().front() != '.');

    const auto path = (fs::temp_directory_path() / "bcad-cadastre-export-test.gpkg").string();
    fs::remove(path);

    // --- Export du document, puis relecture de ce qui a ete ecrit ---
    bcad::core::Document document;
    addParcels(document);
    std::string error;
    assert(exporter.writeDocument(document, path, &error));
    assert(error.empty());
    assert(fs::exists(path));
    // L'export ne modifie pas le document emis.
    assert(document.entities().size() == 2);

    bcad::core::Document restored;
    assert(bcad::cadastre::GeoPackageSerializer{}.read(path, restored));
    assert(restored.entities().size() == 2);
    // Les attributs cadastraux ont traverse le fichier : c'est ce que l'agent
    // foncier vient checker, pas la seule geometrie.
    std::vector<std::string> sections, numeros;
    for (const auto& entity : restored.entities()) {
        const auto* parcel = dynamic_cast<const ParcelEntity*>(entity.get());
        assert(parcel);
        sections.push_back(parcel->properties().getString("cadastre.section"));
        numeros.push_back(parcel->properties().getString("cadastre.numero"));
        assert(parcel->vertices().size() == 4);
    }
    assert(sections == std::vector<std::string>({"A", "B"}));
    assert(numeros == std::vector<std::string>({"7", "12"}));
    fs::remove(path);

    // --- Echec : un message, jamais un succes sans fichier ---
    error.clear();
    assert(!exporter.writeDocument(document, path + ".absent/introuvable.gpkg", &error));
    assert(!error.empty());
    // `error` peut etre nul cote appelant : l'exporteur doit survivre a l'appel.
    assert(!exporter.writeDocument(document, path + ".absent/introuvable.gpkg", nullptr));
    assert(!fs::exists(path));

    std::printf("GeoPackage exportable : tests PASSED\n");
    return 0;
}
