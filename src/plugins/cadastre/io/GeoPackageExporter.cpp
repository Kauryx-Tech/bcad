#include "GeoPackageExporter.h"
#include "GeoPackageSerializer.h"

#include "bcad/core/Document.h"

namespace bcad::cadastre {

bool GeoPackageExporter::writeDocument(const core::Document& document,
                                       const std::string& path,
                                       std::string* error) const {
    if (!GeoPackageSerializer{}.write(path, document)) {
        if (error) *error = "écriture GeoPackage impossible : " + path;
        return false;
    }
    return true;
}

} // namespace bcad::cadastre
