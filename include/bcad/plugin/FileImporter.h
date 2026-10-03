#pragma once

// Extension d'import fichier, pendant de IFileExporter (ADR-005/016). Un format
// d'échange porte tout un document, ce qu'un IEntitySerializer — mono-entité,
// clé = TypeId — ne peut pas exprimer. D'où un point d'extension distinct.
//
// L'hote ne connaît AUCUN format : il dresse la liste des importeurs enregistres,
// construit le filtre de la boîte d'ouverture avec leurs extensions, et appelle
// readDocument(). Un module métier ajoute donc son format sans qu'une ligne de
// src/app le nomme.
//
// Contrat d'ABI (PLUGIN_ARCHITECTURE.md §13) : l'objet est cree par son déclarant
// mais detenu par l'hote, comme les exporteurs. Il est detruit par l'hote
// PENDANT le dechargement, avant dlclose, jamais apres.

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

    // Identifiant stable (ex. "geojson", "cadastre.levé").
    virtual std::string id() const = 0;

    // Libellé de l'action d'import, dans la langue du déclarant. L'hote
    // l'affiche tel quel.
    virtual std::string label() const = 0;

    // Extension(s) acceptées, sans le point, séparées par une espace
    // (ex. "geojson json"). Sert à construire le filtre de la boîte d'ouverture.
    virtual std::string extensions() const = 0;

    // Lit `path` et AJOUTE ses entités à `document`, sans le vider : l'appelant
    // décide. Faux en cas d'échec, avec le message dans `error` (peut être nul).
    virtual bool readDocument(core::Document& document,
                              const std::string& path,
                              std::string* error) const = 0;
};

// Registre des importeurs. Singleton porté par l'hote (comme
// FileExporterRegistry) : un plugin ne fait qu'enregistrer, l'hote detient les
// instances.
class BCAD_PLUGIN_API FileImporterRegistry {
public:
    static FileImporterRegistry& instance();

    // Faux si l'identifiant est deja pris.
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
