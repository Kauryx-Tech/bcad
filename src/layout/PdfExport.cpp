#include "bcad/layout/PdfExport.h"
#include <QPainter>
#include <QPrinter>
#include <QPageLayout>
#include <QPageSize>

namespace bcad::layout {

bool exportPdf(const PdfExportOptions& opts, std::string* error) {
    if (opts.outputPath.empty()) {
        if (error) *error = "chemin vide";
        return false;
    }
    if (!opts.sheet.isValid()) {
        if (error) *error = "feuille invalide (marges trop grandes)";
        return false;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(QString::fromStdString(opts.outputPath));

    // Format de page
    QPageSize::Unit unit = QPageSize::Millimeter;
    QPageSize pageSize;
    switch (opts.sheet.format()) {
        case PaperFormat::A4: pageSize = QPageSize(QPageSize::A4); break;
        case PaperFormat::A3: pageSize = QPageSize(QPageSize::A3); break;
        case PaperFormat::A2: pageSize = QPageSize(QPageSize::A2); break;
        case PaperFormat::A1: pageSize = QPageSize(QPageSize::A1); break;
        case PaperFormat::A0: pageSize = QPageSize(QPageSize::A0); break;
    }
    QPageLayout layout(pageSize,
        opts.sheet.orientation() == Orientation::Portrait ? QPageLayout::Portrait : QPageLayout::Landscape,
        QMarginsF(opts.sheet.margins().left, opts.sheet.margins().top,
                  opts.sheet.margins().right, opts.sheet.margins().bottom),
        QPageLayout::Millimeter);
    printer.setPageLayout(layout);

    QPainter painter;
    if (!painter.begin(&printer)) {
        if (error) *error = "impossible d'ouvrir le PDF";
        return false;
    }

    // Cartouche en bas
    if (opts.cartouche.isValid()) {
        QRectF pageRect = printer.pageRect(QPrinter::Millimeter);
        QRectF cartoucheRect(0, pageRect.height() - opts.cartouche.heightMm,
                             pageRect.width(), opts.cartouche.heightMm);
        painter.drawRect(cartoucheRect);
        painter.drawText(cartoucheRect.adjusted(2, 2, -2, -2),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromStdString(opts.cartouche.title()));
    }

    painter.end();
    return true;
}

} // namespace bcad::layout
