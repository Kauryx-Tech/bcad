#include "bcad/geometry/BooleanOps.h"
#include "bcad/geometry/detail/CgalConversions.h"
#include "bcad/geometry/Polyline.h"

namespace bcad::geom {

namespace detail {

using Kernel = ::CGAL::Exact_predicates_inexact_constructions_kernel;
using CgalPolygon2 = ::CGAL::Polygon_2<Kernel>;
using CgalPolygonWithHoles2 = ::CGAL::Polygon_with_holes_2<Kernel>;

CgalPolygon2 toCgalPolygon(const PolylineEntity& poly) {
    CgalPolygon2 result;
    for (const auto& v : poly.vertices()) {
        result.push_back(toCgal(v));
    }
    return result;
}

PolylineEntity fromCgalPolygon(const ::CGAL::Polygon_2<Kernel>& poly) {
    std::vector<Point2> verts;
    verts.reserve(poly.size());
    for (const auto& v : poly.vertices()) {
        verts.push_back(fromCgal(v));
    }
    return PolylineEntity(std::move(verts), true);
}

::CGAL::Polygon_with_holes_2<Kernel> toCgalPolygonWithHoles(const PolygonWithHolesResult& pwh) {
    ::CGAL::Polygon_with_holes_2<Kernel> result;
    result.outer_boundary() = toCgalPolygon(pwh.outer);
    for (const auto& hole : pwh.holes) {
        result.add_hole(toCgalPolygon(hole));
    }
    return result;
}

PolygonWithHolesResult fromCgalPolygonWithHoles(const ::CGAL::Polygon_with_holes_2<Kernel>& pwh) {
    PolygonWithHolesResult result;
    result.outer = fromCgalPolygon(pwh.outer_boundary());
    for (auto it = pwh.holes_begin(); it != pwh.holes_end(); ++it) {
        result.holes.push_back(fromCgalPolygon(*it));
    }
    return result;
}

CgalPolygon2 orientedCcw(CgalPolygon2 poly) {
    if (poly.is_clockwise_oriented()) poly.reverse_orientation();
    return poly;
}

} // namespace detail

std::vector<PolygonWithHolesResult> booleanOpWithHoles(const PolylineEntity& a,
                                                        const PolylineEntity& b,
                                                        BooleanOp op) {
    using namespace detail;

    CgalPolygon2 pa = orientedCcw(toCgalPolygon(a));
    CgalPolygon2 pb = orientedCcw(toCgalPolygon(b));

    std::vector<::CGAL::Polygon_with_holes_2<Kernel>> cgalResults;

    switch (op) {
        case BooleanOp::Union: {
            ::CGAL::Polygon_with_holes_2<Kernel> out;
            if (::CGAL::join(pa, pb, out)) cgalResults.push_back(std::move(out));
            break;
        }
        case BooleanOp::Intersection: {
            ::CGAL::intersection(pa, pb, std::back_inserter(cgalResults));
            break;
        }
        case BooleanOp::Difference: {
            ::CGAL::difference(pa, pb, std::back_inserter(cgalResults));
            break;
        }
        case BooleanOp::SymmetricDifference: {
            ::CGAL::symmetric_difference(pa, pb, std::back_inserter(cgalResults));
            break;
        }
    }

    std::vector<PolygonWithHolesResult> results;
    results.reserve(cgalResults.size());
    for (const auto& r : cgalResults) {
        results.push_back(fromCgalPolygonWithHoles(r));
    }
    return results;
}

std::vector<PolylineEntity> booleanOp(const PolylineEntity& a, const PolylineEntity& b, BooleanOp op) {
    std::vector<PolylineEntity> out;
    for (const auto& pwh : booleanOpWithHoles(a, b, op)) {
        out.push_back(pwh.outer);
        for (const auto& hole : pwh.holes) {
            out.push_back(hole);
        }
    }
    return out;
}

double polygonArea(const PolylineEntity& polyline) {
    using namespace detail;
    double area = ::CGAL::to_double(toCgalPolygon(polyline).area());
    // Déduire la surface de chaque anneau intérieur (trou) via la formule du
    // lacet — en conservant le sens de signe de l'anneau extérieur (§0.7).
    const double sign = area >= 0.0 ? 1.0 : -1.0;
    for (const auto& hole : polyline.holes()) {
        double sum = 0.0;
        const std::size_t n = hole.size();
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;
            sum += hole[i].x_ * hole[j].y_ - hole[j].x_ * hole[i].y_;
        }
        area -= sign * std::abs(sum / 2.0);
    }
    return area;
}

bool isSimplePolygon(const PolylineEntity& polyline) {
    using namespace detail;
    return toCgalPolygon(polyline).is_simple();
}

} // namespace bcad::geom