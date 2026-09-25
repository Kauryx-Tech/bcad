#pragma once

#include "bcad/commands/Command.h"
#include "bcad/geometry/Entity.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bcad::cadastre {

// Référence cadastrale telle qu'un utilisateur la saisit : « A 007 », « A|7 »,
// « A-7 », « A7 ». La section est la partie alphabétique, le numero la partie
// numerique ; l'une des deux peut manquer (recherche dans toute la section, ou
// dans toutes les sections), jamais les deux.
struct ParcelReference {
    std::string section;   // majuscules, separateurs retires
    std::string numero;    // chiffres, zeros de tete retires

    bool isEmpty() const { return section.empty() && numero.empty(); }
};

// Reference invalide (lettres apres des chiffres, numero non numerique) ou
// vide : `isEmpty()` est vrai et le demandeur doit abandonner.
ParcelReference parseParcelReference(std::string_view raw);

class FindParcelCommand final : public commands::Command {
public:
    explicit FindParcelCommand(ParcelReference reference) : reference_(reference) {}

    std::string_view text() const override { return "cadastre.find_parcel"; }

    // Remplace la selection par les parcelles correspondant a la reference.
    void execute(core::Document& doc) override;
    // Retablit la selection telle qu'elle etait avant la recherche.
    void undo(core::Document& doc) override;

    std::unique_ptr<commands::Command> clone() const override;

    const std::vector<int>& matchedIds() const { return matchedIds_; }

private:
    bool matches(const geom::Entity& entity) const;

    ParcelReference reference_;
    std::vector<int> previousSelection_;
    std::vector<int> matchedIds_;
    bool selectionSaved_ = false;
};

std::unique_ptr<commands::Command> makeFindParcel(const std::vector<std::string>& args);

} // namespace bcad::cadastre
