#include "bcad/geometry/Triangulation.h"

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Triangulation_vertex_base_2.h>
#include <deque>

namespace bcad::geom {

std::vector<Triangle> delaunayTriangulate(const std::vector<Point2>& points) {
    using DT = CGAL::Delaunay_triangulation_2<Kernel>;
    DT dt;
    dt.insert(points.begin(), points.end());

    std::vector<Triangle> triangles;
    triangles.reserve(dt.number_of_faces());
    for (auto fit = dt.finite_faces_begin(); fit != dt.finite_faces_end(); ++fit) {
        triangles.push_back(Triangle{ { fit->vertex(0)->point(),
                                         fit->vertex(1)->point(),
                                         fit->vertex(2)->point() } });
    }
    return triangles;
}

namespace {

struct FaceInfo {
    bool processed = false;
    bool inDomain = false;
};

using Fbb = CGAL::Triangulation_face_base_with_info_2<FaceInfo, Kernel>;
using Fb = CGAL::Constrained_triangulation_face_base_2<Kernel, Fbb>;
using Vb = CGAL::Triangulation_vertex_base_2<Kernel>;
using TDS = CGAL::Triangulation_data_structure_2<Vb, Fb>;
using CDT = CGAL::Constrained_Delaunay_triangulation_2<Kernel, TDS>;

// Remplissage par propagation depuis la face infinie, en basculant intérieur/extérieur
// à chaque franchissement d'une arête contrainte, afin que les trous (et les trous
// dans les trous) ressortent correctement sans nécessiter de pré-normalisation du sens de parcours du polygone.
void markDomains(CDT& cdt) {
    for (auto fit = cdt.all_faces_begin(); fit != cdt.all_faces_end(); ++fit) {
        fit->info().processed = false;
        fit->info().inDomain = false;
    }

    std::deque<CDT::Face_handle> queue;
    queue.push_back(cdt.infinite_face());
    cdt.infinite_face()->info().processed = true;
    cdt.infinite_face()->info().inDomain = false;

    while (!queue.empty()) {
        CDT::Face_handle fh = queue.front();
        queue.pop_front();
        for (int i = 0; i < 3; ++i) {
            CDT::Face_handle neighbor = fh->neighbor(i);
            if (neighbor->info().processed) continue;
            bool crossesConstraint = cdt.is_constrained(CDT::Edge(fh, i));
            neighbor->info().processed = true;
            neighbor->info().inDomain = crossesConstraint ? !fh->info().inDomain : fh->info().inDomain;
            queue.push_back(neighbor);
        }
    }
}

void insertConstraints(CDT& cdt, const std::vector<Point2>& ring) {
    if (ring.size() < 2) return;
    std::vector<CDT::Vertex_handle> handles;
    handles.reserve(ring.size());
    for (const auto& p : ring) handles.push_back(cdt.insert(p));
    for (std::size_t i = 0; i < handles.size(); ++i) {
        cdt.insert_constraint(handles[i], handles[(i + 1) % handles.size()]);
    }
}

} // namespace

std::vector<Triangle> triangulatePolygon(const std::vector<Point2>& outerBoundary,
                                          const std::vector<std::vector<Point2>>& holes) {
    CDT cdt;
    insertConstraints(cdt, outerBoundary);
    for (const auto& hole : holes) insertConstraints(cdt, hole);

    markDomains(cdt);

    std::vector<Triangle> triangles;
    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (fit->info().inDomain) {
            triangles.push_back(Triangle{ { fit->vertex(0)->point(),
                                             fit->vertex(1)->point(),
                                             fit->vertex(2)->point() } });
        }
    }
    return triangles;
}

} // namespace bcad::geom
