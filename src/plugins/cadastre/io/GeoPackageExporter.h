#pragma once

#include "bcad/plugin/FileExporter.h"

namespace bcad::cadastre {

// GeoPackage : format d'echange du module cadastral. Aucun chemin n'existait
// pour qu'un plugin expose un format portant tout un document (un
// IEntitySerializer est mono-entite) ; `registerFileExporter` est ce chemin, et
// le menu « Exporter » de l'hote le decouvre sans que src/app nomme le cadastre.
class GeoPackageExporter final : public plugin::IFileExporter {
public:
    std::string id() const override { return "cadastre.geopackage"; }
    std::string label() const override { return "GeoPackage cadastral"; }
    std::string extension() const override { return "gpkg"; }

    bool writeDocument(const core::Document& document, const std::string& path,
                       std::string* error) const override;
};

} // namespace bcad::cadastre
