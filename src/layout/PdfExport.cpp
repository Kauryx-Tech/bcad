#include "bcad/layout/PdfExport.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/TextEntity.h"
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

// Le peintre est au millimetre de feuille, la taille d'une police doit donc etre
// donnee en millimetres. QFont::setPixelSize est une unite utilisateur — donc un
// millimetre — et rend a la meme taille quelque soit la resolution du
// peripherique ; setPointSize, lui, est multiplie par le dpi logique de
// l'imprimante (1200 dpi sur un QPrinter haute resolution : des glyphes seize
// fois trop grands, vérifié). Le pixelSize etant entier, on dessine les textes
// dans un repere intermediaire au 1/16 mm pour garder les tailles fractionnaires.
constexpr double kFontSubdivision = 16.0;

// Famille par defaut du mobilier de feuille : le cartouche, lui, porte la sienne.
const QString kSheetFamily = QStringLiteral("Sans");

void drawTextMm(QPainter& painter, const QRectF& rect, int flags, const QString& text,
                const QString& family, double heightMm, bool bold = false) {
    if (text.isEmpty() || heightMm <= 0.0 || rect.width() <= 0.0 || rect.height() <= 0.0)
        return;
    painter.save();
    painter.translate(rect.topLeft());
    painter.scale(1.0 / kFontSubdivision, 1.0 / kFontSubdivision);
    QFont font(family);
    font.setPixelSize(std::max(1, qRound(heightMm * kFontSubdivision)));
    font.setBold(bold);
    painter.setFont(font);
    painter.drawText(QRectF(0, 0, rect.width() * kFontSubdivision,
                            rect.height() * kFontSubdivision), flags, text);
    painter.restore();
}

// Variante au point d'ancrage : centre verticalement sur `at`, le texte part a
// droite. Une largeur de 300 mm couvre n'importe quelle etiquette de plan.
void drawTextMm(QPainter& painter, const QPointF& at, const QString& text,
                const QString& family, double heightMm, bool bold = false) {
    drawTextMm(painter, QRectF(at.x(), at.y() - heightMm, 300.0, heightMm * 2.0),
               Qt::AlignLeft | Qt::AlignVCenter, text, family, heightMm, bold);
}

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
    add("Code commune", c.communeCode);
    add("Échelle", c.echelle);
    add("Date", c.date);
    add("Géomètre", c.geometre);
    add("Dossier", c.dossier);
    add("Propriétaire", c.proprietaire);
    add("Nature", c.nature);
    add("Réf. plan", c.referencePlan);
    add("Révision", c.revision);
    add("Auteur", c.auteur);
    add("Vérifié par", c.verifiePar);
    add("Approuvé par", c.approuvePar);
    add("Créé le", c.dateCreation);
    add("Modifié le", c.dateModification);
    return cells;
}

} // namespace

