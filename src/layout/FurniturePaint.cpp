// Peintre des meubles déclaratifs : voir FurniturePaint.h.

#include "bcad/layout/FurniturePaint.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QString>
#include <Qt>
#include <QtMath>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <sstream>

namespace bcad::layout {

namespace {

constexpr double kFontSubdivision = 16.0;

QString toQString(const std::string& text) {
    return QString::fromStdString(text);
}

} // namespace

void drawTextMm(QPainter& painter, const QRectF& rect, int flags, const QString& text,
                const QString& family, double heightMm, bool bold) {
    if (text.isEmpty() || !(heightMm > 0.0) || rect.width() <= 0.0 || rect.height() <= 0.0)
        return;
    heightMm = std::min(heightMm, 1e4);   // au-dela, qRound deborde l'entier
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

std::string formatFieldValue(const Field& champ) {
    using properties::PropertyType;
    const auto& value = champ.value.value;
    switch (champ.value.type) {
        case PropertyType::String:
            if (const auto* texte = std::get_if<std::string>(&value)) return *texte;
            return {};
        case PropertyType::Double: {
            if (const auto* nombre = std::get_if<double>(&value)) {
                std::ostringstream ss;
                ss.precision(10);
                ss << std::fixed << *nombre;
                std::string texte = ss.str();
                while (texte.size() > 1 && texte.back() == '0') texte.pop_back();
                if (!texte.empty() && texte.back() == '.') texte.pop_back();
                return texte;
            }
            return {};
        }
        case PropertyType::Int:
            if (const auto* entier = std::get_if<int>(&value)) return std::to_string(*entier);
            return {};
        case PropertyType::Bool:
            if (const auto* booleen = std::get_if<bool>(&value))
                return *booleen ? "true" : "false";
            return {};
        case PropertyType::Color:
            if (const auto* couleur = std::get_if<geom::Color>(&value)) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "#%02x%02x%02x",
                              static_cast<int>(couleur->r * 255.0f),
                              static_cast<int>(couleur->g * 255.0f),
                              static_cast<int>(couleur->b * 255.0f));
                return buf;
            }
            return {};
        case PropertyType::Enum:
            if (const auto* index = std::get_if<properties::EnumIndex>(&value)) {
                if (index->value >= 0 &&
                    static_cast<std::size_t>(index->value) < champ.value.enumValues.size())
                    return champ.value.enumValues[static_cast<std::size_t>(index->value)];
                return "#" + std::to_string(index->value);
            }
            return {};
    }
    return {};
}

namespace {

struct Appearance {
    QString family = QStringLiteral("Standard");
    double fontSizeMm = 2.5;
    double borderWidth = 0.5;
};

Appearance appearanceOf(const FurnitureTemplate& gabarit) {
    Appearance apparence;
    if (!gabarit.fontName.empty()) apparence.family = toQString(gabarit.fontName);
    if (gabarit.fontSizeMm > 0.0) apparence.fontSizeMm = gabarit.fontSizeMm;
    if (gabarit.borderWidth > 0.0) apparence.borderWidth = gabarit.borderWidth;
    return apparence;
}

// Champs à peindre, dans l'ordre des slots : un libellé sans valeur se peint
// vide et se signale, il ne disparaît pas — sauf les champs « image », dont
// une valeur vide n'est pas une case à montrer mais une absence de case.
std::vector<const Field*> paintableFields(const ResolvedFurniture& meuble) {
    std::vector<const Field*> champs;
    for (const Field& champ : meuble.fields) {
        if (champ.format == "image" && !champ.hasValue()) continue;
        champs.push_back(&champ);
    }
    std::sort(champs.begin(), champs.end(),
              [](const Field* a, const Field* b) { return a->slot < b->slot; });
    return champs;
}

void drawImageOrLine(QPainter& painter, const QRectF& cell, const Field& champ) {
    QImage image;
    if (champ.hasValue()) {
        if (const auto* chemin = std::get_if<std::string>(&champ.value.value)) {
            if (!chemin->empty()) image.load(toQString(*chemin));
        }
    }
    if (!image.isNull()) {
        const QRectF target = cell.adjusted(0.8, 0.8, -0.8, -0.8);
        const QSize src = image.size();
        if (src.width() > 0 && src.height() > 0) {
            const double factor =
                std::min(target.width() / static_cast<double>(src.width()),
                         target.height() / static_cast<double>(src.height()));
            const QSizeF box(src.width() * factor, src.height() * factor);
            painter.drawImage(QRectF(target.center().x() - box.width() / 2.0,
                                     target.center().y() - box.height() / 2.0,
                                     box.width(), box.height()),
                              image);
            return;
        }
    }
    painter.drawLine(QPointF(cell.left() + 1.0, cell.bottom() - 1.2),
                     QPointF(cell.right() - 1.0, cell.bottom() - 1.2));
}

} // namespace

