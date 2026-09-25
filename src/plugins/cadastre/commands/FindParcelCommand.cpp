// Recherche par reference cadastrale (F4). La regle de rapprochement entre la
// saisie de l'utilisateur et les proprietes de la parcelle vit ici, pas dans
// l'hote : src/app ne demande qu'une chaine et affiche un compte.
//
// La recherche ne modifie pas le dessin, seulement la selection : la commande
// reste annulable (elle retablit la selection precedente) mais l'hote ne la
// pousse pas dans la pile d'annulation et ne marque pas le document modifie
// (WorkbenchAction::modifiesDocument).

#include "FindParcelCommand.h"

#include "../entities/ParcelEntity.h"
#include "bcad/core/Document.h"
#include "bcad/properties/PropertyMap.h"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace bcad::cadastre {

namespace {

bool isSeparator(char c) {
    return c == ' ' || c == '\t' || c == '|' || c == '-' || c == '/' || c == '.';
}

} // namespace

ParcelReference parseParcelReference(std::string_view raw) {
    ParcelReference reference;
    std::size_t i = 0;
    while (i < raw.size() && isSeparator(raw[i])) ++i;

    std::string section;
    for (; i < raw.size(); ++i) {
        const char c = raw[i];
        if (std::isdigit(static_cast<unsigned char>(c))) break;
        // Le separateur ne termine pas la section : « A 7 » est une reference
        // complete, pas une section suivie d'un reste invalide.
        if (isSeparator(c)) continue;
        if (!std::isalpha(static_cast<unsigned char>(c))) return {};
        section.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    std::string numero;
    for (; i < raw.size(); ++i) {
        const char c = raw[i];
        if (isSeparator(c)) continue;
        if (!std::isdigit(static_cast<unsigned char>(c))) return {};
        numero.push_back(c);
    }

    const std::size_t firstSignificant = numero.find_first_not_of('0');
    if (firstSignificant == std::string::npos)
        reference.numero = numero.empty() ? std::string() : std::string("0");
    else
        reference.numero = numero.substr(firstSignificant);
    reference.section = std::move(section);
    return reference;
}

bool FindParcelCommand::matches(const geom::Entity& entity) const {
    if (!isCadastreParcel(&entity)) return false;
    const auto& properties = entity.properties();
    if (!reference_.section.empty()
        && parseParcelReference(properties.getString("cadastre.section")).section
               != reference_.section)
        return false;
    if (!reference_.numero.empty()
        && parseParcelReference(properties.getString("cadastre.numero")).numero
               != reference_.numero)
        return false;
    return true;
}

void FindParcelCommand::execute(core::Document& doc) {
    // La recherche remplace la selection : ajouter aux results ce qui etait
    // deja selectionne laisserait un compte incapable d'expliquer la vue.
    if (!selectionSaved_) {
        for (const auto& entity : doc.entities())
            if (entity->selected) previousSelection_.push_back(entity->id());
        selectionSaved_ = true;
    }
    matchedIds_.clear();
    for (const auto& entity : doc.entities()) {
        const bool hit = matches(*entity);
        entity->selected = hit;
        if (hit) matchedIds_.push_back(entity->id());
    }
}

void FindParcelCommand::undo(core::Document& doc) {
    for (const auto& entity : doc.entities()) entity->selected = false;
    for (int id : previousSelection_) {
        if (auto* entity = doc.findEntity(id)) entity->selected = true;
    }
    matchedIds_.clear();
}

std::unique_ptr<commands::Command> FindParcelCommand::clone() const {
    return std::make_unique<FindParcelCommand>(reference_);
}

std::unique_ptr<commands::Command> makeFindParcel(const std::vector<std::string>& args) {
    if (args.size() != 1) return nullptr;
    const ParcelReference reference = parseParcelReference(args[0]);
    // Saisie vide ou sans ni section ni numero exploitable : rien a chercher.
    if (reference.isEmpty()) return nullptr;
    return std::make_unique<FindParcelCommand>(reference);
}

} // namespace bcad::cadastre
