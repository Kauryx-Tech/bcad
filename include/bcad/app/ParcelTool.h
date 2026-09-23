#pragma once

#include "bcad/geometry/Point.h"
#include "bcad/cadastre/Cogo.h"
#include <vector>

namespace bcad::app {

// Outil dessin parcelle (F1) : accumulation de clics OU saisie COGO
// (gisement/distance depuis le dernier point, cf. Civil 3D COGO input).
class ParcelTool {
public:
    void setStart(const bcad::geom::Point2& p) { points_.clear(); points_.push_back(p); }
    void addPoint(const bcad::geom::Point2& p) { points_.push_back(p); }

    // Saisie COGO : ajoute le point à (azimut Nord horaire°, distance) du dernier.
    // Retourne false si aucun point de départ.
    bool addCogoPoint(double azimuthDeg, double distance) {
        if (points_.empty() || distance < 0) return false;
        points_.push_back(bcad::cadastre::forward(points_.back(), azimuthDeg, distance));
        return true;
    }

    void clear() { points_.clear(); }
    size_t count() const { return points_.size(); }
    const std::vector<bcad::geom::Point2>& points() const { return points_; }

    bool canClose() const { return points_.size() >= 3; }

    // Erreur de fermeture : distance entre dernier et premier point.
    // 0 = polygone fermé. Sert au contrôle qualité (traverse closure).
    double closureError() const {
        if (points_.size() < 2) return 0;
        auto r = bcad::cadastre::inverse(points_.front(), points_.back());
        return r.distance;
    }

    bool close(std::vector<bcad::geom::Point2>& out) const {
        if (!canClose()) return false;
        out = points_;
        return true;
    }

    void undoLast() { if (!points_.empty()) points_.pop_back(); }

private:
    std::vector<bcad::geom::Point2> points_;
};

} // namespace bcad::app