void drawFurniture(QPainter& painter, const QRectF& rect, const ResolvedFurniture& meuble) {
    if (rect.width() <= 0.0 || rect.height() <= 0.0) return;
    const Appearance apparence = appearanceOf(meuble.gabarit);
    const auto champs = paintableFields(meuble);
    if (champs.empty()) return;

    painter.save();
    painter.setPen(QPen(QColor(0, 0, 0), apparence.borderWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);

    const int columns = std::max(1, meuble.gabarit.columns);
    const int rows = (static_cast<int>(champs.size()) + columns - 1) / columns;
    const double cellW = rect.width() / columns;
    const double cellH = rect.height() / std::max(rows, 1);
    const double labelHeight = std::max(1.5, apparence.fontSizeMm * 0.8);
    painter.setPen(QPen(QColor(0, 0, 0), apparence.borderWidth * 0.75));

    for (std::size_t i = 0; i < champs.size(); ++i) {
        const int row = static_cast<int>(i) / columns;
        const int col = static_cast<int>(i) % columns;
        const QRectF cell(rect.left() + col * cellW, rect.top() + row * cellH, cellW, cellH);
        painter.drawRect(cell);
        const QRectF content = cell.adjusted(1.5, 1.0, -1.5, -1.0);
        drawTextMm(painter, content, Qt::AlignLeft | Qt::AlignTop,
                   toQString(champs[i]->label) + QLatin1Char(':'), apparence.family,
                   labelHeight, true);
        if (champs[i]->format == "image") {
            drawImageOrLine(painter, content, *champs[i]);
        } else {
            drawTextMm(painter, content, Qt::AlignLeft | Qt::AlignBottom,
                       toQString(formatFieldValue(*champs[i])), apparence.family,
                       apparence.fontSizeMm);
        }
    }
    painter.restore();
}

void drawFurnitureTable(QPainter& painter, const QRectF& rect, const ResolvedFurniture& meuble) {
    if (rect.width() <= 0.0 || rect.height() <= 0.0) return;
    const Appearance apparence = appearanceOf(meuble.gabarit);
    const auto champs = paintableFields(meuble);
    if (champs.empty() && meuble.gabarit.columnLabels.empty()) return;

    const int columns = std::max(1, meuble.gabarit.columns);
    const double cellW = rect.width() / columns;
    const double headerH =
        meuble.gabarit.columnLabels.empty() ? 0.0 : std::max(3.0, apparence.fontSizeMm * 1.8);
    const double rowH = std::max(2.5, apparence.fontSizeMm * 2.0);

    painter.save();
    painter.setPen(QPen(QColor(0, 0, 0), apparence.borderWidth * 0.75));
    painter.setBrush(Qt::NoBrush);

    double top = rect.top();
    if (headerH > 0.0) {
        for (int col = 0; col < columns; ++col) {
            const QRectF cell(rect.left() + col * cellW, top, cellW, headerH);
            painter.drawRect(cell);
            const std::string label = static_cast<std::size_t>(col) <
                                              meuble.gabarit.columnLabels.size()
                                          ? meuble.gabarit.columnLabels[static_cast<std::size_t>(col)]
                                          : std::string();
            drawTextMm(painter, cell, Qt::AlignCenter | Qt::AlignVCenter, toQString(label),
                       apparence.family, apparence.fontSizeMm, true);
        }
        top += headerH;
    }

    const double usable = rect.bottom() - top;
    const std::size_t capacity = usable > 0.0 && rowH > 0.0
                                     ? static_cast<std::size_t>(usable / rowH)
                                     : 0;
    if (capacity == 0) {
        painter.restore();
        return;
    }
    const std::size_t rows = (champs.size() + static_cast<std::size_t>(columns) - 1) /
                             static_cast<std::size_t>(columns);
    const std::size_t shown = std::min(rows, capacity);

    for (std::size_t row = 0; row < shown; ++row) {
        for (int col = 0; col < columns; ++col) {
            const std::size_t i = row * static_cast<std::size_t>(columns) +
                                  static_cast<std::size_t>(col);
            const QRectF cell(rect.left() + col * cellW, top + row * rowH, cellW, rowH);
            painter.drawRect(cell);
            if (i >= champs.size()) continue;
            if (champs[i]->format == "image") {
                drawImageOrLine(painter, cell.adjusted(1.0, 1.0, -1.0, -1.0), *champs[i]);
            } else {
                drawTextMm(painter, cell.adjusted(1.0, 0.5, -1.0, -0.5),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           toQString(formatFieldValue(*champs[i])), apparence.family,
                           apparence.fontSizeMm);
            }
        }
    }

    // Lignes non montrées : le dit, plutôt que de laisser croire que le
    // tableau est complet.
    if (shown < rows) {
        drawTextMm(painter, rect, Qt::AlignRight | Qt::AlignBottom,
                   QStringLiteral("+%1").arg((rows - shown) * static_cast<std::size_t>(columns)),
                   apparence.family, apparence.fontSizeMm);
    }
    painter.restore();
}

} // namespace bcad::layout
