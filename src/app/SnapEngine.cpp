#include "SnapEngine.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/SnapGeometry.h"
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

namespace bcad::app {

namespace {

struct Candidate {
    geom::Point2 point;
    SnapType type;
    double dist;
};

void addPointCandidates(const geom::Entity& e, std::vector<Candidate>& out, const geom::Point2& cursor) {
    auto mapSnapType = [](geom::Entity::SnapPointType t) -> SnapType {
        switch (t) {
            case geom::Entity::SnapPointType::Endpoint: return SnapType::Endpoint;
            case geom::Entity::SnapPointType::Midpoint: return SnapType::Midpoint;
            case geom::Entity::SnapPointType::Center: return SnapType::Center;
            case geom::Entity::SnapPointType::Quadrant: return SnapType::Quadrant;
            case geom::Entity::SnapPointType::Intersection: return SnapType::Intersection;
            case geom::Entity::SnapPointType::Perpendicular: return SnapType::Perpendicular;
            case geom::Entity::SnapPointType::Nearest: return SnapType::Nearest;
        }
        return SnapType::Nearest;
    };

    e.addSnapCandidates(cursor, [&](const geom::Point2& p, geom::Entity::SnapPointType t) {
        out.push_back({ p, mapSnapType(t), geom::distance(cursor, p) });
    });
}

std::optional<Candidate> bestInTier(const std::vector<Candidate>& candidates, double tolerance,
                                     std::initializer_list<SnapType> tierTypes) {
    const Candidate* best = nullptr;
    for (const auto& c : candidates) {
        if (c.dist > tolerance) continue;
        for (SnapType t : tierTypes) {
            if (c.type == t && (!best || c.dist < best->dist)) best = &c;
        }
    }
    if (best) return *best;
    return std::nullopt;
}

} // namespace

SnapResult SnapEngine::findSnap(const core::Document& doc, const geom::Point2& cursor, double worldTolerance,
                                 std::optional<geom::Point2> referencePoint) const {
    geom::BoundingBox region = geom::BoundingBox::fromCenterHalfExtent(cursor, worldTolerance);
    std::vector<geom::Entity*> hits = doc.entitiesInRegion(region);

    std::vector<Candidate> candidates;
    for (geom::Entity* e : hits) addPointCandidates(*e, candidates, cursor);

    for (std::size_t i = 0; i < hits.size(); ++i) {
        for (std::size_t j = i + 1; j < hits.size(); ++j) {
            for (const geom::Point2& p : geom::entityIntersections(*hits[i], *hits[j])) {
                candidates.push_back({ p, SnapType::Intersection, geom::distance(cursor, p) });
            }
        }
    }

    if (auto c = bestInTier(candidates, worldTolerance,
                             { SnapType::Endpoint, SnapType::Center, SnapType::Intersection, SnapType::Quadrant })) {
        return { c->type, c->point };
    }
    if (auto c = bestInTier(candidates, worldTolerance, { SnapType::Midpoint })) {
        return { c->type, c->point };
    }

    // La perpendiculaire n'a de sens que par rapport à un point de
    // référence actif (le dernier point placé par l'outil), et elle
    // s'active sur la proximité à l'*entité* plutôt qu'au pied résultant,
    // qui peut se trouver loin du curseur le long de cette entité — cela
    // correspond au comportement du snap PER d'AutoCAD : survoler n'importe
    // où sur la ligne/cercle/arc qualifiant.
    if (referencePoint) {
        geom::Entity* nearest = nullptr;
        double nearestDist = std::numeric_limits<double>::infinity();
        for (geom::Entity* e : hits) {
            double d = e->distanceTo(cursor);
            if (d <= worldTolerance && d < nearestDist) {
                nearestDist = d;
                nearest = e;
            }
        }
        if (nearest) {
            if (auto foot = geom::perpendicularFoot(*nearest, *referencePoint, cursor)) {
                return { SnapType::Perpendicular, *foot };
            }
        }
    }

    // Nearest : dernier recours absolu, "accrocher à la courbe la plus
    // proche, quelle qu'elle soit". Le point le plus proche d'un point de
    // requête sur une entité et le pied de la perpendiculaire depuis ce
    // même point sont la même construction (pour une ligne, les deux sont
    // la projection sur le segment ; pour un cercle/arc, les deux se
    // trouvent le long du rayon centre-point) — donc ceci réutilise
    // perpendicularFoot avec le curseur servant lui-même de point de
    // référence.
    {
        const geom::Entity* bestEntity = nullptr;
        geom::Point2 bestPoint(0, 0);
        double bestDist = worldTolerance;
        for (geom::Entity* e : hits) {
            if (auto foot = geom::perpendicularFoot(*e, cursor, cursor)) {
                double d = geom::distance(cursor, *foot);
                if (d < bestDist) {
                    bestDist = d;
                    bestPoint = *foot;
                    bestEntity = e;
                }
            }
        }
        if (bestEntity) return { SnapType::Nearest, bestPoint };
    }

    return {};
}

} // namespace bcad::app
