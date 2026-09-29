#pragma once

// Le peintre des meubles déclaratifs (ADR-017, décision 7) : l'hôte peint des
// libellés qu'il ne comprend pas, depuis un gabarit que le module déclare.
//
// Deux peintres, deux formes de meuble :
//   - `drawFurniture` : grille « libellé : valeur », champs dans l'ordre des
//     slots, sur autant de colonnes que le gabarit en demande ;
//   - `drawFurnitureTable` : tableau à en-têtes (`columnLabels` du gabarit) et
//     cellules rangées par slot en lignes de `columns` — nomenclature,
//     signatures, même combat.
//
// Le seul indice de format que l'hôte honore est « image » : la valeur chaîne
// est un chemin d'image, chargée avec son rapport conservé, traitée comme
// absente si illisible (elle ne fait jamais échouer l'export). Tout autre
// format (« date », « surface », « pression-bar »…) est imprimé tel quel et
// gardé tel quel : l'hôte n'a pas à le comprendre pour le rendre.
//
// Comme le tableau des signatures d'avant : le meuble ne déborde jamais de sa
// zone — ce qui dépasse est tronqué et dit (« +N »), pas rogné en silence.
//

#include "bcad/layout/Furniture.h"
#include "bcad/layout/FurnitureTemplate.h"

#include <string>
#include <vector>

class QPainter;
class QRectF;
class QString;

namespace bcad::layout {

// Un meuble prêt à peindre : son gabarit (zone, grille, apparence) et ses
// champs résolus (valeurs). La résolution (`resolveFields`) est ailleurs :
// peindre n'est pas trancher.
struct ResolvedFurniture {
    FurnitureTemplate gabarit;
    std::vector<Field> fields;
};

// La valeur d'un champ en texte à peindre. Le vocabulaire est celui de
// `value_json`, pas celui d'un pays : un booléen dit « true »/« false », un
// enum hors de son domaine dit son index plutôt qu'un libellé deviné.
std::string formatFieldValue(const Field& champ);

void drawFurniture(QPainter& painter, const QRectF& rect, const ResolvedFurniture& meuble);
void drawFurnitureTable(QPainter& painter, const QRectF& rect, const ResolvedFurniture& meuble);

// Le texte au millimètre de feuille (partagé avec le reste du peintre) :
// QFont::setPixelSize est une unité utilisateur — donc un millimètre — et rend
// à la même taille quelle que soit la résolution du périphérique ; setPointSize
// serait multiplié par le dpi logique (1200 dpi : glyphes seize fois trop
// grands, vérifié). Repère intermédiaire au 1/16 mm pour les tailles
// fractionnaires.
void drawTextMm(QPainter& painter, const QRectF& rect, int flags, const QString& text,
                const QString& family, double heightMm, bool bold = false);

} // namespace bcad::layout
