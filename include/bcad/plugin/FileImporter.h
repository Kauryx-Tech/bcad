#pragma once

// Extension d'import fichier, pendant de IFileExporter (ADR-005/016).
// Un plugin enregistre un IFileImporter pour lire un format de fichier
// complet dans un Document. L'hote liste les importeurs enregistrés,
// construit le filtre de la boîte d'ouverture, et appelle readDocument().
//
// Contrat d'ABI : même règle que IFileExporter — l'objet est créé par son
// déclarant et détruit par l'hote avant dlclose.

#include "bcad/plugin/Api.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::core {
class Document;
} // namespace bcad::core

namespace bcad::plugin {

class BCAD_PLUGIN_API IFileImporter {
public:
    virtual ~IFileImporter() = default;

    // Identifiant stable (ex. "geojson", "shapefile").
    virtual std::string id() const = 0;

    // Libellé affiché dans la boîte de dialogue (ex. "GeoJSON").
    virtual std::string label() const = 0;

    // Extension(s) acceptées, sans le point, séparées par espace (ex. "geojson json").
    virtual std::string extensions() const = 0;

    // Lit `path` et ajoute les entités dans `document`. Faux en cas d'échec ;
    // le message d'erreur optionnel est écrit dans `error`. Ne vide PAS le
    // document avant d'importer — l'appelant décide.
    virtual bool readDocument(const std::string& path,
                              core::Document& document,
                              std::string* error) const = 0;
};

class BCAD_PLUGIN_API FileImporterRegistry {
public:
    static FileImporterRegistry& instance();

    bool registerImporter(std::unique_ptr<IFileImporter> importer);
    void unregisterImporter(const std::string& id);
    void clear();

    std::vector<const IFileImporter*> importers() const;
    const IFileImporter* find(std::string_view id) const;
    size_t size() const { return entries_.size(); }

    FileImporterRegistry(const FileImporterRegistry&) = delete;
    FileImporterRegistry& operator=(const FileImporterRegistry&) = delete;

private:
    FileImporterRegistry() = default;

    std::vector<std::unique_ptr<IFileImporter>> entries_;
};

} // namespace bcad::plugin
