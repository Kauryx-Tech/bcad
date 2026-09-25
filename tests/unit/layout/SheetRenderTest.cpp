// Rendu de la feuille verifie a l'encre : un peintre qui se trompe de repere
// produit un timbre-poste dans un coin ou une page rognee, et aucune verification
// numerique de composition ne le voit. Le test dessine sur une QImage de la
// taille exacte de la zone imprimable et mesure ou tombe l'encre.
//
// Les appels a effet de bord sont hors des assert() : assert() n'evalue pas son
// argument quand NDEBUG est defini.

#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/PdfExport.h"

#include <QGuiApplication>
#include <QImage>
#include <QMargins>
#include <QPageLayout>
#include <QPageSize>
#include <QPen>
#include <QPainter>
#include <QPrinter>
#include <QRect>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

using namespace bcad;

namespace {

QRect inkExtents(const QImage& image) {
    int minX = std::numeric_limits<int>::max(), minY = minX;
    int maxX = -1, maxY = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qGray(line[x]) > 200) continue;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX < 0) return QRect();
    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

} // namespace

int main(int argc, char** argv) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    // La precondition du repere millimetre est mesuree, pas supposee : le
    // peripherique d'un QPrinter couvre la zone imprimable, marges deductives.
    {
        const auto path = (std::filesystem::temp_directory_path() /
                           "bcad_sheet_render_premisse.pdf").string();
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(QString::fromStdString(path));
        printer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                                          QMarginsF(10, 10, 10, 10), QPageLayout::Millimeter));
        const QRect paint = printer.pageRect(QPrinter::DevicePixel).toRect();
        assert(printer.width() == paint.width());
        assert(printer.height() == paint.height());
        // 190 mm de large sur la zone peignable, 210 sur le papier : la marge
        // n'est pas peignable, et c'est ce qui rend la composition juste.
        const double paintMm = printer.pageRect(QPrinter::Millimeter).width();
        assert(paintMm > 185.0 && paintMm < 195.0);
        assert(printer.paperRect(QPrinter::Millimeter).width() > 205.0);
        std::filesystem::remove(path);
    }

    core::Document document;
    document.addEntity(std::make_unique<geom::PolylineEntity>(
        std::vector<geom::Point2>{{0, 0}, {100, 0}, {100, 60}, {0, 60}}, true));

    layout::PdfExportOptions options;
    options.sheet = layout::Sheet(layout::PaperFormat::A4, layout::Orientation::Portrait);
    options.viewport.setSource(document.extents());
    options.document = &document;
    layout::applySuggestedScale(options);
    assert(options.cartouche.echelle == "1:1000");

    constexpr double kDpi = 96.0;
    QImage image(static_cast<int>(options.sheet.printableWidth() * kDpi / 25.4),
                 static_cast<int>(options.sheet.printableHeight() * kDpi / 25.4),
                 QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        assert(painter.isActive());
        layout::drawSheet(painter, options);
    }

    const QRect ink = inkExtents(image);
    assert(!ink.isNull());
    // Le cadre epouse la zone imprimable : c'est la mesure qui distingue une
    // feuille a l'echelle d'un dessin 47 fois trop petit dans un coin.
    assert(ink.width() > static_cast<int>(image.width() * 0.95));
    assert(ink.height() > static_cast<int>(image.height() * 0.95));
    assert(ink.left() >= 0 && ink.top() >= 0);
    assert(ink.right() < image.width() && ink.bottom() < image.height());

    // Avec un cartouche, la bande du bas est occupee et le plan ne descend pas
    // dedans : a 1:1000, 60 m de terrain font 60 mm, l'empreinte doit s'arreter
    // au-dessus des 25 mm du cartouche une fois le cadre et le cartouche retires.
    options.cartouche.commune = "Test";
    options.cartouche.section = "A";
    layout::applySuggestedScale(options);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        layout::drawSheet(painter, options);
    }
    const double scale = options.viewport.scale();
    const auto composed = layout::composeSheet(options.sheet, options.viewport,
                                              options.cartouche);
    assert(std::abs(composed.mapping.rect.h - 60000.0 / scale) < 1e-6);
    assert(composed.drawing.bottom() <= composed.cartouche.top() + 1e-9);
    // Le cartouche est bien encre, dans sa bande.
    const QRect inked = inkExtents(image);
    const int bandTop = static_cast<int>(composed.cartouche.y * kDpi / 25.4);
    const QRect band = inked.intersected(QRect(0, bandTop, image.width(), image.height() - bandTop));
    assert(band.width() > static_cast<int>(image.width() * 0.9));
    // Le plan (60 mm de haut) reste au-dessus du cartouche : entre les deux, la
    // seule encre attendue est le bord droit/gauche du cadre et la barre
    // d'echelle, jamais une ligne horizontale pleine.
    const int planBottom = static_cast<int>((composed.mapping.rect.bottom()) * kDpi / 25.4);
    const int gapRow = (planBottom + bandTop) / 2;
    int fullRow = 0;
    const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(gapRow));
    for (int x = 1; x < image.width() - 1; ++x)
        if (qGray(line[x]) <= 200) ++fullRow;
    assert(fullRow < image.width() / 2);

    std::cout << "rendu de feuille OK\n";
    return 0;
}
