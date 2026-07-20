#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Reads the DXF subset produced by writeDxf(), and is lenient enough to
// import simple LINE/CIRCLE/ARC/LWPOLYLINE/POLYLINE geometry from files
// exported by other CAD tools (AutoCAD, LibreCAD, QCAD).
//
// Populates outDoc in place (clearing it first) rather than returning a
// Document by value: Document holds a std::shared_mutex guarding its
// spatial index, which makes it intentionally non-copyable/movable.
bool readDxf(const std::string& path, core::Document& outDoc);

} // namespace bcad::io
