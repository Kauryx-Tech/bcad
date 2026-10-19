#include "CrsCommand.h"

#include "../crs/SystemesCoordonnees.h"

#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

namespace bcad::cadastre {

void SetCrsCommand::execute(core::Document& doc) {
    auto& dossier = doc.properties();
    if (!pris_) {
        present_ = dossier.has(kCleSystemeCoordonnees);
        precedent_ = dossier.getString(kCleSystemeCoordonnees);
        pris_ = true;
    }
    if (!dossier.has(kCleSystemeCoordonnees)) dossier.addString(kCleSystemeCoordonnees, "");
    dossier.setString(kCleSystemeCoordonnees, libelle_);
    if (auto* p = dossier.get(kCleSystemeCoordonnees)) p->setDescription("Système de coordonnées");
}

void SetCrsCommand::undo(core::Document& doc) {
    if (!pris_) return;
    if (present_) doc.properties().setString(kCleSystemeCoordonnees, precedent_);
    else doc.properties().remove(kCleSystemeCoordonnees);
    pris_ = false;
}

std::unique_ptr<commands::Command> SetCrsCommand::clone() const {
    return std::make_unique<SetCrsCommand>(libelle_);
}

std::unique_ptr<commands::Command> makeSetCrs(const std::vector<std::string>& args) {
    if (args.size() != 1) return nullptr;
    const auto code = codeEpsgSaisi(args.front());
    if (!code) return nullptr;
    return std::make_unique<SetCrsCommand>(libelleSysteme(*code, systemesCoordonneesInstalles()));
}

} // namespace bcad::cadastre
