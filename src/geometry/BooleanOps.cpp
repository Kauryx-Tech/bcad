#include "bcad/geometry/BooleanOps.h"

#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Polygon_2_algorithms.h>

namespace bcad::geom {

namespace {

// CGAL's join/intersection/difference expect counter-clockwise outer
// boundaries; a clockwise input (common when digitizing by hand) silently
// produces wrong results, so we normalize orientation before every op.
Polygon2 orientedCcw(Polygon2 poly) {
    if (poly.is_clockwise_oriented()) poly.reverse_orientation();
    return poly;
}

} // namespace

std::vector<PolygonWithHoles2> booleanOpWithHoles(const PolylineEntity& a,
                                                    const PolylineEntity& b,
                                                    BooleanOp op) {
    Polygon2 pa = orientedCcw(a.toPolygon());
    Polygon2 pb = orientedCcw(b.toPolygon());

    std::vector<PolygonWithHoles2> results;

    switch (op) {
        case BooleanOp::Union: {
            PolygonWithHoles2 out;
            if (CGAL::join(pa, pb, out)) results.push_back(std::move(out));
            break;
        }
        case BooleanOp::Intersection: {
            CGAL::intersection(pa, pb, std::back_inserter(results));
            break;
        }
        case BooleanOp::Difference: {
            CGAL::difference(pa, pb, std::back_inserter(results));
            break;
        }
        case BooleanOp::SymmetricDifference: {
            CGAL::symmetric_difference(pa, pb, std::back_inserter(results));
            break;
        }
    }
    return results;
}

std::vector<PolylineEntity> booleanOp(const PolylineEntity& a, const PolylineEntity& b, BooleanOp op) {
    std::vector<PolylineEntity> out;
    for (const auto& pwh : booleanOpWithHoles(a, b, op)) {
        out.push_back(PolylineEntity::fromPolygon(pwh.outer_boundary()));
        for (auto holeIt = pwh.holes_begin(); holeIt != pwh.holes_end(); ++holeIt) {
            out.push_back(PolylineEntity::fromPolygon(*holeIt));
        }
    }
    return out;
}

double polygonArea(const PolylineEntity& polyline) {
    return CGAL::to_double(polyline.toPolygon().area());
}

bool isSimplePolygon(const PolylineEntity& polyline) {
    return polyline.toPolygon().is_simple();
}

} // namespace bcad::geom
