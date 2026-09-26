#pragma once

// Le gabarit d'un meuble (ADR-017 decision 7, deuxieme piece) : la mise en page
// qu'un module declare — sa zone, ses lignes, ses colonnes, les champs qu'il
// attend et ou ils se posent.
//
// Ce fichier ne lit rien. La resolution du nom d'un gabarit dans un fichier de
// donnees installees est l'affaire du module, qui connait ses repertoires et son
// vocabulaire (`resolveDataFile`, `src/plugins/cadastre/Templates.cpp`) ; l'hote
// ne recoit qu'un conteneur deja rempli. Publier un parseur ici ferait glisser
// le vocabulaire d'un pays dans l'interface installee — exactement la faute que
// cette decision retracte.

#include "bcad/layout/GeometryMm.h"

#include <string>
#include <vector>

namespace bcad::layout {

// Un champ attendu par le gabarit. `slot` est sa position dans l'ordre de
// peinture, `row`/`column` sa place dans la grille : l'ordre d'un cartouche est
// une donnee du pays, pas l'ordre des membres d'une structure C++.
struct TemplateField {
    std::string role;
    std::string label;
    std::string key;       // cle opaque ou vide : `literal` porte alors la valeur
    std::string literal;
    std::string format;
    int row = 0;
    int column = 0;
    int slot = 0;
};

struct FurnitureTemplate {
    std::string id;        // « cadastre.cartouche.profil_national »
    std::string nature;    // le meuble qu'il met en page
    int rows = 1;
    int columns = 1;
    // En-tetes de colonnes : une nomenclature se nomme elle-meme, l'hote recopie.
    std::vector<std::string> columnLabels;
    std::vector<TemplateField> fields;
    // Bande que le meuble reserve sur la feuille, et l'apparence qu'il demande.
    RectMm reservedZone;
    double borderWidth = 0.5;
    std::string fontName = "Standard";
    double fontSizeMm = 2.5;
    // Chemin du fichier d'ou vient ce gabarit ; vide = gabarit tenu par le module
    // en dur. Un operateur doit pouvoir savoir lequel a repondu.
    std::string source;
};

} // namespace bcad::layout
