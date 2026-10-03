// Cycle de vie des importeurs de fichier declares par un plugin, pendant de
// FileExporterTest. Ce qui casse n'est pas l'enregistrement mais la DESTRUCTION :
// l'objet vit dans le DSO du module, l'hote doit le detruire pendant le
// dechargement, avant dlclose. Les compteurs d'instances verifient cette regle ;
// `registeredFileImporterIds()` est le traceur qui la rend possible.
//
// L'autre moitie verifie le contrat generique : l'hote ne connait ni format ni
// metier, il demande id/label/extensions et appelle readDocument(), qui ajoute
// au document sans le vider.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo la macro
// NDEBUG est definie et assert() n'evalue pas son argument.

#include "bcad/core/Document.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/plugin/FileImporter.h"
#include "bcad/plugin/PluginRegistry.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <string>

using namespace bcad::plugin;

namespace {

int liveCount = 0;
std::string readPath;

// Importeur fictif : il ajoute un point par lecture, ou signale un echec.
class FakeImporter : public IFileImporter {
public:
    FakeImporter(std::string id, std::string label, std::string extensions, bool ok = true)
        : id_(std::move(id)), label_(std::move(label)),
          extensions_(std::move(extensions)), ok_(ok) { ++liveCount; }
    ~FakeImporter() override { --liveCount; }

    std::string id() const override { return id_; }
    std::string label() const override { return label_; }
    std::string extensions() const override { return extensions_; }

    bool readDocument(bcad::core::Document& document, const std::string& path,
                      std::string* error) const override {
        readPath = path;
        if (!ok_) {
            if (error) *error = "fichier illisible";
            return false;
        }
        document.addEntity(
            std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{1, 2}));
        return true;
    }

private:
    std::string id_, label_, extensions_;
    bool ok_;
};

} // namespace

int main() {
    FileImporterRegistry& registry = FileImporterRegistry::instance();
    registry.clear();
    assert(registry.size() == 0);
    assert(registry.find("absent") == nullptr);
    assert(liveCount == 0);

    // --- Rejets : instance nulle et identifiant vide ---
    PluginRegistry pluginRegistry;
    bool ok = pluginRegistry.registerFileImporter(nullptr);
    assert(!ok);
    PluginRegistry emptyIdRegistry;
    ok = emptyIdRegistry.registerFileImporter(std::make_unique<FakeImporter>("", "Vide", "txt"));
    assert(!ok);
    assert(liveCount == 0);
    assert(emptyIdRegistry.registeredFileImporterIds().empty());

    // --- Enregistrement via le bcad_plugin_init du module ---
    ok = pluginRegistry.registerFileImporter(
        std::make_unique<FakeImporter>("topo.leve", "Leve topo", "txt csv"));
    assert(ok);
    ok = pluginRegistry.registerFileImporter(
        std::make_unique<FakeImporter>("reseau.format", "Format reseau", "net"));
    assert(ok);
    assert(liveCount == 2 && registry.size() == 2);
    assert(pluginRegistry.registeredFileImporterIds().size() == 2);

    // Identifiant deja pris : refuse, la premiere instance reste en place.
    ok = pluginRegistry.registerFileImporter(
        std::make_unique<FakeImporter>("topo.leve", "Doublon", "other"));
    assert(!ok);
    assert(liveCount == 2 && registry.size() == 2);
    assert(pluginRegistry.registeredFileImporterIds().size() == 2);

    // L'ordre d'enregistrement est l'ordre d'affichage du menu.
    const auto all = registry.importers();
    assert(all.size() == 2);
    assert(all[0]->id() == "topo.leve" && all[1]->id() == "reseau.format");
    assert(all[0]->label() == "Leve topo" && all[0]->extensions() == "txt csv");

    // --- Appel par l'hote : ajoute sans vider ---
    bcad::core::Document document;
    document.addEntity(std::make_unique<bcad::geom::PointEntity>(bcad::geom::Point2{0, 0}));
    assert(document.entities().size() == 1);
    std::string error;
    const std::string path = "/tmp/bcad_file_importer_test.txt";
    ok = registry.find("topo.leve")->readDocument(document, path, &error);
    assert(ok && error.empty());
    assert(readPath == path);
    assert(document.entities().size() == 2);

    // Echec signale avec un message, et `error` nul ne casse pas l'appel.
    ok = registry.registerImporter(std::make_unique<FakeImporter>("muet", "Muet", "x", false));
    assert(ok);
    ok = registry.find("muet")->readDocument(document, path, &error);
    assert(!ok && !error.empty());
    ok = registry.find("muet")->readDocument(document, path, nullptr);
    assert(!ok);
    assert(document.entities().size() == 2);
    registry.unregisterImporter("muet");

    // --- Dechargement : destruction AVANT dlclose ---
    registry.unregisterImporter("inconnu");
    assert(registry.size() == 2 && liveCount == 2);
    registry.unregisterImporter("topo.leve");
    assert(registry.size() == 1 && liveCount == 1);
    assert(registry.find("topo.leve") == nullptr);
    assert(registry.find("reseau.format") != nullptr);
    registry.clear();
    assert(registry.size() == 0 && liveCount == 0);

    // --- Rechargement idempotent apres dechargement ---
    ok = registry.registerImporter(std::make_unique<FakeImporter>("topo.leve", "Leve topo", "txt"));
    assert(ok);
    ok = registry.registerImporter(std::make_unique<FakeImporter>("topo.leve", "Encore", "txt"));
    assert(!ok);
    assert(registry.size() == 1 && liveCount == 1);
    registry.clear();
    assert(liveCount == 0);

    (void)ok;
    std::printf("FileImporter : tests PASSED\n");
    return 0;
}
