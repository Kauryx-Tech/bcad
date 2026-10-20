#pragma once

#include "bcad/commands/Command.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bcad::cadastre {

// « Riverains et présence… » (K-08) : prepare, cote par cote, les attributs
// des parcelles designees que le calcul ne peut pas fournir — la presence du
// riverain au bornage et le nom d'un riverain non leve. L'operateur les
// remplit dans la palette Proprietes de l'hote, la parcelle selectionnee ; le
// tableau des riverains et le PV (K-07) les reprennent.
//
// Comme le cartouche (EditDossierCommand) : une saisie n'est jamais ecrasee,
// la commande peut etre relancee (elle redonne leurs libelles, numeros de
// borne a jour, et ajoute les cotes nouveaux) ; annuler retire seulement ce
// qu'elle a cree. Un cote est repere par son rang dans le contour : inserer
// un sommet decale les saisies des cotes suivants.
class EditLimitesCommand final : public commands::Command {
public:
    explicit EditLimitesCommand(std::vector<int> ids) : ids_(std::move(ids)) {}

    std::string_view text() const override { return "cadastre.edit_limites"; }
    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;
    std::unique_ptr<commands::Command> clone() const override;

private:
    std::vector<int> ids_;
    std::vector<std::pair<int, std::string>> crees_;   // (id de parcelle, cle)
};

// Arguments : les ids des entites selectionnees ; au moins une parcelle.
std::unique_ptr<commands::Command> makeEditLimites(const std::vector<std::string>& args);

} // namespace bcad::cadastre
