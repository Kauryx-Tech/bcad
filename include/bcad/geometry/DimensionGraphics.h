#pragma once

// Dessin d'une cotation (A-01) : lignes d'attache, ligne de cote, fleches et
// emplacement du texte, calcules une fois ici pour le canevas, le PDF, la
// selection et l'emprise. Fonctions libres, sans toucher a la disposition des
// classes de cotation (ABI des modules inchangee).

#include "bcad/geometry/DimensionEntity.h"
#include "bcad/geometry/Point.h"

#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace bcad::geom {

// Hauteur du texte d'une cotation, en unites du dessin. Gardee dans la
// propriete `dimension.text_height` (enregistree avec le document, modifiable
// dans le panneau Proprietes) ; les fleches et les ecarts en derivent.
inline constexpr const char* kDimensionTextHeightProperty = "dimension.text_height";
inline constexpr double kDefaultDimensionTextHeight = 2.5;
inline constexpr double kMinDimensionTextHeight = 1e-6;
inline constexpr double kMaxDimensionTextHeight = 1e6;

double dimensionTextHeight(const DimensionEntity& dimension);

// Texte d'une cotation : `position` est le milieu de la ligne de base, `angle`
// (radians) toujours lisible (jamais a l'envers).
struct DimensionLabel {
    Point2 position;
    double angle = 0.0;
    double height = kDefaultDimensionTextHeight;
    std::string text;
};

struct DimensionGraphics {
    // Un seul trace continu (les retours sur un meme segment ne se voient pas) :
    // c'est ce que rendent tessellate(), le canevas et le PDF.
    std::vector<Point2> path;
    DimensionLabel label;
};

DimensionGraphics dimensionGraphics(const DimensionEntity& dimension);

// Emprise (trace et texte) et distance au trace : selection et index spatial.
BoundingBox dimensionBounds(const DimensionEntity& dimension);
double dimensionDistance(const DimensionEntity& dimension, const Point2& p);

// Export DXF : la cotation est ecrite en LINE et TEXT tires de son dessin.
// Une entite DIMENSION exige un bloc anonyme de geometrie que BCAD ne produit
// pas ; sans lui, les lecteurs l'affichent vide ou rejettent le fichier. Les
// lignes et le texte se lisent partout, au prix de l'associativite.
void writeDimensionDxf(std::ostream& f, const DimensionEntity& dimension, const std::string& layer,
                       const std::optional<Color>& colorOverride);

// Angle ramene dans ]-90°, 90°] pour qu'un texte se lise de gauche a droite.
double readableAngle(double radians);

} // namespace bcad::geom