// Dessine le tableau des signatures sous le cartouche principal
void drawSignaturesTable(QPainter& painter, const QRectF& rect, const Cartouche& cartouche, double startY) {
    if (cartouche.signatures.empty()) return;

    const QString family = QString::fromStdString(cartouche.fontName);
    const double fontSize = cartouche.fontSizeMm;
    const double rowHeight = std::max(5.0, cartouche.fontSizeMm * 2.5);

    painter.setPen(QPen(QColor(0, 0, 0), cartouche.borderWidth * 0.75));

    // En-têtes
    constexpr int kCols = 4;

    // Dessiner l'en-tête
    QStringList headers = {"Nom", "Rôle", "Date", "Signature"};
    painter.setFont(QFont(QString::fromStdString("Standard"), 2.0, QFont::Bold));
    for (int col = 0; col < 4; ++col) {
        double x = col * (rect.width() / 4.0);
        drawTextMm(painter, QRectF(col * 25.0, startY, 25.0, 5.0),
                   Qt::AlignCenter | Qt::AlignVCenter,
                   QStringList{"Nom", "Rôle", "Date", "Signature"}[col], 
                   "Standard", 2.0, true);
    }
    
    // Lignes de signatures
    double rowY = startY + 5.0;
    for (std::size_t i = 0; i < cartouche.signatures.size(); ++i) {
        const auto& sig = cartouche.signatures[i];
        if (i > 0) {
            painter.drawLine(QPointF(0, rowY), QPointF(100.0, rowY));
        }
        
        drawTextMm(painter, QRectF(0, rowY, 25.0, 5.0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromStdString(cartouche.signatures[i].nom),
                   "Standard", 1.8);
        drawTextMm(painter, QRectF(25.0, rowY, 25.0, 5.0),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromStdString(cartouche.signatures[i].role),
                   "Standard", 1.8);
        drawTextMm(painter, QRectF(50.0, rowY, 25.0, 5.0),
                   Qt::AlignCenter | Qt::AlignVCenter,
                   QString::fromStdString(cartouche.signatures[i].date),
                   "Standard", 1.8);
        // Signature image placeholder
        if (!cartouche.signatures[i].signaturePath.empty()) {
            // TODO: charger et dessiner l'image
        } else {
            // Ligne de signature vide
            painter.drawLine(QPointF(75.0, rowY + 2.5), QPointF(95.0, rowY + 2.5));
        }
        
        rowY += 5.0;
    }
}

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
        drawTextMm(painter, rect.adjusted(2, 2, -2, -2),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromStdString(cartouche.title()),
                   QString::fromStdString(cartouche.fontName), cartouche.fontSizeMm);
        painter.restore();
        return;
    }

    // Grille : 2 lignes × N colonnes, remplie cellule par cellule.
    constexpr int kCols = 4;
    const int rows = (static_cast<int>(cells.size()) + kCols - 1) / kCols;
    const double cellW = rect.width() / kCols;
    const double cellH = rect.height() / std::max(rows, 1);
    const QString family = QString::fromStdString(cartouche.fontName);
    const double labelHeight = std::max(1.5, cartouche.fontSizeMm * 0.8);

    painter.setPen(QPen(QColor(0, 0, 0), cartouche.borderWidth * 0.75));

    for (std::size_t i = 0; i < cells.size(); ++i) {
        const int row = static_cast<int>(i) / kCols;
        const int col = static_cast<int>(i) % kCols;
        const QRectF cell(rect.left() + col * cellW,
                          rect.top() + row * cellH,
                          cellW, cellH);
        painter.drawRect(cell);

        const QRectF content = cell.adjusted(1.5, 1.0, -1.5, -1.0);
        drawTextMm(painter, content, Qt::AlignLeft | Qt::AlignTop,
                   cells[i].first + QLatin1Char(':'), family, labelHeight, true);
        drawTextMm(painter, content, Qt::AlignLeft | Qt::AlignBottom,
                   cells[i].second, family, cartouche.fontSizeMm);
    }

    // Dessiner le tableau des signatures en dessous
    double signaturesStartY = rect.bottom() + 2.0; // 2mm d'espace
    drawSignaturesTable(painter, rect, cartouche, rect.bottom() + 2.0);

    painter.restore();
}

