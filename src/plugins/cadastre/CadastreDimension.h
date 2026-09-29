#pragma once

// Cotations cadastrales (H1-H5) : génération automatique et interactive
// H1: Cotations linéaires - distances entre bornes (côtés parcelle)
// H2: Cotations angulaires - angles aux sommets
// H3: Étiquettes parcelle - Numéro + contenance au centre, échelle-dépendant
// H4: Bornes - Symboles + numérotation
// H5: Flèche Nord - Orientation

#include "bcad/geometry/Point.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/layout/Dimension.h"
#include "bcad/layout/AngleDimension.h"
#include "bcad/layout/NorthArrow.h"
#include "bcad/layout/Borne.h"
#include "bcad/layout/Label.h"
#include "bcad/core/Document.h"

#include <vector>
#include <string>
#include <optional>

namespace bcad::cadastre {

// Résultat complet des cotations pour une parcelle
struct ParcelDimensions {
    std::vector<layout::Dimension> linear;      // H1: côtés
    std::vector<layout::AngleDimension> angular; // H2: angles
    layout::Label label;                         // H3: étiquette (section + numero + contenance)
    std::vector<layout::Borne> bornes;          // H4: bornes
    std::optional<layout::NorthArrow> northArrow; // H5: flèche nord (optionnel, par plan)
    
    bool hasGeometry() const {
        return !linear.empty() || !angular.empty() || (label.position.x_ != 0.0 || label.position.y_ != 0.0) || !bornes.empty();
    }
};

// Génère toutes les cotations pour une parcelle
ParcelDimensions generateParcelDimensions(const geom::PolylineEntity& parcel);

// Génère les cotations pour toutes les parcelles d'un document
std::vector<ParcelDimensions> generateDocumentDimensions(const core::Document& document);

// Crée les entités de cotation (LinearDimensionEntity, AngularDimensionEntity, etc.)
// et les ajoute au document
void addParcelDimensionsToDocument(core::Document& document, const std::vector<ParcelDimensions>& dims);

// Calcule l'échelle optimale pour l'affichage des textes en fonction de l'échelle du plan
double computeTextScale(double planScale, double baseTextHeight = 2.5);

// Formate une distance pour affichage cadastral (m ou mm selon échelle)
std::string formatCadastralDistance(double distanceM, double planScale);

// Génère une étiquette de parcelle (H3) : section + numero + contenance
layout::Label makeParcelLabel(const geom::PolylineEntity& parcel, double planScale);

} // namespace bcad::cadastre