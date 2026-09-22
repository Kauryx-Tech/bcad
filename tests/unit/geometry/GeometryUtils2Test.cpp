#include "bcad/geometry/GeometryUtils2.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>

namespace {
int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

using namespace bcad;
using namespace bcad::geom;

void testIntersectLineLine() {
    std::printf("\n=== testIntersectLineLine ===\n");
    
    // Crossing lines
    Line2 a{{0, 0}, {10, 10}};
    Line2 b{{0, 10}, {10, 0}};
    auto res = intersect(a, b);
    check(res.has_value(), "Intersecting lines have intersection");
    if (res) check(nearlyEqual(res->x_, 5.0) && nearlyEqual(res->y_, 5.0), "Intersection at (5,5)");

    // Parallel lines
    Line2 c{{0, 0}, {10, 0}};
    Line2 d{{0, 5}, {10, 5}};
    auto res2 = intersect(c, d);
    check(!res2.has_value(), "Parallel lines have no intersection");

    // Collinear overlapping (treated as parallel)
    Line2 e{{0, 0}, {10, 0}};
    Line2 f{{5, 0}, {15, 0}};
    auto res3 = intersect(e, f);
    check(!res3.has_value(), "Collinear lines have no single intersection");
}

void testIntersectSegmentSegment() {
    std::printf("\n=== testIntersectSegmentSegment ===\n");
    
    // Crossing segments
    Segment2 a{{0, 0}, {10, 10}};
    Segment2 b{{0, 10}, {10, 0}};
    auto res = intersect(a, b);
    check(res.has_value(), "Intersecting segments have intersection");
    if (res) check(nearlyEqual(res->x_, 5.0) && nearlyEqual(res->y_, 5.0), "Intersection at (5,5)");

    // Segments that don't intersect
    Segment2 c{{0, 0}, {5, 0}};
    Segment2 d{{10, 0}, {15, 0}};
    auto res2 = intersect(c, d);
    check(!res2.has_value(), "Non-overlapping collinear segments have no intersection");
}

void testIntersectLineCircle() {
    std::printf("\n=== testIntersectLineCircle ===\n");
    
    // Line through circle center - 2 intersections
    Line2 a{{-10, 0}, {10, 0}};
    Circle2 c{{0, 0}, 5};
    auto res = intersect(a, c);
    check(res.size() == 2, "Line through circle center has 2 intersections");
    if (res.size() == 2) {
        check(nearlyEqual(res[0].x_, -5.0) && nearlyEqual(res[0].y_, 0.0), "First intersection at (-5,0)");
        check(nearlyEqual(res[1].x_, 5.0) && nearlyEqual(res[1].y_, 0.0), "Second intersection at (5,0)");
    }

    // Tangent line - 1 intersection
    Line2 b{{0, 5}, {10, 5}};
    auto res2 = intersect(b, c);
    check(res2.size() == 1, "Tangent line has 1 intersection");
    if (res2.size() == 1) {
        check(nearlyEqual(res2[0].x_, 0.0) && nearlyEqual(res2[0].y_, 5.0), "Tangent at (0,5)");
    }

    // Line missing circle - 0 intersections
    Line2 d{{0, 10}, {10, 10}};
    auto res3 = intersect(d, c);
    check(res3.empty(), "Line missing circle has 0 intersections");
}

void testIntersectCircleCircle() {
    std::printf("\n=== testIntersectCircleCircle ===\n");
    
    // Two intersecting circles
    Circle2 a{{0, 0}, 5};
    Circle2 b{{8, 0}, 5};
    auto res = intersect(a, b);
    check(res.size() == 2, "Two intersecting circles have 2 intersections");
    if (res.size() == 2) {
        // Intersections should be at x=4, y=+/-3
        check(nearlyEqual(res[0].x_, 4.0), "First intersection x=4");
        check(nearlyEqual(res[1].x_, 4.0), "Second intersection x=4");
    }

    // Concentric circles - 0 intersections
    Circle2 c{{0, 0}, 5};
    Circle2 d{{0, 0}, 3};
    auto res2 = intersect(c, d);
    check(res2.empty(), "Concentric circles have 0 intersections");

    // Separate circles - 0 intersections
    Circle2 e{{0, 0}, 5};
    Circle2 f{{20, 0}, 5};
    auto res3 = intersect(e, f);
    check(res3.empty(), "Separate circles have 0 intersections");

    // Touching circles - 1 intersection (tangent)
    Circle2 g{{0, 0}, 5};
    Circle2 h{{10, 0}, 5};
    auto res4 = intersect(g, h);
    check(res4.size() == 1, "Touching circles have 1 intersection");
}

void testIntersectLinePolyline() {
    std::printf("\n=== testIntersectLinePolyline ===\n");
    
    // Line crossing a square polyline
    Line2 a{{-5, 5}, {15, 5}};
    Polyline2 p;
    p.vertices = {Point2(0, 0), Point2(10, 0), Point2(10, 10), Point2(0, 10)};
    p.closed = true;
    auto res = intersect(a, p);
    check(res.size() == 2, "Line crossing square has 2 intersections");
    if (res.size() == 2) {
        // Order depends on segment iteration, check both points exist
        bool has0_5 = false, has10_5 = false;
        for (const auto& pt : res) {
            if (geom::nearlyEqual(pt.x_, 0.0) && geom::nearlyEqual(pt.y_, 5.0)) has0_5 = true;
            if (geom::nearlyEqual(pt.x_, 10.0) && geom::nearlyEqual(pt.y_, 5.0)) has10_5 = true;
        }
        check(has0_5, "Has intersection at (0,5)");
        check(has10_5, "Has intersection at (10,5)");
    }
}

void testTrimLine() {
    std::printf("\n=== testTrimLine ===\n");
    
    // Line trimmed by another line
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 0));
    LineEntity* l = line.get();
    
    auto cuttingLine = std::make_unique<LineEntity>(Point2(5, -5), Point2(5, 5));
    
    check(trimLine(*l, *cuttingLine, Point2(8, 0)), "Trim line at intersection");
    check(nearlyEqual(l->end().x_, 5.0) && nearlyEqual(l->end().y_, 0.0), "Line trimmed to x=5");
}

