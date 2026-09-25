#pragma once

#include "bcad/layout/Borne.h"
#include "bcad/layout/Cartouche.h"
#include "bcad/layout/Label.h"
#include "bcad/layout/ParcelTable.h"
#include <vector>

namespace bcad::core {
class Document;
}

namespace bcad::cadastre {

// Meuble de feuille construit depuis les entités cadastrales.
// Les noms de propriété `cadastre.*` vivent ici : src/layout et src/app ignorent
// ce qu'est une parcelle (ADR-016) et se contentent de dessiner ce qu'on leur
// fournit. Un document sans parcelle produit un meuble vide, donc une feuille
// sans tableau ni étiquette.
struct SheetFurniture {
    std::vector<layout::Label> labels;
    std::vector<layout::Borne> bornes;
    layout::ParcelTable table;
};

SheetFurniture buildSheetFurniture(const core::Document& document);

// Le cartouche du document : ce que le dossier porte, plus ce que les parcelles
// ont en commun. Le module nomme les clés d'attribut, l'hôte ne les connaît pas.
//
// Deux règles de résolution, pas une de plus :
//
//   - un champ d'abord cherché dans les attributs du dossier
//     (`cadastre.dossier.<nom>`), où l'opérateur l'a saisi ;
//   - à défaut, dans les parcelles de la feuille — mais seulement si TOUTES les
//     parcelles disent la même chose. La valeur de la première parcelle lue
//     n'est pas une information sur la feuille, c'est un artefact de l'ordre
//     de stockage : la rendre visible ferait porter au livrable une affirmation
//     que personne n'a saisie.
//
// Aucun champ n'est inventé faute de saisie, et `echelle` n'est pas résolu ici :
// c'est la composition qui le pose.
layout::Cartouche buildCartouche(const core::Document& document);

} // namespace bcad::cadastre
