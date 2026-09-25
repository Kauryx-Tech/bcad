#pragma once

#include "bcad/layout/Borne.h"
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

} // namespace bcad::cadastre