void testExtendLine() {
    std::printf("\n=== testExtendLine ===\n");
    
    // Extend line to boundary line
    auto line = std::make_unique<LineEntity>(Point2(0, 0), Point2(5, 0));
    LineEntity* l = line.get();
    
    auto boundary = std::make_unique<LineEntity>(Point2(10, -5), Point2(10, 5));
    
    check(extendLine(*l, *boundary, Point2(7, 0)), "Extend line to boundary");
    check(nearlyEqual(l->end().x_, 10.0) && nearlyEqual(l->end().y_, 0.0), "Line extended to x=10");
}

void testBreakLine() {
    std::printf("\n=== testBreakLine ===\n");
    
    LineEntity line(Point2(0, 0), Point2(10, 0));
    auto [l1, l2] = breakLine(line, Point2(5, 0));
    
    check(l1 != nullptr && l2 != nullptr, "Break creates two lines");
    if (l1 && l2) {
        check(nearlyEqual(l1->start().x_, 0.0) && nearlyEqual(l1->end().x_, 5.0), "First line 0 to 5");
        check(nearlyEqual(l2->start().x_, 5.0) && nearlyEqual(l2->end().x_, 10.0), "Second line 5 to 10");
    }
}

void testBreakPolyline() {
    std::printf("\n=== testBreakPolyline ===\n");
    
    PolylineEntity poly({Point2(0, 0), Point2(10, 0), Point2(10, 10)}, false);
    breakPolyline(poly, Point2(5, 0));
    
    check(poly.vertices().size() == 4, "Polyline gets new vertex");
    // New vertex inserted at index 1 (after first vertex, splitting segment 0-1)
    check(geom::nearlyEqual(poly.vertices()[1].x_, 5.0) && geom::nearlyEqual(poly.vertices()[1].y_, 0.0), "New vertex at index 1: (5,0)");
}

