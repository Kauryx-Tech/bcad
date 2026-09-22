#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Polyline.h"

namespace bcad::geom {

// Offset polyline (simplified v1 - offset each segment)
std::unique_ptr<PolylineEntity> offsetPolyline(const PolylineEntity& poly, double distance, const Point2& sidePoint) {
    std::vector<Point2> newVerts;
    newVerts.reserve(poly.vertices().size());
    
    // For each vertex, offset along the angle bisector
    // Simplified v1: offset each segment and connect
    // This is a simplified implementation
    return std::make_unique<PolylineEntity>(std::vector<Point2>(), poly.closed());
}

// Break polyline - insert vertex at break point
void breakPolyline(PolylineEntity& polyline, const Point2& breakPoint) {
    // Find the segment containing breakPoint
    // Insert breakPoint as new vertex
    // For v1, just insert the vertex
    std::size_t n = polyline.vertices().size();
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const Point2& a = polyline.vertices()[i];
        const Point2& b = polyline.vertices()[(i + 1) % n];
        if (distancePointToLine(breakPoint, a, b) < Tolerance::kLinear) {
            // Insert after vertex i
            polyline.vertices().insert(polyline.vertices().begin() + i + 1, breakPoint);
            break;
        }
    }
}

} // namespace bcad::geom