namespace {

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

// Textes du document, étiquettes et bornes : repère feuille, pour que les
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
        drawTextMm(painter, QPointF(at.x(), at.y()),
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

void drawBornes(QPainter& painter, const std::vector<Borne>& bornes,
                const PageMapping& mapping) {
    if (bornes.empty()) return;
    painter.save();
    painter.setPen(QPen(Qt::black, 0));
    painter.setBrush(Qt::NoBrush);
    for (const auto& borne : bornes) {
        const auto at = mapping.toPage(borne.position);
        const double radius = std::max(0.4, borne.radiusMm);
        painter.drawEllipse(QPointF(at.x(), at.y()), radius, radius);
        drawTextMm(painter, QPointF(at.x() + radius + 0.4, at.y() - radius),
                   QString::fromStdString(borne.numero), kSheetFamily, 1.6);
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

void drawParcelTable(QPainter& painter, const RectMm& rect, const ParcelTable& table) {
    if (!rect.isValid() || table.size() == 0) return;

    // Le tableau est haut de ses lignes, pas de toute la colonne reservee :
    // etire, il produirait 250 mm de vide sous cinq parcelles.
    const std::size_t wantedRows = table.size() + 2;  // en-tête + total
    double rowHeight = std::min(5.0, rect.h / static_cast<double>(wantedRows));
    // Sous 2,5 mm les lignes ne sont plus lisibles : on tronque et on le dit,
    // plutot que d'imprimer un tableau illisible ou de deborder sur le cartouche.
    const bool truncated = rowHeight < 2.5;
    if (truncated) rowHeight = 2.5;
    const std::size_t rows = std::min(wantedRows, static_cast<std::size_t>(rect.h / rowHeight));
    if (rows < 3) return;  // moins que ça, il n'y a rien à lire

    const double height = rows * rowHeight;
    const QRectF box(rect.x, rect.y, rect.w, height);
    const double columnWidth = rect.w / 3.0;

    painter.save();
    painter.setPen(QPen(Qt::black, 0.2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(box);
    for (std::size_t row = 1; row < rows; ++row) {
        const double y = rect.y + row * rowHeight;
        painter.drawLine(QPointF(rect.x, y), QPointF(box.right(), y));
    }
    for (int column = 1; column < 3; ++column) {
        const double x = rect.x + column * columnWidth;
        painter.drawLine(QPointF(x, rect.y), QPointF(x, box.bottom()));
    }

    auto writeCell = [&](std::size_t row, int column, const QString& value, bool bold) {
        if (value.isEmpty()) return;
        drawTextMm(painter, QRectF(rect.x + column * columnWidth + 1.0, rect.y + row * rowHeight,
                                   columnWidth - 2.0, rowHeight),
                   Qt::AlignLeft | Qt::AlignVCenter, value, kSheetFamily,
                   bold ? 2.2 : 2.0, bold);
    };

    writeCell(0, 0, QStringLiteral("Section"), true);
    writeCell(0, 1, QStringLiteral("N°"), true);
    writeCell(0, 2, QStringLiteral("Contenance"), true);

    // Derniere ligne = total, avant-derniere = suite eventuelle.
    const std::size_t totalRow = rows - 1;
    std::size_t dataRows = rows - 2;
    if (truncated && dataRows > 0) --dataRows;
    dataRows = std::min(dataRows, table.size());

    std::size_t row = 1;
    for (; row <= dataRows; ++row) {
        const auto& parcel = table.rows()[row - 1];
        QString area = QString::fromStdString(parcel.contenance);
        if (area.isEmpty() && parcel.area > 0.0)
            area = QString::number(parcel.area, 'f', 2) + QStringLiteral(" m²");
        writeCell(row, 0, QString::fromStdString(parcel.section), false);
        writeCell(row, 1, QString::fromStdString(parcel.numero), false);
        writeCell(row, 2, area, false);
    }
    if (truncated && row < totalRow) {
        writeCell(row, 0,
                  QStringLiteral("… %1 de plus").arg(table.size() - dataRows), false);
    }
    writeCell(totalRow, 0, QStringLiteral("Total"), true);
    if (table.totalArea() > 0.0)
        writeCell(totalRow, 2, QString::number(table.totalArea(), 'f', 2) +
                                   QStringLiteral(" m²"), true);
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
    const double tableWidth = opts.parcelTable.size() > 0 ? kParcelTableWidthMm : 0.0;
    const auto composition = composeSheet(opts.sheet, opts.viewport, opts.cartouche, tableWidth);

    painter.save();
    useMillimetrePage(painter, opts.sheet);

    if (opts.document) {
        drawPlan(painter, *opts.document, composition);
        drawDocumentTexts(painter, *opts.document, composition.mapping);
    }
    if (opts.showLabels) drawLabels(painter, opts.labels, composition.mapping);
    drawBornes(painter, opts.bornes, composition.mapping);
    if (opts.showNorthArrow)
        drawNorthArrow(painter, composition.northArrow, opts.northArrowAngleDeg);
    if (opts.showScaleBar)
        drawScaleBar(painter, composition.scaleBar, composition.mapping.scale);
    drawParcelTable(painter, composition.parcelTable, opts.parcelTable);
    if (opts.cartouche.isValid())
        drawCartouche(painter, toRect(composition.cartouche), opts.cartouche);
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

void applySuggestedScale(PdfExportOptions& opts) {
    const double tableWidth = opts.parcelTable.size() > 0 ? kParcelTableWidthMm : 0.0;
    const auto composition = composeSheet(opts.sheet, opts.viewport, opts.cartouche, tableWidth);
    opts.viewport.setScale(composition.suggestedScale);
    opts.cartouche.echelle = scaleText(static_cast<int>(composition.suggestedScale));
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
