#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Écrit un sous-ensemble de DXF R2000 (AC1015) ASCII : table LAYER plus
// entités LINE/CIRCLE/ARC/LWPOLYLINE. Les courbes sont stockées nativement
// (non pré-tessellées) pour que l'aller-retour via AutoCAD/LibreCAD/QCAD
// reste exact.
bool writeDxf(const std::string& path, const core::Document& doc);

} // namespace bcad::io
