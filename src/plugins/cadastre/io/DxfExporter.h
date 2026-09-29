#pragma once

// Export DXF cadastral complet (I3) : calques CADASTRE/COTATION/CARTOUCHE
// avec cartouche, étiquettes, bornes, flèche Nord, barre d'échelle, tableau
// parcellaire — composés par le module, l'hôte n'écrit que la géométrie.

#include "bcad/plugin/FileExporter.h"
#include <string>

namespace bcad::cadastre {

class DxfExporter final : public plugin::IFileExporter {
public:
    std::string id() const override { return "cadastre.dxf"; }
    std::string label() const override { return "Plan cadastral DXF"; }
    std::string extension() const override { return "dxf"; }

    bool writeDocument(const core::Document& document,
                       const std::string& path,
                       std::string* error = nullptr) const override;
};

} // namespace bcad::cadastre