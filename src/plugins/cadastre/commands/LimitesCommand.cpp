// Les cles, les valeurs de presence et les libelles sont du module (ADR-016) :
// l'hote ne fait que montrer la PropertyMap de la parcelle selectionnee.

#include "LimitesCommand.h"

#include "../entities/ParcelEntity.h"
#include "../layout/CadastreSheet.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

#include <charconv>

namespace bcad::cadastre {

void EditLimitesCommand::execute(core::Document& doc) {
    crees_.clear();
    for (const int id : ids_) {
        auto* entity = doc.findEntity(id);
        if (!isCadastreParcel(entity)) continue;
        auto& parcelle = static_cast<geom::PolylineEntity&>(*entity);
        auto& props = parcelle.properties();
        const auto cotes = sideBornes(doc, parcelle);
        for (std::size_t i = 0; i < cotes.size(); ++i) {
            const std::string cote = "Côté " + cotes[i].first + " - " + cotes[i].second;

            const std::string presence = limiteKey(i, "presence");
            properties::Property* p = props.get(presence);
            if (!p) {
                p = props.addEnum(presence, 0, presenceValues());
                crees_.emplace_back(id, presence);
            }
            // Le libelle n'est pas enregistre avec la valeur : il est redonne a
            // chaque passage, avec les numeros de borne du moment.
            if (p) p->setDescription(cote + " · présence au bornage");

            const std::string riverain = limiteKey(i, "riverain");
            p = props.get(riverain);
            if (!p) {
                p = props.addString(riverain, "");
                crees_.emplace_back(id, riverain);
            }
            if (p) p->setDescription(cote + " · riverain non levé");
        }
    }
}

void EditLimitesCommand::undo(core::Document& doc) {
    for (const auto& [id, cle] : crees_)
        if (auto* entity = doc.findEntity(id)) entity->properties().remove(cle);
    crees_.clear();
}

std::unique_ptr<commands::Command> EditLimitesCommand::clone() const {
    return std::make_unique<EditLimitesCommand>(ids_);
}

std::unique_ptr<commands::Command> makeEditLimites(const std::vector<std::string>& args) {
    if (args.empty()) return nullptr;
    std::vector<int> ids;
    for (const auto& arg : args) {
        int id = 0;
        const auto [fin, erreur] = std::from_chars(arg.data(), arg.data() + arg.size(), id);
        if (erreur != std::errc{} || fin != arg.data() + arg.size()) return nullptr;
        ids.push_back(id);
    }
    return std::make_unique<EditLimitesCommand>(std::move(ids));
}

} // namespace bcad::cadastre
