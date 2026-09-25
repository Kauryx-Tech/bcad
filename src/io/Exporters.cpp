#include "bcad/io/Exporters.h"
#include "bcad/core/Document.h"
#include "bcad/io/DxfWriter.h"
#include "bcad/io/Exchange.h"
#include "bcad/plugin/FileExporter.h"

#include <memory>

namespace bcad::io {

namespace {

using plugin::FileExporterRegistry;
using plugin::IFileExporter;

// Les formats du noyau sont de fines coquilles sur les écrivains de ce module :
// ils existent pour que l'hôte n'ait qu'une liste d'exporteurs à afficher, pas
// trois actions de menu codées à la main.
class TextExporter final : public IFileExporter {
public:
    TextExporter(std::string id, std::string label, std::string extension,
                 std::string (*write)(const core::Document&))
        : id_(std::move(id)), label_(std::move(label)),
          extension_(std::move(extension)), write_(write) {}

    std::string id() const override { return id_; }
    std::string label() const override { return label_; }
    std::string extension() const override { return extension_; }

    bool writeDocument(const core::Document& document, const std::string& path,
                       std::string* error) const override {
        return writeTextFile(path, write_(document), error);
    }

private:
    std::string id_;
    std::string label_;
    std::string extension_;
    std::string (*write_)(const core::Document&);
};

class DxfExporter final : public IFileExporter {
public:
    std::string id() const override { return "dxf"; }
    std::string label() const override { return "DXF"; }
    std::string extension() const override { return "dxf"; }

    bool writeDocument(const core::Document& document, const std::string& path,
                       std::string* error) const override {
        if (writeDxf(path, document)) return true;
        if (error) *error = "écriture DXF impossible : " + path;
        return false;
    }
};

} // namespace

void initializeNativeFileExporters() {
    auto& registry = FileExporterRegistry::instance();
    if (!registry.find("geojson"))
        registry.registerExporter(std::make_unique<TextExporter>(
            "geojson", "GeoJSON", "geojson", &toGeoJson));
    if (!registry.find("csv"))
        registry.registerExporter(std::make_unique<TextExporter>(
            "csv", "Coordonnées CSV", "csv", &toCoordinateCsv));
    if (!registry.find("dxf"))
        registry.registerExporter(std::make_unique<DxfExporter>());
}

} // namespace bcad::io
