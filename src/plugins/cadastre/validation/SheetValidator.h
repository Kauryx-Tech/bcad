#pragma once

// Mise en page du dossier (ADR-017, décision 5) : la validité d'une feuille est
// une validation, pas une exception — et ce n'est pas une propriété d'un lot
// d'entités, c'est une propriété du document. D'où `IDocumentValidator` : la
// règle voit les feuilles, leurs vues et les attributs du dossier.
//
// Deux constats, pas un de plus :
//   - une vue qui déborde de sa feuille est une ERREUR : le livrable est faux,
//     le plan est rogné ou hors papier ;
//   - une échelle hors de la liste du profil est un AVERTISSEMENT : le plan
//     s'imprime, mais le livrable n'est pas conforme au règlement national.
//
// Le profil en vigueur est celui que le dossier désigne
// (`cadastre.dossier.profil`), lu dans les données installées comme la
// commande `cadastre.set_profile` ; à défaut, les valeurs du module
// (`defaultPermittedScales`). Le libellé nomme le profil appliqué, pour que le
// décalage « un autre dossier, un autre règlement » se lise au lieu de se
// deviner — même règle que `ParcelIdentifierRuleValidator`.

#include "bcad/plugin/Validator.h"

#include <string>
#include <vector>

namespace bcad::cadastre {

class SheetValidator : public plugin::IDocumentValidator {
public:
    std::string id() const override;
    std::string label() const override;
    std::vector<validation::Diagnostic> validateDocument(
        const core::Document& document) const override;
};

} // namespace bcad::cadastre
