#pragma once

#include "bcad/core/Document.h"
#include <string>

namespace bcad::cadastre {

class ArcGisAdapter {
public:
    bool exportToGdb(const std::string& path, const core::Document& document) const;
};

} // namespace bcad::cadastre
