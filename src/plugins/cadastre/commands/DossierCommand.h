#pragma once

#include "bcad/commands/Command.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::cadastre {

// « Cartouche du dossier » : prepare dans les attributs du dessin les champs
// que le cartouche du plan attend (projet, geometre, date, dossier…), vides,
// avec leur libelle. L'operateur les remplit dans la palette Proprietes de
// l'hote, sans rien selectionner ; le plan cadastral les reprend.
//
// Une valeur deja saisie n'est jamais ecrasee : la commande peut etre relancee
// (apres un rechargement, ou elle redonne leurs libelles aux champs). Annuler
// retire seulement les champs qu'elle a crees.
class EditDossierCommand final : public commands::Command {
public:
    std::string_view text() const override { return "cadastre.edit_dossier"; }
    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;
    std::unique_ptr<commands::Command> clone() const override;

private:
    std::vector<std::string> crees_;
};

std::unique_ptr<commands::Command> makeEditDossier(const std::vector<std::string>& args);

} // namespace bcad::cadastre
