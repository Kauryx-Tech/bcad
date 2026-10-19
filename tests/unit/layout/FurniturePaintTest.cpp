// Le formatage des champs déclaratifs : l'hôte peint des valeurs qu'il ne
// comprend pas, dans le vocabulaire de `value_json` — pas celui d'un pays.

#include "bcad/layout/FurniturePaint.h"
#include <cassert>
#include <cmath>
#include <string>
#include <variant>

using namespace bcad::layout;
using namespace bcad::properties;

namespace {

Field champ(TypedValue valeur) {
    Field f;
    f.label = "Champ";
    f.value = std::move(valeur);
    return f;
}

TypedValue texte(const std::string& s) {
    TypedValue v;
    v.type = PropertyType::String;
    v.value = s;
    return v;
}

} // namespace

int main() {
    assert(formatFieldValue(champ(texte("Lome"))) == "Lome");

    TypedValue reel;
    reel.type = PropertyType::Double;
    reel.value = 1250.42;
    assert(formatFieldValue(champ(reel)) == "1250.42");

    TypedValue entier;
    entier.type = PropertyType::Int;
    entier.value = 3;
    assert(formatFieldValue(champ(entier)) == "3");

    TypedValue booleen;
    booleen.type = PropertyType::Bool;
    booleen.value = true;
    // « true », pas « oui » : le vocabulaire est celui du format, pas d'un
    // pays — aucun mot d'aucune langue dans le peintre de l'hôte.
    assert(formatFieldValue(champ(booleen)) == "true");

    TypedValue enumeration;
    enumeration.type = PropertyType::Enum;
    enumeration.value = EnumIndex{1};
    enumeration.enumValues = {"brouillon", "valide", "archive"};
    assert(formatFieldValue(champ(enumeration)) == "valide");

    // Un index hors domaine ne devine pas de libellé : il dit son index.
    TypedValue horsDomaine;
    horsDomaine.type = PropertyType::Enum;
    horsDomaine.value = EnumIndex{7};
    horsDomaine.enumValues = {"brouillon", "valide"};
    assert(formatFieldValue(champ(horsDomaine)) == "#7");

    // Un meuble a quelque chose à montrer dès qu'un champ est nommé.
    Furniture meuble("profil.cartouche");
    assert(!meuble.isContentBearing());
    meuble.addField(champ(texte("x")));
    assert(meuble.isContentBearing());

    // Hauteur d'un tableau : en-tete + lignes, pour empiler les tableaux de la
    // feuille sans recouvrement (nomenclature et coordonnees se superposaient).
    ResolvedFurniture tableau;
    tableau.gabarit.columns = 2;
    tableau.gabarit.fontSizeMm = 2.0;
    tableau.gabarit.columnLabels = {"A", "B"};
    assert(furnitureTableHeight(tableau) == 3.6);              // en-tete seul
    for (int i = 0; i < 5; ++i) {
        Field f = champ(texte("v"));
        f.slot = i;
        tableau.fields.push_back(f);
    }
    assert(std::abs(furnitureTableHeight(tableau) - (3.6 + 3 * 4.0)) < 1e-9);   // 5 champs = 3 lignes
    assert(furnitureTableHeight(ResolvedFurniture{}) == 0.0);

    return 0;
}