void testOffsetLine() {
    std::printf("\n=== testOffsetLine ===\n");
    
    LineEntity line(Point2(0, 0), Point2(10, 0));
    auto offset = offsetLine(line, 5.0, Point2(0, 5)); // sidePoint above line
    
    check(offset != nullptr, "Offset creates new line");
    if (offset) {
        check(nearlyEqual(offset->start().y_, 5.0), "Offset line at y=5");
        check(nearlyEqual(offset->end().y_, 5.0), "Offset line at y=5");
    }
}

void testOffsetCircle() {
    std::printf("\n=== testOffsetCircle ===\n");
    
    CircleEntity circle(Point2(0, 0), 5.0);
    auto offset = offsetCircle(circle, 2.0, Point2(0, 10)); // outside
    
    check(offset != nullptr, "Offset creates new circle");
    if (offset) {
        check(nearlyEqual(offset->radius(), 7.0), "New radius = 7");
    }
    
    // Negative offset (inside)
    auto offset2 = offsetCircle(circle, -2.0, Point2(0, 10));
    check(offset2 != nullptr, "Negative offset creates new circle");
    if (offset2) {
        check(nearlyEqual(offset2->radius(), 3.0), "New radius = 3");
    }
    
    // Offset that would make negative radius
    auto offset3 = offsetCircle(circle, -10.0, Point2(0, 10));
    check(offset3 != nullptr, "Negative offset clamped to 0");
    if (offset3) {
        check(nearlyEqual(offset3->radius(), 0.0), "Radius clamped to 0");
    }
}

void testOffsetPolyline() {
    std::printf("\n=== testOffsetPolyline ===\n");
    
    // Square polyline
    PolylineEntity poly({Point2(0, 0), Point2(10, 0), Point2(10, 10), Point2(0, 10)}, true);
    auto offset = offsetPolyline(poly, 2.0, Point2(-5, 5)); // outside square
    
    check(offset != nullptr, "Offset creates new polyline");
    if (offset) {
        check(offset->vertices().size() == 4, "Offset polyline has 4 vertices");
        // First vertex should be offset from origin
        check(offset->vertices()[0].x_ != 0.0 || offset->vertices()[0].y_ != 0.0, "First vertex offset from origin");
    }
}

void testEntityIntersections() {
    std::printf("\n=== testEntityIntersections ===\n");
    
    // Line / Line
    auto l1 = std::make_unique<LineEntity>(Point2(0, 0), Point2(10, 10));
    auto l2 = std::make_unique<LineEntity>(Point2(0, 10), Point2(10, 0));
    auto res = entityIntersections(*l1, *l2);
    check(res.size() == 1, "Line/Line intersection");
    if (!res.empty()) check(nearlyEqual(res[0].x_, 5.0) && nearlyEqual(res[0].y_, 5.0), "At (5,5)");
    
    // Line / Circle
    auto line = std::make_unique<LineEntity>(Point2(-10, 0), Point2(10, 0));
    auto circle = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    res = entityIntersections(*line, *circle);
    check(res.size() == 2, "Line/Circle 2 intersections");
    
    // Circle / Circle
    auto c1 = std::make_unique<CircleEntity>(Point2(0, 0), 5.0);
    auto c2 = std::make_unique<CircleEntity>(Point2(8, 0), 5.0);
    res = entityIntersections(*c1, *c2);
    check(res.size() == 2, "Circle/Circle 2 intersections");
    
    // Line / Polyline
    auto line2 = std::make_unique<LineEntity>(Point2(-5, 5), Point2(15, 5));
    auto poly = std::make_unique<PolylineEntity>(std::vector<Point2>{Point2(0, 0), Point2(10, 0), Point2(10, 10), Point2(0, 10)}, true);
    res = entityIntersections(*line2, *poly);
    check(res.size() == 2, "Line/Polyline 2 intersections");
}

} // namespace

int main() {
    testIntersectLineLine();
    testIntersectSegmentSegment();
    testIntersectLineCircle();
    testIntersectCircleCircle();
    testIntersectLinePolyline();
    testTrimLine();
    testExtendLine();
    testBreakLine();
    testBreakPolyline();
    testOffsetLine();
    testOffsetCircle();
    testOffsetPolyline();
    testEntityIntersections();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d test(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll GeometryUtils2 tests PASSED\n");
    return 0;
}