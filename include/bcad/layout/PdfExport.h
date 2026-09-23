#pragma once

#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include "bcad/layout/Cartouche.h"
#include <string>

namespace bcad::layout {

// Export PDF vectoriel (I2) — interface, implémentation via QPrinter dans src/layout
struct PdfExportOptions {
    std::string outputPath;
    Sheet sheet;
    Viewport viewport;
    Cartouche cartouche;
    bool showLabels = true;
};

bool exportPdf(const PdfExportOptions& opts, std::string* error = nullptr);

} // namespace bcad::layout
