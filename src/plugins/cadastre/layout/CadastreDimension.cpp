#include "bcad/cadastre/CadastreDimension.h"

#include "entities/ParcelEntity.h"
#include "entities/SurveyMarkEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/RadialDimensionEntity.h"
#include "bcad/geometry/DimensionEntity.h"
#include "bcad/layout/DimensionStyle.h"
#include "bcad/layout/Scale.h"
#include "bcad/layout/Label.h"
#include "bcad/layout/Borne.h"
#include "bcad/layout/NorthArrow.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace bcad::cadastre {

namespace {

// Distance euclidienne entre deux points
double distance(const geom::Point2& a, const geom::Point2& b) {
    return std::hypot(b.x_ - a.x_, b.y_ - a.y_);
}

// Angle entre deux vecteurs (en degrés)
double angleBetween(const geom::Point2& v1, const geom::Point2& v2) {
    double dot = v1.x_ * v2.x_ + v1.y_ * v2.y_;
    double det = v1.x_ * v2.y_ - v1.y_ * v2.x_;
    double angle = std::atan2(std::abs(det), dot) * 180.0 / M_PI;
    return angle;
}

// Formate une distance pour affichage cadastral
std::string formatCadastralDistance(double distanceM, double planScale) {
    // En cadastral, on affiche en mètres avec 2 décimales pour les distances > 1m
    // ou en mm pour les petites distances
    if (distanceM >= 1.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f m", distanceM);
        return buf;
    } else {
        int mm = static_cast<int>(std::round(distanceM * 1000.0));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d mm", mm);
        return buf;
    }
}

// Calcule l'échelle de texte optimale
double computeTextScale(double planScale, double baseTextHeight) {
    // Texte de base 2.5mm sur papier, ajusté selon l'échelle
    return baseTextHeight * planScale / 1000.0;
}

} // namespace

// Génère toutes les cotations pour une parcelle
ParcelDimensions generateParcelDimensions(const geom::PolylineEntity& parcel) {
    ParcelDimensions result;
    const auto& vertices = parcel.vertices();
    size_t n = vertices.size();
    
    if (n < 3) return result; // Pas une parcelle valide
    
    // H1: Cotations linéaires - distances entre bornes (côtés parcelle)
    for (size_t i = 0; i < n; ++i) {
        const auto& a = vertices[i];
        const auto& b = vertices[(i + 1) % n];
        double d = geom::distance(a, b);
        geom::Point2 mid{(a.x_ + b.x_) * 0.5, (a.y_ + b.y_) * 0.5};
        result.linear.push_back({a, b, std::round(d * 100.0) / 100.0, mid});
    }
    
    // H2: Cotations angulaires - angles aux sommets
    for (size_t i = 0; i < n; ++i) {
        const auto& p = vertices[i];
        const auto& a = vertices[(i + n - 1) % n];
        const auto& b = vertices[(i + 1) % n];
        
        geom::Point2 v1{a.x_ - p.x_, a.y_ - p.y_};
        geom::Point2 v2{b.x_ - p.x_, b.y_ - p.y_};
        
        double dot = v1.x_ * v2.x_ + v1.y_ * v2.y_;
        double det = v1.x_ * v2.y_ - v1.y_ * v2.x_;
        double angleDeg = std::atan2(std::abs(v1.x_ * v2.y_ - v1.y_ * v2.x_), dot) * 180.0 / M_PI;
        
        result.angular.push_back({p, a, b, angleDeg});
    }
    
    // H3: Étiquette parcelle (section + numero + contenance)
    std::string section = parcel.properties().getString("cadastre.section");
    std::string numero = parcel.properties().getString("cadastre.numero");
    std::string contenance = parcel.properties().getString("cadastre.contenance");
    
    if (section.empty() && numero.empty()) {
        // Fallback: utiliser centroïde
        geom::Point2 centroid{0, 0};
        for (const auto& v : vertices) {
            centroid.x_ += v.x_;
            centroid.y_ += v.y_;
        }
        centroid.x_ /= vertices.size();
        centroid.y_ /= vertices.size();
        
        std::string text = "Parcel";
        if (!section.empty()) text = section + " " + numero;
        if (!contenance.empty()) text += " (" + contenance + ")";
        
        result.label = layout::Label::forParcel(section, numero, contenance, vertices);
    } else {
        result.label = layout::Label::forParcel(section, numero, contenance, vertices);
    }
    
    // H4: Bornes - déjà géré par SurveyMarkEntity, mais on peut extraire depuis les sommets
    // Les bornes sont déjà créées par buildSheetFurniture
    
    return ParcelDimensions{result.linear, result.angular, result.label, {}, {}};
}

std::vector<ParcelDimensions> generateDocumentDimensions(const core::Document& document) {
    std::vector<ParcelDimensions> result;
    
    for (const auto& entity : document.entities()) {
        if (auto* parcel = dynamic_cast<const ParcelEntity*>(entity.get())) {
            result.push_back(generateParcelDimensions(*parcel));
        }
    }
    
    return result;
}

void addParcelDimensionsToDocument(core::Document& document, const std::vector<ParcelDimensions>& dims) {
    // Pour l'instant, on ne crée pas d'entités de cotation persistantes
    // Les cotations sont générées à la volée pour l'affichage/l'export
    // L'implémentation complète nécessiterait de créer des LinearDimensionEntity,
    // AngularDimensionEntity, etc. et de les ajouter au document
    
    // TODO: Créer les entités de cotation persistantes
    // Pour l'instant, les cotations sont générées à la volée pour l'export PDF
    (void)document;
    (void)dims;
}

double computeTextScale(double planScale, double baseTextHeight) {
    return baseTextHeight * planScale / 1000.0;
}

std::string formatCadastralDistance(double distanceM, double planScale) {
    (void)planScale;
    if (distanceM >= 1.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f m", distanceM);
        return buf;
    } else {
        int mm = static_cast<int>(std::round(distanceM * 1000.0));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d mm", mm);
        return buf;
    }
}

layout::Label makeParcelLabel(const geom::PolylineEntity& parcel, double planScale) {
    (void)planScale;
    
    std::string section = parcel.properties().getString("cadastre.section");
    std::string numero = parcel.properties().getString("cadastre.numero");
    std::string contenance = parcel.properties().getString("cadastre.contenance");
    
    // Calculer le centroïde
    const auto& vertices = parcel.vertices();
    geom::Point2 centroid{0, 0};
    for (const auto& v : vertices) {
        centroid.x_ += v.x_;
        centroid.y_ += v.y_;
    }
    if (!vertices.empty()) {
        centroid.x_ /= vertices.size();
        centroid.y_ /= vertices.size();
    }
    
    std::string text = section + " " + numero;
    if (!contenance.empty()) text += " (" + contenance + ")";
    
    layout::Label label;
    label.text = text;
    label.position = centroid;
    label.fontSizeMm = 2.5;
    
    return label;
}

} // namespace bcad::cadastre