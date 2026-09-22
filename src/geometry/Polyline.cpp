#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/BooleanOps.h"
#include <sstream>

namespace bcad::geom {

std::string PolylineEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << "Polyline (" << (closed_ ? "closed" : "open") << ")\nVertices: " << vertices_.size()
       << "\nLength: " << length();
    if (closed_) ss << "\nArea: " << std::abs(polygonArea(*this));
    return ss.str();
}

void PolylineEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    auto midpoint = [](const Point2& a, const Point2& b) {
        return Point2((a.x_ + b.x_) / 2.0, (a.y_ + b.y_) / 2.0);
    };
    std::size_t n = vertices_.size();
    for (std::size_t i = 0; i < n; ++i) add(vertices_[i], SnapPointType::Endpoint);
    std::size_t segCount = closed_ ? n : (n == 0 ? 0 : n - 1);
    for (std::size_t i = 0; i < segCount; ++i) {
        add(midpoint(vertices_[i], vertices_[(i + 1) % n]), SnapPointType::Midpoint);
    }
}

} // namespace bcad::geom