#pragma once

#include "bcad/geometry/Point.h"
#include <vector>

namespace bcad::app {

// Outil dessin parcelle (F1) : accumulation de clics → polygone fermé
class ParcelTool {
public:
    void addPoint(const bcad::geom::Point2& p) { points_.push_back(p); }
    void clear() { points_.clear(); }
    size_t count() const { return points_.size(); }
    const std::vector<bcad::geom::Point2>& points() const { return points_; }

    bool canClose() const { return points_.size() >= 3; }

    // Ferme le polygone (retourne les sommets si valide)
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
