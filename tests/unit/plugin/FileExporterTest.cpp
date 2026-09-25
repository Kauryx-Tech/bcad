// Cycle de vie des exporteurs de fichier declares par un plugin. Comme pour les
// workbenches et les validateurs, ce qui casse n'est pas l'enregistrement mais la
// DESTRUCTION : l'objet vit dans le DSO du module, l'hote doit donc le detruire
// pendant le dechargement, avant dlclose. Les compteurs d'instances verifient
// precisement cette regle ; `registeredFileExporterIds()` est le traceur qui la
// rend possible.
//
// L'autre moitie du test verifie le contrat generique : l'hote ne connait ni
// format ni metier, il demande id/label/extension et appelle writeDocument().
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo la macro
// NDEBUG est definie et assert() n'evalue pas son argument.

#include "bcad/core/Document.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/plugin/FileExporter.h"
#include "bcad/plugin/PluginRegistry.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad::plugin;

namespace {

int liveCount = 0;
std::string writtenPath;

// Exporteur fictif : il ecrit une ligne et signale les chemins impossibles.
class FakeExporter : public IFileExporter {
public:
    FakeExporter(std::string id, std::string label, std::string extension, bool ok = true)
        : id_(std::move(id)), label_(std::move(label)),
          extension_(std::move(extension)), ok_(ok) { ++liveCount; }
    ~FakeExporter() override { --liveCount; }

    std::string id() const override { return id_; }
    std::string label() const override { return label_; }
    std::string extension() const override { return extension_; }

    bool writeDocument(const bcad::core::Document& document, const std::string& path,
                       std::string* error) const override {
        writtenPath = path;
        // Le contrat dit « ne modifie jamais le document » : le test verifie
        // qu'il est vide apres l'appel, donc l'exporteur ne doit rien y ajouter.
        if (!ok_) {
            if (error) *error = "disque indisponible";
            return false;
        }
        std::ofstream out(path, std::ios::binary);
        out << "exporte\n";
        return static_cast<bool>(out);
    }

private:
    std::string id_, label_, extension_;
    bool ok_;
};

} // namespace

int main() {
    FileExporterRegistry& registry = FileExporterRegistry::instance();
    registry.clear();
    assert(registry.size() == 0);
    assert(registry.find("absent") == nullptr);
    assert(liveCount == 0);

    // --- Rejets : instance nulle et identifiant vide ---
    PluginRegistry pluginRegistry;
    assert(!pluginRegistry.registerFileExporter(nullptr));
    PluginRegistry emptyIdRegistry;
    auto emptyId = std::make_unique<FakeExporter>("", "Vide", "txt");
    assert(!emptyIdRegistry.registerFileExporter(std::move(emptyId)));
    assert(liveCount == 0);   // refuses : l'hote n'en detient aucun
    assert(emptyIdRegistry.registeredFileExporterIds().empty());

    // --- Enregistrement via le bcad_plugin_init du module ---
    assert(pluginRegistry.registerFileExporter(
        std::make_unique<FakeExporter>("topo.format", "Format topo", "topo")));
    assert(pluginRegistry.registerFileExporter(
        std::make_unique<FakeExporter>("reseau.format", "Format reseau", "net")));
    assert(liveCount == 2 && registry.size() == 2);
    assert(pluginRegistry.registeredFileExporterIds().size() == 2);

    // Identifiant deja pris : refuse, la premiere instance reste en place.
    assert(!pluginRegistry.registerFileExporter(
        std::make_unique<FakeExporter>("topo.format", "Doublon", "other")));
    assert(liveCount == 2 && registry.size() == 2);

    // L'ordre d'enregistrement est l'ordre d'affichage du menu.
    const auto all = registry.exporters();
    assert(all.size() == 2);
    assert(all[0]->id() == "topo.format" && all[1]->id() == "reseau.format");
    // L'hote n'a que des chaines generiques a afficher.
    assert(all[0]->label() == "Format topo" && all[0]->extension() == "topo");

    // --- Appel par l'hote, sans connaitre le format ---
    bcad::core::Document document;
    auto* point = document.addEntity(
        std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{3, 4}));
    assert(point && document.entities().size() == 1);
    std::string error;
    const std::string path = "/tmp/bcad_file_exporter_test.topo";
    std::remove(path.c_str());
    assert(registry.find("topo.format")->writeDocument(document, path, &error));
    assert(error.empty());
    std::ifstream check(path);
    std::string line;
    assert(std::getline(check, line) && line == "exporte");
    assert(writtenPath == path);
    std::remove(path.c_str());
    // L'export n'a pas touche au document.
    assert(document.entities().size() == 1);

    // Echec signale avec un message, et `error` nul ne casse pas l'appel.
    assert(registry.registerExporter(std::make_unique<FakeExporter>("mute", "Muet", "x", false)));
    error.clear();
    assert(!registry.find("mute")->writeDocument(document, path, &error));
    assert(!error.empty());
    assert(!registry.find("mute")->writeDocument(document, path, nullptr));
    registry.unregisterExporter("mute");

    // --- Dechargement : destruction AVANT dlclose ---
    registry.unregisterExporter("inconnu");
    assert(registry.size() == 2 && liveCount == 2);
    registry.unregisterExporter("topo.format");
    assert(registry.size() == 1 && liveCount == 1);
    assert(registry.find("topo.format") == nullptr);
    assert(registry.find("reseau.format") != nullptr);
    registry.clear();
    assert(registry.size() == 0 && liveCount == 0);

    // --- Rechargement idempotent apres dechargement ---
    assert(registry.registerExporter(std::make_unique<FakeExporter>("topo.format", "Format topo", "topo")));
    assert(!registry.registerExporter(std::make_unique<FakeExporter>("topo.format", "Encore", "topo")));
    assert(registry.size() == 1 && liveCount == 1);
    registry.clear();
    assert(liveCount == 0);

    std::printf("FileExporter : tests PASSED\n");
    return 0;
}
