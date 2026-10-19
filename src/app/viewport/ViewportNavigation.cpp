// Navigation dans le dessin (V-01 a V-03), sans bouton du milieu ni molette
// obligatoires — le poste de reference est souvent un portable a pave
// tactile (ADR-016) :
//   zoom fenetre (ZF)      deux coins, cliques ou glisses ;
//   zoom precedent (ZP) / suivant (ZS)  historique des vues, immediat :
//                          la commande en cours continue ;
//   panoramique (P)        la main, au bouton gauche.
// Chaque changement de cadrage (zoom sur tout, fenetre, molette, glisser)
// garde d'abord la vue courante ; une rafale de molette compte pour un pas.

#include "viewport/Viewport.h"

#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Tolerance.h"

#include <algorithm>
#include <cmath>

namespace bcad::app {

using geom::Point2;

namespace {

constexpr std::size_t kMaxViews = 50;
constexpr qint64 kBurstMs = 800;

bool sameView(const render::Camera2D& a, const render::Camera2D& b) {
    const Point2 pa = a.screenToWorld({0, 0}), pb = b.screenToWorld({0, 0});
    const double tol = a.worldUnitsPerPixel() * 0.5;
    return std::abs(a.pixelsPerUnit() - b.pixelsPerUnit()) <= a.pixelsPerUnit() * 1e-9 &&
           geom::distance(pa, pb) <= tol;
}

} // namespace

void Viewport::rememberView(bool coalesce) {
    const bool inBurst = coalesce && viewBurst_.isValid() && viewBurst_.elapsed() < kBurstMs;
    viewBurst_.restart();
    if (!coalesce) viewBurst_.invalidate();
    if (inBurst) return;
    if (!viewBack_.empty() && sameView(viewBack_.back(), camera_)) return;
    viewBack_.push_back(camera_);
    if (viewBack_.size() > kMaxViews) viewBack_.erase(viewBack_.begin());
    viewForward_.clear();
}

void Viewport::zoomPrevious() {
    // Une vue identique a la courante (rien n'a bouge depuis) est sautee.
    while (!viewBack_.empty() && sameView(viewBack_.back(), camera_)) viewBack_.pop_back();
    if (viewBack_.empty()) {
        emit statusMessage(tr("Zoom précédent : aucune vue antérieure."));
        return;
    }
    viewForward_.push_back(camera_);
    setCamera(viewBack_.back());
    viewBack_.pop_back();
    viewBurst_.invalidate();
}

void Viewport::zoomNext() {
    if (viewForward_.empty()) {
        emit statusMessage(tr("Zoom suivant : aucune vue suivante."));
        return;
    }
    viewBack_.push_back(camera_);
    setCamera(viewForward_.back());
    viewForward_.pop_back();
    viewBurst_.invalidate();
}

void Viewport::applyZoomWindow(const Point2& world) {
    toolPoints_.push_back(world);
    if (toolPoints_.size() < 2) return;
    const Point2 a = toolPoints_[0], b = toolPoints_[1];
    const double minSide = 4.0 * camera_.worldUnitsPerPixel();
    if (std::abs(a.x_ - b.x_) < minSide && std::abs(a.y_ - b.y_) < minSide) {
        emit statusMessage(tr("Zoom fenêtre : fenêtre trop petite."));
        toolPoints_.pop_back();
        return;
    }
    rememberView(false);
    geom::BoundingBox box{std::min(a.x_, b.x_), std::min(a.y_, b.y_), std::max(a.x_, b.x_),
                          std::max(a.y_, b.y_)};
    camera_.zoomToFit(box, 0.0);
    requestTessellation();
    endCommand();
}

} // namespace bcad::app
