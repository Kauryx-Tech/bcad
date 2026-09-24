#include "SurveyToleranceValidator.h"
#include "bcad/geometry/GeometryUtils.h"
#include <algorithm>
#include <limits>

namespace bcad::cadastre {

SurveyToleranceValidator::Result SurveyToleranceValidator::check(
    const geom::PolylineEntity& measured,
    const geom::PolylineEntity& legal) const {
    const auto& measuredVertices = measured.vertices();
    const auto& legalVertices = legal.vertices();
    if (measuredVertices.size() != legalVertices.size() || measuredVertices.empty()) {
        return {false, std::numeric_limits<double>::infinity()};
    }

    double maximumDeviation = 0.0;
    for (std::size_t i = 0; i < measuredVertices.size(); ++i) {
        maximumDeviation = std::max(
            maximumDeviation, geom::distance(measuredVertices[i], legalVertices[i]));
    }
    return {maximumDeviation <= tolerance_, maximumDeviation};
}

} // namespace bcad::cadastre
