// Les champs du cartouche sont ceux du gabarit que le plan imprime
// (defaultCartoucheTemplate) : une seule liste, la feuille et la saisie ne
// peuvent pas diverger. Les cles et les libelles sont du module (ADR-016) ;
// l'hote ne fait que montrer la PropertyMap du dessin.

#include "DossierCommand.h"

#include "../layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

namespace bcad::cadastre {

void EditDossierCommand::execute(core::Document& doc) {
    auto& dossier = doc.properties();
    crees_.clear();
    for (const auto& champ : defaultCartoucheTemplate().fields) {
        if (champ.role != "attribut" || champ.key.empty()) continue;
        properties::Property* p = dossier.get(champ.key);
        if (!p) {
            p = dossier.addString(champ.key, "");
            crees_.push_back(champ.key);
        }
        // Le libelle n'est pas enregistre avec la valeur : il est redonne a
        // chaque passage, et l'hote l'affiche a la place de la cle.
        if (p) p->setDescription(champ.label);
    }
}

void EditDossierCommand::undo(core::Document& doc) {
    for (const std::string& cle : crees_) doc.properties().remove(cle);
    crees_.clear();
}

std::unique_ptr<commands::Command> EditDossierCommand::clone() const {
    return std::make_unique<EditDossierCommand>();
}

std::unique_ptr<commands::Command> makeEditDossier(const std::vector<std::string>& args) {
    if (!args.empty()) return nullptr;
    return std::make_unique<EditDossierCommand>();
}

} // namespace bcad::cadastre
