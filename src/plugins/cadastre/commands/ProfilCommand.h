#pragma once

#include "../Templates.h"

#include "bcad/commands/Command.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bcad::cadastre {

// L'attribut du dossier qui designe le profil cadastral. Le nom de la cle est du
// vocabulaire du module ; l'hote porte le conteneur, il ne nomme aucun champ
// (ADR-016 §4, ADR-017).
inline constexpr const char* kCleProfilDossier = "cadastre.dossier.profil";

// Le profil que l'operateur designe pour son dossier. Deux choses changent, et
// elles doivent changer ensemble :
//
//   - l'attribut du dossier, qui est la donnee saisie — celle que la feuille
//     portera quand le layout sera un objet du document (ADR-017, tranche 2) ;
//   - les motifs appliques par la regle d'identification, qui est l'effet de
//     cette saisie sur la verification.
//
// Annuler la commande defait les deux : un dossier dont Ctrl+Z a retire le
// profil ne doit pas continuer d'etre verifie contre les regles du pays qu'il
// vient de quitter.
class SetProfileCommand final : public commands::Command {
public:
    explicit SetProfileCommand(CadastreTemplates gabarit) : gabarit_(std::move(gabarit)) {}

    std::string_view text() const override { return "cadastre.set_profile"; }

    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;

    std::unique_ptr<commands::Command> clone() const override;

    const std::string& code() const { return gabarit_.profile; }

private:
    CadastreTemplates gabarit_;
    // L'etat trouve au premier execute(), remis par undo(). Il n'est pas pris a
    // la construction : une commande peut etre annulee puis refaite, et chaque
    // fois l'etat a retrouver est celui du moment.
    std::string codePrecedent_;
    bool attributPresent_ = false;
    std::optional<CadastreTemplates> gabaritPrecedent_;
    bool saisiePrise_ = false;
};

// Factory. Un code qui n'est pas un nom, ou un nom sans gabarit installe, est
// refuse ici : l'hote traduit un refus en message d'indisponibilite, et ne
// retient aucune regle metier pour le faire a la place du module.
std::unique_ptr<commands::Command> makeSetProfile(const std::vector<std::string>& args);

} // namespace bcad::cadastre
