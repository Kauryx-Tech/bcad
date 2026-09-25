#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

// Ecrit le sous-ensemble DXF que BCAD relit : geometrie, calques reels du
// document, et les proprietes de chaque entite en XDATA BCAD_PROPS. Aucun nom
// de cle, aucun type d'entite d'un module metier n'intervient ici.
bool writeDxf(const std::string& path, const core::Document& doc);
bool readDxf(const std::string& path, core::Document& doc);

} // namespace bcad::io