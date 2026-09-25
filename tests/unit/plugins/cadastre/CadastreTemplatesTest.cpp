// Les gabarits du module sont des donnees installees, pas du code (C3). Avant
// ce branchement, les quatre fichiers de templates/ etaient installes et jamais
// ouverts : le motif de section etait ecrit en dur dans le validateur. Ce test
// prouve les deux bouts du chemin — l'hote propose des repertoires, le module y
// nomme son fichier, et la regle gardee par l'hote applique la valeur lue.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "Templates.h"
#include "entities/ParcelEntity.h"
#include "validation/CadastreValidators.h"

#include "bcad/geometry/Point.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/validation/Diagnostics.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;
using validation::Severity;

namespace {

std::unique_ptr<ParcelEntity> makeParcel(const std::string& section, const std::string& numero) {
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{
        {0, 0}, {4, 0}, {4, 4}, {0, 4}});
    parcel->properties().setString("cadastre.section", section);
    parcel->properties().setString("cadastre.numero", numero);
    return parcel;
}

// Repertoire de donnees ephemere, range a la fin du test.
class ScratchDataDir {
public:
    explicit ScratchDataDir(const std::string& name)
        : root_(std::filesystem::temp_directory_path() / ("bcad_" + name + "_test")) {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
        std::filesystem::create_directories(root_ / "cadastre" / "templates", ec);
    }

    ~ScratchDataDir() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    void write(const std::string& file, const std::string& contents) const {
        const std::filesystem::path path = root_ / "cadastre" / "templates" / file;
        std::ofstream out(path, std::ios::binary);
        out << contents;
        assert(out.good());
    }

    std::string path() const { return root_.string(); }

private:
    std::filesystem::path root_;
};

bool mentions(const validation::Diagnostic& diagnostic, const std::string& needle) {
    return diagnostic.message.find(needle) != std::string::npos;
}

// Un profil qui resserre la section a deux lettres exactement.
constexpr const char* kTwoLetters = R"({
  "schema_version": 1,
  "country": "BJ",
  "name": "Cadastre Benin",
  "parcel_identifier": { "section_pattern": "^[A-Z]{2}$" }
})";

void checkFallsBackToDefaults() {
    plugin::PluginRegistry empty;
    const CadastreTemplates fallback = loadCadastreTemplates(empty, "cadastre_togo");
    // Rien n'est applique, mais la regle tient toujours : c'est ce qui permet a
    // un module charge sans donnees installees de verifier autant qu'avant.
    assert(fallback.source.empty());
    assert(fallback.sectionPattern == "^[A-Z]{1,3}$");
    assert(fallback.numberPattern == "^[0-9]+$");
}

void checkReadsInstalledProfile() {
    plugin::PluginRegistry registry;
    registry.addDataDirectory(BCAD_CADASTRE_DATA_DIR);
    const CadastreTemplates togo = loadCadastreTemplates(registry, "cadastre_togo");
    // Le gabarit installe est bien ouvert : le chemin rendu existe, et ses
    // valeurs sont celles du fichier (identiques aux par defaut aujourd'hui).
    assert(!togo.source.empty());
    std::error_code ec;
    assert(std::filesystem::is_regular_file(togo.source, ec));
    assert(togo.sectionPattern == "^[A-Z]{1,3}$");
    assert(togo.numberPattern == "^[0-9]+$");

    // Le nom du fichier est choisi par l'hote : un profil inconnu ne fait pas
    // echouer le chargement, il laisse les valeurs par defaut.
    assert(loadCadastreTemplates(registry, "profil_absent").source.empty());
}

void checkProfileReplacesPatterns() {
    const ScratchDataDir dir("profile");
    dir.write("cadastre_benin.json", kTwoLetters);
    plugin::PluginRegistry registry;
    registry.addDataDirectory(dir.path());
    const CadastreTemplates benin = loadCadastreTemplates(registry, "cadastre_benin");
    assert(benin.sectionPattern == "^[A-Z]{2}$");
    // Cle absente du gabarit = valeur par defaut conservee, pas chaine vide.
    assert(benin.numberPattern == "^[0-9]+$");
    assert(!benin.source.empty());
}

void checkIgnoredProfilesKeepDefaults() {
    const ScratchDataDir dir("maladroit");
    dir.write("casse.json", "{ ce n'est pas du JSON ");
    dir.write("avenir.json", R"({"schema_version": 2, "parcel_identifier":
        {"section_pattern": "^[A-Z]{9}$"}})");
    dir.write("vide.json", R"({"schema_version": 1, "parcel_identifier":
        {"section_pattern": ""}})");

    plugin::PluginRegistry registry;
    registry.addDataDirectory(dir.path());
    assert(loadCadastreTemplates(registry, "casse").source.empty());
    // Schema plus recent que le module : ses cles pourraient vouloir dire
    // autre chose, elles ne sont pas appliquees.
    const CadastreTemplates future = loadCadastreTemplates(registry, "avenir");
    assert(future.source.empty() && future.sectionPattern == "^[A-Z]{1,3}$");
    // Une valeur vide n'est pas une valeur : elle ne desarme pas la regle, et
    // le fichier reste celui qui a ete lu.
    const CadastreTemplates vide = loadCadastreTemplates(registry, "vide");
    assert(!vide.source.empty());
    assert(vide.sectionPattern == "^[A-Z]{1,3}$");
}

// Bout du chemin : la regle declaree a l'hote applique le motif lu.
void checkRuleAppliesLoadedProfile() {
    const ScratchDataDir dir("regle");
    dir.write("cadastre_benin.json", kTwoLetters);
    plugin::PluginRegistry registry;
    registry.addDataDirectory(dir.path());
    const CadastreTemplates benin = loadCadastreTemplates(registry, "cadastre_benin");

    ParcelIdentifierRuleValidator rule(benin.sectionPattern, benin.numberPattern);
    auto two = makeParcel("AB", "001");
    auto one = makeParcel("A", "002");
    two->setId(41);
    one->setId(42);

    const auto clean = rule.validate({two.get()});
    assert(clean.empty());

    const auto reports = rule.validate({one.get()});
    assert(reports.size() == 1);
    assert(reports[0].severity == Severity::Error);
    // Le libelle cite le motif du gabarit, pas une description figee :
    // l'operateur voit la regle qui a vraiment servi.
    assert(mentions(reports[0], "^[A-Z]{2}$"));
    assert(mentions(reports[0], "section"));

    // Sans gabarit, la meme regle refuse une section de quatre lettres.
    ParcelIdentifierRuleValidator historical;
    assert(historical.validate({two.get()}).empty());
    auto four = makeParcel("ABCD", "003");
    four->setId(43);
    assert(historical.validate({four.get()}).size() == 1);
    // Le module n'a pas oublie de construire la regle par defaut : les motifs
    // du gabarit installe sont exactement ceux du profil historique.
    assert(benin.numberPattern == "^[0-9]+$");
}

} // namespace

int main() {
    checkFallsBackToDefaults();
    checkReadsInstalledProfile();
    checkProfileReplacesPatterns();
    checkIgnoredProfilesKeepDefaults();
    checkRuleAppliesLoadedProfile();
    return 0;
}
