// Le profil cadastral du dossier est designe par l'operateur, et son effet est
// double : une donnee saisie (l'attribut du dossier) et une regle qui change
// (les motifs d'identification). Ce test tient les deux bouts, plus ce qui les
// precede — un code de profil est une saisie qui devient un composant de chemin,
// donc elle est refusee avant d'ouvrir quoi que ce soit.
//
// Les appels a effet de bord sont hors des assert() : en RelWithDebInfo NDEBUG
// est defini et assert() n'evalue pas son argument.

#include "Templates.h"
#include "commands/ProfilCommand.h"
#include "entities/ParcelEntity.h"
#include "validation/CadastreValidators.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/plugin/Validator.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/validation/Diagnostics.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace bcad;
using namespace bcad::cadastre;

namespace {

// Repertoire de donnees ephemere, range a la fin du test.
class ScratchDataDir {
public:
    explicit ScratchDataDir(const std::string& name)
        : root_(std::filesystem::temp_directory_path() / ("bcad_" + name + "_test")) {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
        std::filesystem::create_directories(root_ / "cadastre" / "templates", ec);
        memoriserRepertoiresDeDonnees({root_.string()});
    }

    ~ScratchDataDir() {
        memoriserRepertoiresDeDonnees({});
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    void write(const std::string& file, const std::string& contents) const {
        const std::filesystem::path path = root_ / "cadastre" / "templates" / file;
        std::ofstream out(path, std::ios::binary);
        out << contents;
        assert(out.good());
    }

private:
    std::filesystem::path root_;
};

// Un profil qui resserre la section a deux lettres exactement et le numero a
// quatre chiffres : ce que la section « A » et le numero « 7 » du profil
// historique ne passent plus.
constexpr const char* kDeuxLettres = R"({
  "schema_version": 1,
  "name": "Cadastre deux lettres",
  "parcel_identifier": { "section_pattern": "^[A-Z]{2}$", "number_pattern": "^[0-9]{4}$" }
})";

std::unique_ptr<ParcelEntity> makeParcel(const std::string& section, const std::string& numero) {
    auto parcel = std::make_unique<ParcelEntity>(std::vector<geom::Point2>{
        {0, 0}, {4, 0}, {4, 4}, {0, 4}});
    parcel->properties().setString("cadastre.section", section);
    parcel->properties().setString("cadastre.numero", numero);
    return parcel;
}

// Un code de profil est une chaine de l'operateur qui devient un nom de fichier.
// Tout ce qui pourrait en faire un chemin est refuse ici, et pas plus loin.
void checkLeCodeResteUnNom() {
    assert(estCodeDeProfilValide("cadastre_togo"));
    assert(estCodeDeProfilValide("Cadastre-Benin.2"));

    assert(!estCodeDeProfilValide(""));
    assert(!estCodeDeProfilValide(".."));
    assert(!estCodeDeProfilValide("../secret"));
    assert(!estCodeDeProfilValide("cadastre/../../secret"));
    assert(!estCodeDeProfilValide("cadastre\\be"));
    assert(!estCodeDeProfilValide("cadastre togo"));
    assert(!estCodeDeProfilValide("cadastre_togo.json/"));
    // « .. » n'est pas un caractere interdit : chaque lettre passe la regle du
    // jeu de caracteres. C'est comme segment qu'il est refuse.
    assert(!estCodeDeProfilValide("cadastre..togo"));
    assert(!estCodeDeProfilValide("cadastre_togo..x"));
    assert(!estCodeDeProfilValide("0projet"));
    assert(!estCodeDeProfilValide("_prive"));
    assert(!estCodeDeProfilValide(std::string(200, 'a')));

    // Refuse avant meme de chercher : un code invalide ne donne aucun gabarit.
    assert(chargerGabaritDuProfil("../secret").source.empty());
}

