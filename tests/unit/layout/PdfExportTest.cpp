#include "bcad/layout/PdfExport.h"
#include "bcad/properties/PropertyTypes.h"
#include <cassert>
#include <filesystem>

#include <QGuiApplication>

using namespace bcad::layout;

int main(int argc, char** argv) {
    // exportPdf() construit un QPrinter et dessine le cartouche avec des QFont :
    // sans QGuiApplication, Qt aborte le processus avant toute verification.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    // Chemin vide → échec
    PdfExportOptions opts;
    std::string err;
    assert(!exportPdf(opts, &err));
    assert(!err.empty());

    // Export minimal A4, avec un meuble resolu : l'hote peint des libelles
    // qu'il ne comprend pas.
    FurnitureTemplate gabarit;
    gabarit.columns = 2;
    gabarit.reservedZone = {0, 0, 0, 25.0};
    Field champ;
    champ.label = "Projet";
    champ.key = "dossier.projet";
    champ.slot = 0;
    champ.value.type = bcad::properties::PropertyType::String;
    champ.value.value = std::string("Test");
    ResolvedFurniture meuble{gabarit, {champ}};
    opts.outputPath = "/tmp/bcad_test_export.pdf";
    opts.sheet = Sheet(PaperFormat::A4, Orientation::Portrait);
    opts.meubles.push_back(meuble);
    assert(exportPdf(opts, &err));
    assert(std::filesystem::exists(opts.outputPath));
    assert(std::filesystem::file_size(opts.outputPath) > 0);
    std::filesystem::remove(opts.outputPath);

    // Marges invalides
    opts.sheet.setMargins({200, 200, 200, 200});
    assert(!exportPdf(opts, &err));

    return 0;
}
