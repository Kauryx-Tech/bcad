// Outils de cotation (A-01, A-02) : chaque cotation est un seul objet
// (`DimensionEntity`), qui se dessine, se selectionne, se deplace et
// s'enregistre d'un bloc — et non plus des lignes et un texte separes.
//
// Etapes, comme AutoCAD :
//   lineaire  : deux origines, puis la position de la ligne de cote ; la
//               position choisit horizontale ou verticale, H ou V la force ;
//   alignee   : deux origines, puis la position (parallele aux origines) ;
//   angulaire : sommet, un point de chaque cote, puis la position de l'arc ;
//   rayon et diametre : un cercle ou un arc designe (ou son centre), puis un
//               point donnant la direction.

#include "Viewport.h"
#include "ViewportTolerances.h"

#include "bcad/geometry/AlignedDimensionEntity.h"
#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/RadialDimensionEntity.h"
#include "bcad/geometry/Tolerance.h"

#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace bcad::app {

using geom::Point2;

namespace {

constexpr double kEps = geom::Tolerance::kDegenerateLength;

std::size_t requiredDimensionPoints(ToolMode tool) {
    switch (tool) {
        case ToolMode::DimensionLinear:
        case ToolMode::DimensionAligned: return 3;
        case ToolMode::DimensionAngular: return 4;
        case ToolMode::DimensionRadius:
        case ToolMode::DimensionDiameter: return 2;
        default: return 0;
    }
}

// Valeur « ronde » (1, 2, 2,5 ou 5 x 10^n) au moins egale a `value`.
double roundUpNice(double value) {
    const double decade = std::pow(10.0, std::floor(std::log10(value)));
    for (double step : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (decade * step >= value * (1.0 - 1e-9)) return decade * step;
    return decade * 10.0;
}

} // namespace

bool Viewport::isDimensionTool() const {
    return requiredDimensionPoints(tool_) > 0;
}

double Viewport::newDimensionTextHeight() const {
    // Toutes les cotations d'un dessin ont la meme taille : celle de la
    // derniere posee. La premiere prend une valeur ronde lisible a l'ecran
    // (environ 12 pixels) ; le panneau Proprietes la change ensuite.
    if (doc_) {
        const auto& entities = doc_->entities();
        for (auto it = entities.rbegin(); it != entities.rend(); ++it)
            if (const auto* dim = dynamic_cast<const geom::DimensionEntity*>(it->get()))
                return geom::dimensionTextHeight(*dim);
    }
    const double ppu = camera_.pixelsPerUnit();
    if (!(ppu > 0.0) || !std::isfinite(ppu)) return geom::kDefaultDimensionTextHeight;
    return roundUpNice(12.0 / ppu);
}

std::unique_ptr<geom::DimensionEntity> Viewport::dimensionFromPoints(
    const std::vector<Point2>& pts) const {
    const std::size_t required = requiredDimensionPoints(tool_);
    if (required == 0 || pts.size() < required) return nullptr;

    std::unique_ptr<geom::DimensionEntity> dim;
    switch (tool_) {
        case ToolMode::DimensionLinear: {
            const Point2 &a = pts[0], &b = pts[1], &loc = pts[2];
            double rotation = 0.0;
            if (dimOrientation_ == 2) {
                rotation = std::numbers::pi / 2.0;
            } else if (dimOrientation_ == 0) {
                // Hors de la boite des deux origines : au-dessus ou en dessous,
                // horizontale ; a gauche ou a droite, verticale.
                const double outX = std::max({0.0, std::min(a.x_, b.x_) - loc.x_, loc.x_ - std::max(a.x_, b.x_)});
                const double outY = std::max({0.0, std::min(a.y_, b.y_) - loc.y_, loc.y_ - std::max(a.y_, b.y_)});
                const bool vertical = outX > outY ||
                    (outX == 0.0 && outY == 0.0 && std::abs(b.y_ - a.y_) > std::abs(b.x_ - a.x_));
                rotation = vertical ? std::numbers::pi / 2.0 : 0.0;
            }
            dim = std::make_unique<geom::LinearDimensionEntity>(a, b, loc, rotation);
            break;
        }
        case ToolMode::DimensionAligned:
            if (geom::distance(pts[0], pts[1]) < kEps) return nullptr;
            dim = std::make_unique<geom::AlignedDimensionEntity>(pts[0], pts[1], pts[2]);
            break;
        case ToolMode::DimensionAngular:
            if (geom::distance(pts[0], pts[1]) < kEps || geom::distance(pts[0], pts[2]) < kEps)
                return nullptr;
            dim = std::make_unique<geom::AngularDimensionEntity>(pts[0], pts[1], pts[2], pts[3]);
            break;
        case ToolMode::DimensionRadius:
        case ToolMode::DimensionDiameter: {
            const Point2& center = pts[0];
            Point2 edge = pts[1];
            if (dimRadius_ > 0.0) {   // cercle designe : le point ne donne que la direction
                const double d = geom::distance(center, pts[1]);
                if (d < kEps) return nullptr;
                edge = Point2{center.x_ + (pts[1].x_ - center.x_) / d * dimRadius_,
                              center.y_ + (pts[1].y_ - center.y_) / d * dimRadius_};
            }
            if (geom::distance(center, edge) < kEps) return nullptr;
            dim = std::make_unique<geom::RadialDimensionEntity>(
                center, edge,
                tool_ == ToolMode::DimensionRadius ? geom::RadialDimensionEntity::RadialType::Radius
                                                   : geom::RadialDimensionEntity::RadialType::Diameter,
                Point2{(center.x_ + edge.x_) * 0.5, (center.y_ + edge.y_) * 0.5});
            break;
        }
        default:
            return nullptr;
    }
    if (dim->measuredValue() < kEps) return nullptr;
    dim->setLayer("Cotations");
    dim->properties().setDouble(geom::kDimensionTextHeightProperty,
                                dimTextHeight_ > 0.0 ? dimTextHeight_ : newDimensionTextHeight());
    return dim;
}

void Viewport::placeDimension(const Point2& world) {
    // Rayon, diametre : un clic sur un cercle ou un arc en prend le centre et
    // le rayon, comme la designation d'objet d'AutoCAD.
    if (toolPoints_.empty() && (tool_ == ToolMode::DimensionRadius || tool_ == ToolMode::DimensionDiameter)) {
        dimRadius_ = 0.0;
        if (doc_) {
            // Seuls les cercles et les arcs se designent : une cote ou un texte
            // proche ne doit pas les masquer.
            double best = kPickToleranceScreenPx / camera_.pixelsPerUnit();
            for (const auto& entity : doc_->entities()) {
                if (entity->typeId() != geom::TypeId_Circle && entity->typeId() != geom::TypeId_Arc)
                    continue;
                const double d = entity->distanceTo(world);
                if (d > best) continue;
                best = d;
                if (auto* circle = dynamic_cast<geom::CircleEntity*>(entity.get())) {
                    toolPoints_ = {circle->center()};
                    dimRadius_ = circle->radius();
                } else if (auto* arc = dynamic_cast<geom::ArcEntity*>(entity.get())) {
                    toolPoints_ = {arc->center()};
                    dimRadius_ = arc->radius();
                }
            }
            if (dimRadius_ > 0.0) {
                dimTextHeight_ = newDimensionTextHeight();
                return;
            }
        }
    }
    // La hauteur de texte est choisie au premier point, pas a chaque image de
    // l'apercu (elle parcourt le document).
    if (toolPoints_.empty()) dimTextHeight_ = newDimensionTextHeight();
    toolPoints_.push_back(world);
    if (toolPoints_.size() < requiredDimensionPoints(tool_)) return;

    auto dim = dimensionFromPoints(toolPoints_);
    if (!dim) {
        emit statusMessage(tr("Cotation nulle : choisissez des points distincts."));
        toolPoints_.clear();
        dimRadius_ = 0.0;
        dimOrientation_ = 0;
        notifyPrompt();
        return;
    }
    ensureDimensionLayer();
    QString label;
    switch (tool_) {
        case ToolMode::DimensionLinear: label = tr("Cotation linéaire"); break;
        case ToolMode::DimensionAligned: label = tr("Cotation alignée"); break;
        case ToolMode::DimensionAngular: label = tr("Cotation angulaire"); break;
        case ToolMode::DimensionRadius: label = tr("Cotation de rayon"); break;
        default: label = tr("Cotation de diamètre"); break;
    }
    commitEntity(std::move(dim), label);
    endCommand();
}

bool Viewport::submitDimensionOption(const QString& text) {
    // Options de la cotation lineaire, avant la position de la ligne de cote.
    if (tool_ != ToolMode::DimensionLinear || toolPoints_.size() != 2) return false;
    const QString option = text.trimmed().toUpper();
    if (option == QLatin1String("H") || option == QLatin1String("HORIZONTALE")) dimOrientation_ = 1;
    else if (option == QLatin1String("V") || option == QLatin1String("VERTICALE")) dimOrientation_ = 2;
    else return false;
    notifyPrompt();
    update();
    return true;
}

} // namespace bcad::app
