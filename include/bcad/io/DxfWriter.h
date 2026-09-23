#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::io {

bool writeDxf(const std::string& path, const core::Document& doc, bool fullCadastre = false);
bool readDxf(const std::string& path, core::Document& doc);

} // namespace bcad::io