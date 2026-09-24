#include "bcad/layout/PdfExport.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/TextEntity.h"
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPrinter>
#include <QPageLayout>
#include <QPageSize>
#include <QRectF>
#include <QString>
#include <algorithm>
#include <utility>
#include <vector>

namespace bcad::layout {

namespace {

// Paires label → valeur à afficher dans le cartouche (ordre d'impression).
std::vector<std::pair<QString, QString>> cartoucheCells(const Cartouche& c) {
    std::vector<std::pair<QString, QString>> cells;
    auto add = [&cells](const char* label, const std::string& value) {
        if (!value.empty()) {
            cells.emplace_back(QString::fromUtf8(label), QString::fromStdString(value));
        }
    };
    add("Projet", c.projectName);
    add("N° projet", c.projectNumber);
    add("Phase", c.phase);
    add("Lot", c.lotNumber);
    add("Commune", c.commune);
    add("Section", c.section);
    add("N° parcelle", c.numero);
    add("Contenance", c.contenance);
    add("Échelle", c.echelle);
    add("Date", c.date);
    add("Géomètre", c.geometre);
    add("Dossier", c.dossier);
    add("Propriétaire", c.proprietaire);
    add("Nature", c.nature);
    add("Réf. plan", c.referencePlan);
    add("Révision", c.revision);
    add("Auteur", c.auteur);
    return cells;
}

} // namespace

void drawCartouche(QPainter& painter, const QRectF& rect, const Cartouche& cartouche) {
    if (rect.width() <= 0 || rect.height() <= 0) return;

    painter.save();
    painter.setPen(QPen(QColor(0, 0, 0), cartouche.borderWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);

    const auto cells = cartoucheCells(cartouche);
    if (cells.empty()) {
        // Repli : titre seul (comportement antérieur).
        painter.setPen(QColor(0, 0, 0));
        painter.setFont(QFont(QString::fromStdString(cartouche.fontName),
                              qRound(cartouche.fontSizeMm * 2.8346))); // mm → pt approx
        painter.drawText(rect.adjusted(2, 2, -2, -2),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromStdString(cartouche.title()));
        painter.restore();
        return;
    }

    // Grille : 2 lignes × N colonnes, remplie cellule par cellule.
    constexpr int kCols = 4;
    const int rows = (static_cast<int>(cells.size()) + kCols - 1) / kCols;
    const double cellW = rect.width() / kCols;
    const double cellH = rect.height() / std::max(rows, 1);

    QFont labelFont(QString::fromStdString(cartouche.fontName),
                    qMax(6, qRound(cartouche.fontSizeMm * 2.8346 * 0.8)));
    labelFont.setBold(true);
    QFont valueFont(QString::fromStdString(cartouche.fontName),
                    qMax(6, qRound(cartouche.fontSizeMm * 2.8346)));
    valueFont.setBold(false);

    painter.setPen(QPen(QColor(0, 0, 0), cartouche.borderWidth * 0.75));

    for (std::size_t i = 0; i < cells.size(); ++i) {
        const int row = static_cast<int>(i) / kCols;
        const int col = static_cast<int>(i) % kCols;
        const QRectF cell(rect.left() + col * cellW,
                          rect.top() + row * cellH,
                          cellW, cellH);
        painter.drawRect(cell);

        const QRectF content = cell.adjusted(1.5, 1.0, -1.5, -1.0);
        painter.setFont(labelFont);
        painter.drawText(content, Qt::AlignLeft | Qt::AlignTop,
                         cells[i].first + QLatin1Char(':'));
        painter.setFont(valueFont);
        painter.drawText(content, Qt::AlignLeft | Qt::AlignBottom,
                         cells[i].second);
    }

    painter.restore();
}

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

    const QRectF pageRect = printer.pageRect(QPrinter::Millimeter);
    if (opts.document) {
        const auto bbox = opts.document->extents();
        if (bbox.isValid()) {
            const double scale = opts.viewport.scale();
            const double bottom = opts.cartouche.isValid()
                ? pageRect.height() - opts.cartouche.heightMm : pageRect.height();
            painter.save();
            painter.translate(opts.sheet.margins().left, bottom);
            painter.scale(1000.0 / scale, -1000.0 / scale);
            painter.translate(-opts.viewport.source().minX, -opts.viewport.source().maxY);
            for (const auto& entity : opts.document->entities()) {
                if (const auto* text = dynamic_cast<const geom::TextEntity*>(entity.get())) {
                    painter.save();
                    painter.setPen(Qt::black);
                    painter.setFont(QFont(QStringLiteral("Sans"),
                                          std::max(1, qRound(text->height() * 2.8346))));
                    painter.drawText(QPointF(text->position().x_, text->position().y_),
                                     QString::fromStdString(text->text()));
                    painter.restore();
                    continue;
                }
                const auto points = entity->tessellate(0.01);
                if (points.size() < 2) continue;
                QPolygonF polygon;
                for (const auto& point : points) polygon << QPointF(point.x_, point.y_);
                painter.setPen(QPen(Qt::black, 0));
                painter.setBrush(Qt::NoBrush);
                if (points.size() >= 3 && entity->typeId() != geom::TypeId_Line &&
                    entity->typeId() != geom::TypeId_Point)
                    painter.drawPolygon(polygon);
                else
                    painter.drawPolyline(polygon);
            }
            painter.restore();
        }
    }

    // Cartouche en bas
    if (opts.cartouche.isValid()) {
        QRectF cartoucheRect(0, pageRect.height() - opts.cartouche.heightMm,
                             pageRect.width(), opts.cartouche.heightMm);
        drawCartouche(painter, cartoucheRect, opts.cartouche);
    }

    painter.end();
    return true;
}

} // namespace bcad::layout
