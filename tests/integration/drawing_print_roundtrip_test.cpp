#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/io/Database.h"
#include "bcad/layout/PdfExport.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include <QGuiApplication>

using namespace bcad;

int main(int argc, char** argv) {
    // exportPdf() construit un QPrinter et dessine le cartouche avec des QFont :
    // sans QGuiApplication, Qt aborte le processus avant toute verification.
    // Plateforme 'offscreen' : le test n'ouvre aucune fenetre.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    const auto root = std::filesystem::temp_directory_path();
    const auto projectPath = root / "bcad_drawing_print_roundtrip.bcad";
    const auto pdfPath = root / "bcad_drawing_print_roundtrip.pdf";
    std::filesystem::remove(projectPath);
    std::filesystem::remove(pdfPath);

    core::Document document;
    document.layerManager().createLayer("Dimensions", geom::Color::fromRgb255(0, 180, 0));
    document.addEntity(std::make_unique<geom::LineEntity>(
        geom::Point2(0, 0), geom::Point2(10, 5)));
    document.addEntity(std::make_unique<geom::PolylineEntity>(
        std::vector<geom::Point2>{{0, 0}, {10, 0}, {10, 5}, {0, 5}}, true));
    auto dimension = std::make_unique<geom::LineEntity>(
        geom::Point2(0, -1), geom::Point2(10, -1));
    dimension->setLayer("Dimensions");
    document.addEntity(std::move(dimension));

    assert(io::Database::save(projectPath.string(), document));
    core::Document loaded;
    assert(io::Database::load(projectPath.string(), loaded));
    assert(loaded.entities().size() == 3);

    layout::PdfExportOptions options;
    options.outputPath = pdfPath.string();
    options.sheet = layout::Sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
    options.viewport.setSource(loaded.extents());
    options.document = &loaded;
    // Sans module, aucun meuble : l'echelle admise est choisie dans la liste
    // donnee ici, sans rien fixer a la main et sans vocabulaire d'hote.
    options.permittedScales = {500, 1000, 2000};
    layout::applyFittingScale(options);
    assert(options.viewport.scale() == 500);
    assert(options.viewport.fitsIn(options.sheet));

    std::string error;
    assert(layout::exportPdf(options, &error));
    assert(error.empty());
    assert(std::filesystem::exists(pdfPath));
    assert(std::filesystem::file_size(pdfPath) > 1000);

    std::ifstream pdf(pdfPath, std::ios::binary);
    std::string header(5, '\0');
    pdf.read(header.data(), static_cast<std::streamsize>(header.size()));
    assert(header == "%PDF-");

    std::filesystem::remove(projectPath);
    std::filesystem::remove(pdfPath);
    std::cout << "drawing -> save -> load -> print PDF OK\n";
    return 0;
}