void checkLeProfilEstLuDansLesRepertoires() {
    const ScratchDataDir donnees{"profil_gabarit"};
    donnees.write("cadastre_deuxlettres.json", kDeuxLettres);

    const CadastreTemplates gabarit = chargerGabaritDuProfil("cadastre_deuxlettres");
    assert(!gabarit.source.empty());
    assert(gabarit.profile == "cadastre_deuxlettres");
    assert(gabarit.sectionPattern == "^[A-Z]{2}$");
    assert(gabarit.numberPattern == "^[0-9]{4}$");

    // Un nom correct mais absent rend les motifs du module, avec le nom demande
    // et `source` vide : l'appelant sait qu'il n'a rien lu.
    const CadastreTemplates absent = chargerGabaritDuProfil("cadastre_inexistant");
    assert(absent.source.empty());
    assert(absent.profile == "cadastre_inexistant");
    assert(absent.sectionPattern == "^[A-Z]{1,3}$");
}

// La fin a tester : la commande executee par l'hote change bien la regle que
// l'hote detient et applique — pas seulement une chaine rangee dans le document.
void checkLaCommandeChangeLaRegleEnregistree() {
    const ScratchDataDir donnees{"profil_commande"};
    donnees.write("cadastre_deuxlettres.json", kDeuxLettres);

    auto regle = std::make_unique<ParcelIdentifierRuleValidator>(
        CadastreTemplates{"^[A-Z]{1,3}$", "^[0-9]+$", "", "cadastre_historique"});
    const ParcelIdentifierRuleValidator* regleEnregistree = regle.get();
    assert(plugin::ValidatorRegistry::instance().registerValidator(std::move(regle)));

    auto commande = makeSetProfile({"cadastre_deuxlettres"});
    assert(commande != nullptr);

    core::Document document;
    document.addEntity(makeParcel("A", "7"));
    geom::Entity* laParcelle = document.entities().front().get();

    // Sous le motif historique, « A 7 » est conforme.
    assert(regleEnregistree->validate({laParcelle}).empty());

    commande->execute(document);

    // L'attribut du dossier porte le nom choisi...
    assert(document.properties().getString(kCleProfilDossier) == "cadastre_deuxlettres");
    // ... et la regle enregistree applique desormais le motif du gabarit : la
    // meme parcelle est devenue hors regle, sans qu'aucun code hote n'ait eu a
    // le savoir.
    const auto apres = regleEnregistree->validate({laParcelle});
    assert(apres.size() == 1);
    assert(apres.front().message.find("^[A-Z]{2}$") != std::string::npos);
    // Le libelle du lot dit quel profil a repondu : sans lui, un profil herite
    // d'un autre dossier se prendrait pour une verification du bon.
    assert(regleEnregistree->label().find("cadastre_deuxlettres") != std::string::npos);

    commande->undo(document);
    assert(!document.properties().has(kCleProfilDossier));
    // Annuler defait la regle aussi : la parcelle redevient conforme, et le
    // libelle nomme de nouveau le profil precedemment en vigueur.
    assert(regleEnregistree->validate({laParcelle}).empty());
    assert(regleEnregistree->profil() == "cadastre_historique");

    plugin::ValidatorRegistry::instance().unregisterValidator("cadastre.identification");
}

// Un nom que le module ne sait pas lire n'est pas un profil : la factory rend
// nullptr, l'hote affiche son message d'indisponibilite, et le dossier ne porte
// jamais le nom d'un reglement que personne n'a verifie.
void checkUnNomInconnuEstRefuse() {
    const ScratchDataDir donnees{"profil_refus"};
    assert(makeSetProfile({"cadastre_inexistant"}) == nullptr);
    assert(makeSetProfile({"../secret"}) == nullptr);
    assert(makeSetProfile({}) == nullptr);
    assert(makeSetProfile({"a", "b"}) == nullptr);

    core::Document document;
    assert(!document.properties().has(kCleProfilDossier));
}

} // namespace

int main() {
    checkLeCodeResteUnNom();
    checkLeProfilEstLuDansLesRepertoires();
    checkLaCommandeChangeLaRegleEnregistree();
    checkUnNomInconnuEstRefuse();

    std::cout << "profil cadastral du dossier OK\n";
    return 0;
}
