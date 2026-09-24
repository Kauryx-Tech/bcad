#pragma once

#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include "bcad/layout/Cartouche.h"
#include <string>

class QPainter;
class QRectF;

namespace bcad::core {
class Document;
}

namespace bcad::layout {

// Export PDF vectoriel (I2) — interface, implémentation via QPrinter dans src/layout
struct PdfExportOptions {
    std::string outputPath;
    Sheet sheet;
    Viewport viewport;
    Cartouche cartouche;
    const core::Document* document = nullptr;
    bool showLabels = true;
};

bool exportPdf(const PdfExportOptions& opts, std::string* error = nullptr);

// Dessine le cartouche enrichi (grille label+valeur) dans `rect`.
// Partagé par l'export PDF et l'aperçu impression (MainWindow).
// Chaque cellule non vide du cartouche apparaît comme "LABEL : valeur".
void drawCartouche(QPainter& painter, const QRectF& rect, const Cartouche& cartouche);

} // namespace bcad::layout
