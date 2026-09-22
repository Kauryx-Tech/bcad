#include "bcad/geometry/Triangulation.h"
#include "bcad/geometry/detail/CgalConversions.h"

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Triangulation_vertex_base_2.h>
#include <deque>

namespace bcad::geom {

namespace detail {

using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using CgalPoint2 = Kernel::Point_2;

struct FaceInfo {
    bool processed = false;
    bool inDomain = false;
};

using Fbb = CGAL::Triangulation_face_base_with_info_2<FaceInfo, Kernel>;
using Fb = CGAL::Constrained_triangulation_face_base_2<Kernel, Fbb>;
using Vb = CGAL::Triangulation_vertex_base_2<Kernel>;
using TDS = CGAL::Triangulation_data_structure_2<Vb, Fb>;
using CDT = CGAL::Constrained_Delaunay_triangulation_2<Kernel, TDS>;
using DT = CGAL::Delaunay_triangulation_2<Kernel>;

CDT::Vertex_handle insertPoint(CDT& cdt, const Point2& p) {
    return cdt.insert(toCgal(p));
}

DT::Vertex_handle insertPoint(DT& dt, const Point2& p) {
    return dt.insert(toCgal(p));
}

Point2 fromCgalPoint(const CDT::Vertex_handle& v) {
    return fromCgal(v->point());
}

Point2 fromCgalPoint(const DT::Vertex_handle& v) {
    return fromCgal(v->point());
}

} // namespace detail

std::vector<Triangle> delaunayTriangulate(const std::vector<Point2>& points) {
    using namespace detail;

    DT dt;
    for (const auto& p : points) {
        insertPoint(dt, p);
    }

    std::vector<Triangle> triangles;
    triangles.reserve(dt.number_of_faces());
    for (auto fit = dt.finite_faces_begin(); fit != dt.finite_faces_end(); ++fit) {
        triangles.push_back(Triangle{ { fromCgalPoint(fit->vertex(0)),
                                         fromCgalPoint(fit->vertex(1)),
                                         fromCgalPoint(fit->vertex(2)) } });
    }
    return triangles;
}

namespace {

// Remplissage par propagation depuis la face infinie, en basculant intérieur/extérieur
// à chaque franchissement d'une arête contrainte, afin que les trous (et les trous
// dans les trous) ressortent correctement sans nécessiter de pré-normalisation du sens de parcours du polygone.
void markDomains(detail::CDT& cdt) {
    for (auto fit = cdt.all_faces_begin(); fit != cdt.all_faces_end(); ++fit) {
        fit->info().processed = false;
        fit->info().inDomain = false;
    }

    std::deque<detail::CDT::Face_handle> queue;
    queue.push_back(cdt.infinite_face());
    cdt.infinite_face()->info().processed = true;
    cdt.infinite_face()->info().inDomain = false;

    while (!queue.empty()) {
        detail::CDT::Face_handle fh = queue.front();
        queue.pop_front();
        for (int i = 0; i < 3; ++i) {
            detail::CDT::Face_handle neighbor = fh->neighbor(i);
            if (neighbor->info().processed) continue;
            bool crossesConstraint = cdt.is_constrained(detail::CDT::Edge(fh, i));
            neighbor->info().processed = true;
            neighbor->info().inDomain = crossesConstraint ? !fh->info().inDomain : fh->info().inDomain;
            queue.push_back(neighbor);
        }
    }
}

void insertConstraints(detail::CDT& cdt, const std::vector<Point2>& ring) {
    if (ring.size() < 2) return;
    std::vector<detail::CDT::Vertex_handle> handles;
    handles.reserve(ring.size());
    for (const auto& p : ring) handles.push_back(detail::insertPoint(cdt, p));
    for (std::size_t i = 0; i < handles.size(); ++i) {
        cdt.insert_constraint(handles[i], handles[(i + 1) % handles.size()]);
    }
}

} // namespace

std::vector<Triangle> triangulatePolygon(const std::vector<Point2>& outerBoundary,
                                          const std::vector<std::vector<Point2>>& holes) {
    using namespace detail;

    CDT cdt;
    insertConstraints(cdt, outerBoundary);
    for (const auto& hole : holes) insertConstraints(cdt, hole);

    markDomains(cdt);

    std::vector<Triangle> triangles;
    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (fit->info().inDomain) {
            triangles.push_back(Triangle{ { fromCgalPoint(fit->vertex(0)),
                                             fromCgalPoint(fit->vertex(1)),
                                             fromCgalPoint(fit->vertex(2)) } });
        }
    }
    return triangles;
}

} // namespace bcad::geom