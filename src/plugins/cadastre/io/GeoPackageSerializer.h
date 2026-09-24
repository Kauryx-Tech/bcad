#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::cadastre {

class GeoPackageSerializer {
public:
    bool write(const std::string& path, const core::Document& document) const;
    bool read(const std::string& path, core::Document& document) const;
};

} // namespace bcad::cadastre
