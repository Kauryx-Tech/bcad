#include "bcad/layout/PdfExport.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/layout/FurniturePaint.h"
#include "bcad/layout/NorthArrow.h"
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QPrinter>
#include <QPageLayout>
#include <QPageSize>
#include <QRectF>
#include <QString>
#include <QtMath>
#include <algorithm>
#include <utility>
#include <vector>

namespace bcad::layout {

namespace {

QRectF toRect(const RectMm& rect) {
    return QRectF(rect.x, rect.y, rect.w, rect.h);
}

// Famille par defaut du mobilier de feuille. Les meubles portent la leur,
// depuis leur gabarit.
const QString kSheetFamily = QStringLiteral("Sans");

// Variante au point d'ancrage : centre verticalement sur `at`, le texte part a
// droite. Une largeur de 300 mm couvre n'importe quelle etiquette de plan.
void drawTextAt(QPainter& painter, const QPointF& at, const QString& text,
                const QString& family, double heightMm, bool bold = false) {
    drawTextMm(painter, QRectF(at.x(), at.y() - heightMm, 300.0, heightMm * 2.0),
               Qt::AlignLeft | Qt::AlignVCenter, text, family, heightMm, bold);
}

// Hauteur que le bandeau bas réserve : la somme des hauteurs que les gabarits
// des meubles déclarent. L'hôte réserve, le module déclare : ni l'un ni
// l'autre ne devine.
double bottomBandOf(const std::vector<ResolvedFurniture>& meubles) {
    double total = 0.0;
    for (const auto& meuble : meubles) total += std::max(0.0, meuble.gabarit.reservedZone.h);
    return total;
}

double rightColumnOf(const std::vector<ResolvedFurniture>& tables) {
    double width = 0.0;
    for (const auto& table : tables) width = std::max(width, table.gabarit.reservedZone.w);
    return width;
}

// Ramène le repère du peintre au millimètre de feuille, origine au coin
// haut-gauche de la zone imprimable. Le repère natif d'un QPrinter est le pixel
// de l'imprimante (1200 dpi : 1 unité = 0,021 mm) et son périphérique couvre la
// zone imprimable, marges déduites — sans cette mise à l'échelle, toute la
// feuille est dessinée ~47 fois trop petite dans un coin.
void useMillimetrePage(QPainter& painter, const Sheet& sheet) {
    const auto device = painter.device();
    if (!device || device->width() <= 0 || device->height() <= 0) return;
    painter.scale(device->width() / sheet.printableWidth(),
                  device->height() / sheet.printableHeight());
}

// Le plan, dans le repère monde : une seule transformation pour toutes les
// entités. Les textes en sont exclus, ils seraient miroirs ici.
void drawPlan(QPainter& painter, const core::Document& document,
              const SheetComposition& composition) {
    const auto& mapping = composition.mapping;
    painter.save();
    painter.translate(mapping.rect.x, mapping.rect.y);
    painter.scale(1000.0 / mapping.scale, -1000.0 / mapping.scale);
    painter.translate(-mapping.origin.x_, -mapping.origin.y_);
    painter.setBrush(Qt::NoBrush);
    for (const auto& entity : document.entities()) {
        if (entity->typeId() == geom::TypeId_Text) continue;
        const auto points = entity->tessellate(0.01);
        if (points.size() < 2) continue;
        QPen pen(Qt::black);
        pen.setWidthF(0);  // épaisseur cosmétique : 1 pixel de sortie
        if (const auto color = entity->colorOverride())
            pen.setColor(QColor::fromRgbF(color->r, color->g, color->b));
        painter.setPen(pen);
        QPolygonF polygon;
        for (const auto& point : points) polygon << QPointF(point.x_, point.y_);
        if (points.size() >= 3 && entity->typeId() != geom::TypeId_Line &&
            entity->typeId() != geom::TypeId_Point)
            painter.drawPolygon(polygon);
        else
            painter.drawPolyline(polygon);
    }
    painter.restore();
}

// Textes du document, étiquettes et repères : repère feuille, pour que les
// glyphes ne soient pas retournés par l'inversion Y du plan.
void drawDocumentTexts(QPainter& painter, const core::Document& document,
                       const PageMapping& mapping) {
    for (const auto& entity : document.entities()) {
        const auto* text = dynamic_cast<const geom::TextEntity*>(entity.get());
        if (!text) continue;
        const auto at = mapping.toPage(text->position());
        const double sizeMm = std::max(0.8, text->height() * 1000.0 / mapping.scale);
        painter.save();
        painter.setPen(Qt::black);
        drawTextAt(painter, QPointF(at.x(), at.y()),
                   QString::fromStdString(text->text()), kSheetFamily, sizeMm);
        painter.restore();
    }
}

void drawLabels(QPainter& painter, const std::vector<Label>& labels,
                const PageMapping& mapping) {
    for (const auto& label : labels) {
        const auto at = mapping.toPage(label.position);
        painter.save();
        painter.setPen(Qt::black);
        const double height = std::max(1.0, label.fontSizeMm);
        const QRectF box(at.x() - 20.0, at.y() - height * 2.0, 40.0, height * 4.0);
        drawTextMm(painter, box, Qt::AlignCenter, QString::fromStdString(label.text),
                   kSheetFamily, height);
        painter.restore();
    }
}

void drawMarkers(QPainter& painter, const std::vector<PointMarker>& markers,
                 const PageMapping& mapping) {
    if (markers.empty()) return;
    painter.save();
    painter.setPen(QPen(Qt::black, 0));
    painter.setBrush(Qt::NoBrush);
    for (const auto& marker : markers) {
        const auto at = mapping.toPage(marker.position);
        const double radius = std::max(0.4, marker.radiusMm);
        painter.drawEllipse(QPointF(at.x(), at.y()), radius, radius);
        drawTextAt(painter, QPointF(at.x() + radius + 0.4, at.y() - radius),
                   QString::fromStdString(marker.text), kSheetFamily, 1.6);
    }
    painter.restore();
}

void drawNorthArrow(QPainter& painter, const RectMm& rect, double angleDeg) {
    if (!rect.isValid()) return;
    painter.save();
    NorthArrow arrow;
    arrow.sizeMm = rect.w * 0.8;
    arrow.angleDeg = angleDeg;
    arrow.position = {rect.x + rect.w * 0.5, rect.y + rect.h * 0.62};
    QPolygonF triangle;
    for (const auto& point : arrow.triangle()) triangle << QPointF(point.x_, point.y_);
    painter.setPen(QPen(Qt::black, 0));
    painter.setBrush(Qt::black);
    painter.drawPolygon(triangle);
    drawTextMm(painter, QRectF(rect.x, rect.y, rect.w, rect.h * 0.35),
               Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("N"),
               kSheetFamily, 2.5, true);
    painter.restore();
}

void drawScaleBar(QPainter& painter, const RectMm& rect, double scale) {
    const auto bar = makeScaleBar(scale, rect.w);
    if (bar.lengthMm <= 0.0) return;
    painter.save();
    painter.setPen(QPen(Qt::black, 0));
    const double baseline = rect.y + rect.h;
    const double segment = bar.lengthMm / bar.segments;
    for (int i = 0; i < bar.segments; ++i) {
        const QRectF cell(rect.x + i * segment, baseline - 1.4, segment, 1.4);
        if (i % 2 == 0) painter.setBrush(Qt::black);
        else painter.setBrush(Qt::NoBrush);
        painter.drawRect(cell);
    }
    painter.setBrush(Qt::NoBrush);
    // Les deux marques aux deux bouts de la place reservee, pas de la barre : a
    // 40 mm de barre sur 60 mm de bande, se toucher serait illisible.
    const QRectF marks(rect.x, baseline - 5.4, std::max(rect.w, bar.lengthMm), 3.6);
    drawTextMm(painter, QRectF(marks.x(), marks.y(), 12.0, marks.height()),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0"), kSheetFamily, 2.0);
    drawTextMm(painter, QRectF(marks.right() - 30.0, marks.y(), 30.0, marks.height()),
               Qt::AlignRight | Qt::AlignVCenter,
               QString::fromStdString(bar.label()), kSheetFamily, 2.0);
    painter.restore();
}

void drawFrame(QPainter& painter, const RectMm& rect) {
    painter.save();
    const qreal penWidth = 0.35;
    painter.setPen(QPen(Qt::black, penWidth));
    painter.setBrush(Qt::NoBrush);
    // Le cadre est exactement a bord de la zone imprimable : sans demi-epaisseur
    // de reprise, le trait serait rogne de moitie sur le papier.
    painter.drawRect(QRectF(rect.x + penWidth / 2, rect.y + penWidth / 2,
                            rect.w - penWidth, rect.h - penWidth));
    painter.restore();
}

} // namespace

void drawSheet(QPainter& painter, const PdfExportOptions& opts) {
    const auto composition = composeSheet(opts.sheet, opts.viewport, opts.permittedScales,
                                          bottomBandOf(opts.meubles), rightColumnOf(opts.tables));

    painter.save();
    useMillimetrePage(painter, opts.sheet);

    if (opts.document) {
        drawPlan(painter, *opts.document, composition);
        drawDocumentTexts(painter, *opts.document, composition.mapping);
    }
    if (opts.showLabels) drawLabels(painter, opts.labels, composition.mapping);
    drawMarkers(painter, opts.markers, composition.mapping);
    if (opts.showNorthArrow)
        drawNorthArrow(painter, composition.northArrow, opts.northArrowAngleDeg);
    if (opts.showScaleBar)
        drawScaleBar(painter, composition.scaleBar, composition.mapping.scale);

    // Bandeau bas : empilés depuis le bas, chacun sur la hauteur que son
    // gabarit déclare — jamais plus que la bande réservée.
    double bottom = composition.cartouche.bottom();
    for (const auto& meuble : opts.meubles) {
        const double height =
            std::min(std::max(0.0, meuble.gabarit.reservedZone.h), bottom - composition.cartouche.y);
        if (height <= 0.0) break;
        bottom -= height;
        drawFurniture(painter,
                      QRectF(composition.cartouche.x, bottom, composition.cartouche.w, height),
                      meuble);
    }

    for (const auto& table : opts.tables) {
        if (composition.parcelTable.isValid())
            drawFurnitureTable(painter, toRect(composition.parcelTable), table);
    }

    drawFrame(painter, composition.printable);

    painter.restore();
}

void applyPageLayout(QPrinter* printer, const Sheet& sheet) {
    if (!printer) return;
    QPageSize pageSize;
    switch (sheet.format()) {
        case PaperFormat::A4: pageSize = QPageSize(QPageSize::A4); break;
        case PaperFormat::A3: pageSize = QPageSize(QPageSize::A3); break;
        case PaperFormat::A2: pageSize = QPageSize(QPageSize::A2); break;
        case PaperFormat::A1: pageSize = QPageSize(QPageSize::A1); break;
        case PaperFormat::A0: pageSize = QPageSize(QPageSize::A0); break;
    }
    printer->setPageLayout(QPageLayout(pageSize,
        sheet.orientation() == Orientation::Portrait ? QPageLayout::Portrait
                                                     : QPageLayout::Landscape,
        QMarginsF(sheet.margins().left, sheet.margins().top,
                  sheet.margins().right, sheet.margins().bottom),
        QPageLayout::Millimeter));
}

void applyFittingScale(PdfExportOptions& opts) {
    const auto composition = composeSheet(opts.sheet, opts.viewport, opts.permittedScales,
                                          bottomBandOf(opts.meubles), rightColumnOf(opts.tables));
    opts.viewport.setScale(composition.suggestedScale);
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
    applyPageLayout(&printer, opts.sheet);

    QPainter painter;
    if (!painter.begin(&printer)) {
        if (error) *error = "impossible d'ouvrir le PDF";
        return false;
    }
    drawSheet(painter, opts);
    painter.end();
    return true;
}

} // namespace bcad::layout
