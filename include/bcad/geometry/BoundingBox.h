#pragma once

#include "bcad/geometry/Point.h"
#include <algorithm>
#include <limits>

namespace bcad::geom {

struct BoundingBox {
    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();

    bool isValid() const { return minX <= maxX && minY <= maxY; }

    void expand(const Point2& p) {
        minX = std::min(minX, p.x_);
        minY = std::min(minY, p.y_);
        maxX = std::max(maxX, p.x_);
        maxY = std::max(maxY, p.y_);
    }

    void expand(const BoundingBox& other) {
        if (!other.isValid()) return;
        minX = std::min(minX, other.minX);
        minY = std::min(minY, other.minY);
        maxX = std::max(maxX, other.maxX);
        maxY = std::max(maxY, other.maxY);
    }

    void expand(double margin) {
        minX -= margin;
        minY -= margin;
        maxX += margin;
        maxY += margin;
    }

    bool intersects(const BoundingBox& other) const {
        if (!isValid() || !other.isValid()) return false;
        return minX <= other.maxX && maxX >= other.minX &&
               minY <= other.maxY && maxY >= other.minY;
    }

    bool contains(const Point2& p) const {
        return p.x_ >= minX && p.x_ <= maxX && p.y_ >= minY && p.y_ <= maxY;
    }

    bool contains(const BoundingBox& other) const {
        return other.minX >= minX && other.maxX <= maxX &&
               other.minY >= minY && other.maxY <= maxY;
    }

    double width() const { return maxX - minX; }
    double height() const { return maxY - minY; }
    Point2 center() const { return Point2((minX + maxX) / 2.0, (minY + maxY) / 2.0); }

    static BoundingBox fromCenterHalfExtent(const Point2& c, double halfExtent) {
        return BoundingBox{ c.x_ - halfExtent, c.y_ - halfExtent,
                             c.x_ + halfExtent, c.y_ + halfExtent };
    }
};

} // namespace bcad::geom