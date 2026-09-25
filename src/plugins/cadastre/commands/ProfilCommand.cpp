// Le profil cadastral du dossier, choisi par l'operateur (ADR-017, tranche 1).
// L'hote ne fait que demander une chaine — WorkbenchParams::PromptText — et ne
// sait pas ce qu'un profil signifie : le nom de la cle d'attribut, la forme du
// code et l'effet sur la verification sont ecrits ici.

#include "ProfilCommand.h"

#include "../Templates.h"
#include "../validation/CadastreValidators.h"

#include "bcad/core/Document.h"
#include "bcad/plugin/Validator.h"
#include "bcad/properties/PropertyMap.h"

#include <optional>
#include <utility>

namespace bcad::cadastre {

namespace {

// La regle d'identification est enregistree chez l'hote a l'initialisation du
// module. Elle se cherche par son identifiant, et le `dynamic_cast` est ce qui
// distingue « ma regle » de « un autre module a pris ce nom » : sans lui, un
// reglement de profil se deverserait dans la regle d'un tiers.
const ParcelIdentifierRuleValidator* regleIdentification() {
    const plugin::IValidator* regle =
        plugin::ValidatorRegistry::instance().find("cadastre.identification");
    return dynamic_cast<const ParcelIdentifierRuleValidator*>(regle);
}

} // namespace

void SetProfileCommand::execute(core::Document& doc) {
    auto& dossier = doc.properties();
    const ParcelIdentifierRuleValidator* regle = regleIdentification();

    if (!saisiePrise_) {
        codePrecedent_ = dossier.getString(kCleProfilDossier);
        attributPresent_ = dossier.has(kCleProfilDossier);
        if (regle) gabaritPrecedent_ = regle->profilEnVigueur();
        saisiePrise_ = true;
    }

    dossier.setString(kCleProfilDossier, gabarit_.profile);
    // Une regle absente — module charge sans son validateur, ou nom d'identifiant
    // deja pris — laisse la saisie au dossier sans effet sur la verification.
    // L'attribut est la donnee de l'operateur ; la regle est son effet, pas
    // l'inverse.
    if (regle) regle->appliquerProfil(gabarit_);
}

void SetProfileCommand::undo(core::Document& doc) {
    if (!saisiePrise_) return;
    auto& dossier = doc.properties();

    if (attributPresent_) dossier.setString(kCleProfilDossier, codePrecedent_);
    else dossier.remove(kCleProfilDossier);

    if (const auto* regle = regleIdentification(); gabaritPrecedent_ && regle)
        regle->appliquerProfil(*gabaritPrecedent_);

    saisiePrise_ = false;
    gabaritPrecedent_.reset();
}

std::unique_ptr<commands::Command> SetProfileCommand::clone() const {
    return std::make_unique<SetProfileCommand>(gabarit_);
}

std::unique_ptr<commands::Command> makeSetProfile(const std::vector<std::string>& args) {
    if (args.size() != 1) return nullptr;
    // Un code refuse ici se traduit par une factory rendue nullptr, que l'hote
    // affiche comme une commande indisponible : c'est le seul message que
    // l'hote sait donner sans connaitre le metier.
    const std::string& code = args.front();
    if (!estCodeDeProfilValide(code)) return nullptr;

    const CadastreTemplates gabarit = chargerGabaritDuProfil(code);
    // Un profil dont le gabarit n'est pas installe n'est pas un profil :
    // l'enregistrer ferait porter au dossier le nom d'un reglement que le
    // module n'a pas lu.
    if (gabarit.source.empty()) return nullptr;

    return std::make_unique<SetProfileCommand>(gabarit);
}

} // namespace bcad::cadastre
