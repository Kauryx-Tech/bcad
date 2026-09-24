#pragma once

#include "bcad/geometry/Polyline.h"

namespace bcad::cadastre {

class SurveyToleranceValidator {
public:
    struct Result {
        bool withinTolerance = true;
        double deviation = 0.0;
    };

    explicit SurveyToleranceValidator(double tolerance = 0.02)
        : tolerance_(tolerance) {}

    Result check(const geom::PolylineEntity& measured,
                 const geom::PolylineEntity& legal) const;

private:
    double tolerance_;
};

} // namespace bcad::cadastre
