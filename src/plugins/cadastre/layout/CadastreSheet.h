#pragma once

#include "bcad/layout/Borne.h"
#include "bcad/layout/FurniturePaint.h"
#include "bcad/layout/FurnitureTemplate.h"
#include "bcad/layout/Label.h"
#include "bcad/validation/Diagnostics.h"
#include <string>
#include <vector>

namespace bcad::core {
class Document;
}

namespace bcad::cadastre {

// Meuble de feuille construit depuis les entités cadastrales.
// Les noms de propriété `cadastre.*` vivent ici : src/layout et src/app ignorent
// ce qu'est une parcelle (ADR-016) et se contentent de peindre des libellés
// qu'ils ne comprennent pas. Un document sans parcelle produit un meuble vide,
// donc une feuille sans tableau ni étiquette.
struct SheetFurniture {
    std::vector<layout::Label> labels;
    std::vector<layout::Borne> bornes;
};

SheetFurniture buildSheetFurniture(const core::Document& document);

// Le cartouche du document, résolu depuis un gabarit : ce que le dossier porte,
// plus ce que les parcelles ont en commun. Le module nomme les clés d'attribut,
// l'hôte ne les connaît pas.
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
// Un champ à clé vide porte son littéral (l'échelle « 1:n », posée par
// l'appelant une fois la composition connue). Ce que le gabarit attend et ne
// trouve pas ressort sans valeur ET avec un diagnostic nommant la clé : la
// valeur manquante est un constat, jamais une disparition silencieuse.
//
// `echelle` n'est pas résolu ici : c'est la composition qui la choisit, et un
// cartouche dont la seule donnée serait une échelle ne doit pas apparaître sur
// une feuille où l'opérateur n'a rien saisi.
layout::ResolvedFurniture buildCartoucheFurniture(
    const core::Document& document, const layout::FurnitureTemplate& gabarit,
    std::vector<validation::Diagnostic>& diagnostics);

// La nomenclature des parcelles de la feuille, en champs rangés par lignes de
// `gabarit.columns` : ce que le gabarit nomme en en-têtes, le module le range.
// Dernière ligne : le total des surfaces calculées. Le gabarit par défaut est
// `defaultNomenclatureTemplate()`, le profil national peut le remplacer.
layout::ResolvedFurniture buildNomenclatureFurniture(
    const core::Document& document, const layout::FurnitureTemplate& gabarit);

// Le tableau des signatures, lu dans les attributs du dossier
// (`cadastre.dossier.signature.<i>.nom/.role/.date/.image`). Un dossier sans
// signatures donne un meuble vide : la feuille n'en réserve ni n'en peint.
// Le champ image porte le format « image » — le seul indice que l'hôte honore.
layout::ResolvedFurniture buildSignaturesFurniture(
    const core::Document& document, const layout::FurnitureTemplate& gabarit);

// Gabarits par défaut du module, quand le profil national n'en donne pas (ou
// pas encore) : le module vérifie et peint exactement comme avant la mise en
// service des gabarits de profil. Les libellés français sont du vocabulaire
// déclaré du module, pas des constantes de l'hôte (ADR-017).
layout::FurnitureTemplate defaultCartoucheTemplate();
layout::FurnitureTemplate defaultNomenclatureTemplate();
layout::FurnitureTemplate defaultSignaturesTemplate();

// Échelles admises par défaut (1:n) quand le profil n'en donne aucune.
// Donnée du module, pas de l'hôte : `Scale.h` ne nomme plus aucune liste.
std::vector<int> defaultPermittedScales();

} // namespace bcad::cadastre
