#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Writes a subset of DXF R2000 (AC1015) ASCII: LAYER table plus
// LINE/CIRCLE/ARC/LWPOLYLINE entities. Curves stored natively (not
// pre-tessellated) so round-tripping through AutoCAD/LibreCAD/QCAD stays exact.
bool writeDxf(const std::string& path, const core::Document& doc);

} // namespace bcad::io
