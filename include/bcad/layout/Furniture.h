#pragma once

// Le meuble de mise en page et son champ declaratif (ADR-017 decision 3 et 7).
//
// Un meuble est un couple « nature + zone + champs ordonnés ». Sa nature est un
// nom libre — « cadastre.cartouche », « reseau.legende » — rangé par l'hôte sans
// jamais être compris par lui, exactement comme le `type_id` d'une entité (ADR-004).
// Un champ est la pièce atomique du vocabulaire : l'hôte peint son libellé, forme sa
// valeur selon son indice, et ne nomme aucun des trois.
//
// C'est la fin des vingt-deux membres C++ de `Cartouche` comme seule facon
// d'exprimer un cartouche. `Cartouche` reste en service jusqu'a ce que le peintre
// sache faire sans lui ; il n'est plus le modele.

#include "bcad/layout/GeometryMm.h"
#include "bcad/properties/PropertyTypes.h"

#include <string>
#include <vector>

namespace bcad::layout {

// Une valeur et son type, plus le domaine d'un enum : le domaine voyage avec la
// valeur, sans quoi un index recu d'un module absent serait hors domaine et
// donc perdu. C'est la forme que `value_json` sait deja rendre (src/io).
struct TypedValue {
    properties::PropertyType type = properties::PropertyType::String;
    properties::PropertyValue value{std::string{}};
    std::vector<std::string> enumValues;
};

// Un champ déclaratif. `role` est la nature du champ telle que le module la nomme
// (« attribut », « calculé », « littéral ») : l'hôte la recopie, ne l'interprète
// pas. `format` est un indice de formatage (« date », « surface ») laissé libre :
// ce qu'il ne connait pas, il l'imprime tel quel et le garde tel quel.
struct Field {
    std::string role;
    std::string label;
    std::string key;
    std::string format;
    int slot = 0;
    TypedValue value;

    // Un libellé sans valeur se peint vide et se signale : il ne disparait pas.
    // Seule une chaine vide compte comme absence — un zero, un faux, un index
    // d'enum a zero sont des valeurs.
    bool hasValue() const {
        if (const auto* texte = std::get_if<std::string>(&value.value)) return !texte->empty();
        return true;
    }
};

// Un meuble de la feuille : sa nature, la zone papier qu'il occupe, et ses champs
// dans l'ordre ou ils doivent etre peints.
class Furniture {
public:
    Furniture() = default;
    explicit Furniture(std::string nature) : nature_(std::move(nature)) {}

    const std::string& nature() const { return nature_; }
    void setNature(std::string nature) { nature_ = std::move(nature); }

    // Le gabarit que ce meuble attend, s'il en a un. Vide = le meuble porte deja
    // ses champs, et l'hôte n'a rien a resoudre.
    const std::string& templateId() const { return templateId_; }
    void setTemplateId(std::string id) { templateId_ = std::move(id); }

    // Zone occupee sur la feuille. Nulle = c'est a la composition de la reserver,
    // d'apres le gabarit si le meuble en nomme un.
    const RectMm& zone() const { return zone_; }
    void setZone(const RectMm& zone) { zone_ = zone; }

    void addField(Field field) { fields_.push_back(std::move(field)); }
    const std::vector<Field>& fields() const { return fields_; }
    std::vector<Field>& fields() { return fields_; }

    // Un meuble a quelque chose a montrer des qu'un champ est nomme : reserver sa
    // zone ne doit pas dependre duquel d'entre eux, qui est une donnee de module.
    bool isContentBearing() const { return !fields_.empty(); }

private:
    std::string nature_;
    std::string templateId_;
    RectMm zone_;
    std::vector<Field> fields_;
};

} // namespace bcad::layout
