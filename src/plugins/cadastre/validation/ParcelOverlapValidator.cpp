#include "ParcelOverlapValidator.h"
#include "bcad/geometry/BooleanOps.h"
#include "bcad/geometry/Polyline.h"

#include <algorithm>
#include <cmath>

namespace bcad::cadastre {

namespace {
const geom::PolylineEntity* asClosedPolyline(const geom::Entity& entity) {
    const auto* polyline = dynamic_cast<const geom::PolylineEntity*>(&entity);
    return polyline && polyline->closed() && polyline->vertices().size() >= 3
        ? polyline
        : nullptr;
}
}

ParcelOverlapValidator::Result ParcelOverlapValidator::check(
    const geom::Entity& a, const geom::Entity& b) const {
    const auto* first = asClosedPolyline(a);
    const auto* second = asClosedPolyline(b);
    if (!first || !second) {
        return {false, "Les deux entités doivent être des polygones fermés"};
    }
    if (!first->boundingBox().intersects(second->boundingBox())) {
        return {false, {}};
    }

    // Intersection des anneaux extérieurs
    const auto intersection = geom::booleanOp(
        *first, *second, geom::BooleanOp::Intersection);
    double area = 0.0;
    for (const auto& polygon : intersection) {
        area += std::abs(geom::polygonArea(polygon));
    }

    // Soustraire les parties de l'intersection qui tombent dans les trous de
    // chaque parcelle : évite les faux positifs quand une voisine remplit le
    // trou d'une autre parcelle (§0.8).
    auto subtractHoleIntersections = [&](const geom::PolylineEntity& holed,
                                         const geom::PolylineEntity& other) {
        for (const auto& hole : holed.holes()) {
            if (hole.size() < 3) continue;
            const geom::PolylineEntity holeEntity(hole, true);
            if (!holeEntity.boundingBox().intersects(other.boundingBox())) continue;
            for (const auto& poly : geom::booleanOp(holeEntity, other, geom::BooleanOp::Intersection))
                area -= std::abs(geom::polygonArea(poly));
        }
    };
    subtractHoleIntersections(*first, *second);
    subtractHoleIntersections(*second, *first);
    area = std::max(area, 0.0);

    return {area > 1e-9, area > 1e-9
        ? "Les parcelles possèdent une emprise commune"
        : ""};
}

std::vector<ParcelOverlapValidator::Result> ParcelOverlapValidator::checkAll(
    const std::vector<geom::Entity*>& entities) const {
    std::vector<Result> results;
    for (std::size_t i = 0; i < entities.size(); ++i) {
        if (!entities[i]) continue;
        for (std::size_t j = i + 1; j < entities.size(); ++j) {
            if (!entities[j]) continue;
            results.push_back(check(*entities[i], *entities[j]));
        }
    }
    return results;
}

} // namespace bcad::cadastre
