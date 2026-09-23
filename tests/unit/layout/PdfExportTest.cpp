#include "bcad/layout/PdfExport.h"
#include <cassert>
#include <filesystem>

using namespace bcad::layout;

int main() {
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
