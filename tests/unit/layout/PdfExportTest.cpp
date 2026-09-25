#include "bcad/layout/PdfExport.h"
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

    // Export minimal A4
    opts.outputPath = "/tmp/bcad_test_export.pdf";
    opts.sheet = Sheet(PaperFormat::A4, Orientation::Portrait);
    opts.cartouche.commune = "Test";
    opts.cartouche.section = "A";
    assert(exportPdf(opts, &err));
    assert(std::filesystem::exists(opts.outputPath));
    assert(std::filesystem::file_size(opts.outputPath) > 0);
    std::filesystem::remove(opts.outputPath);

    // Marges invalides
    opts.sheet.setMargins({200, 200, 200, 200});
    assert(!exportPdf(opts, &err));

    return 0;
}
