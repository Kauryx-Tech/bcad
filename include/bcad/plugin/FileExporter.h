#pragma once

// Extension d'export fichier (ADR-005/016). Un format d'échange n'est pas une
// entité : il porte tout un document (une FeatureCollection GeoJSON, une base
// GeoPackage), ce qu'un IEntitySerializer — mono-entité, clé = TypeId — ne peut
// pas exprimer. D'où un point d'extension distinct.
//
// L'hote ne connaît AUCUN format : il dresse la liste des exporteurs enregistres,
// demande le libellé et l'extension à leur déclarant, et appelle writeDocument().
// Un module métier ajoute donc son format sans qu'une ligne de src/app le nomme.
//
// Contrat d'ABI (PLUGIN_ARCHITECTURE.md §13) : l'objet est cree par son déclarant
// mais detenu par l'hote, comme les validateurs et les workbenches. Il est detruit
// par l'hote PENDANT le dechargement, avant dlclose, jamais apres.

#include "bcad/plugin/Api.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::core {
class Document;
} // namespace bcad::core

namespace bcad::plugin {

class BCAD_PLUGIN_API IFileExporter {
public:
    virtual ~IFileExporter() = default;

    // Identifiant stable (ex. "geojson", "cadastre.geopackage").
    virtual std::string id() const = 0;

    // Libellé de l'action d'export, dans la langue du déclarant
    // (ex. "GeoPackage"). L'hote l'affiche tel quel.
    virtual std::string label() const = 0;

    // Extension du fichier produit, sans le point (ex. "gpkg"). Sert à
    // construire le filtre de la boîte d'enregistrement.
    virtual std::string extension() const = 0;

    // Écrit le document complet à `path`. Faux en cas d'échec, avec le message
    // dans `error` (peut être nul). Ne modifie jamais le document.
    virtual bool writeDocument(const core::Document& document,
                               const std::string& path,
                               std::string* error) const = 0;
};

// Registre des exporteurs. Singleton porté par l'hote (comme
// WorkbenchRegistry et ValidatorRegistry) : un plugin ne fait qu'enregistrer,
// l'hote detient les instances.
class BCAD_PLUGIN_API FileExporterRegistry {
public:
    static FileExporterRegistry& instance();

    // Faux si l'identifiant est deja pris.
    bool registerExporter(std::unique_ptr<IFileExporter> exporter);
    void unregisterExporter(const std::string& id);
    void clear();

    std::vector<const IFileExporter*> exporters() const;
    const IFileExporter* find(std::string_view id) const;
    size_t size() const { return entries_.size(); }

    FileExporterRegistry(const FileExporterRegistry&) = delete;
    FileExporterRegistry& operator=(const FileExporterRegistry&) = delete;

private:
    FileExporterRegistry() = default;

    std::vector<std::unique_ptr<IFileExporter>> entries_;
};

} // namespace bcad::plugin
