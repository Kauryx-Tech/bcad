// LinearDimensionEntity implementation

#include "bcad/geometry/LinearDimensionEntity.h"
#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/layout/DimensionStyle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/Point.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace bcad::geom {

// LinearDimensionEntity implementation
LinearDimensionEntity::LinearDimensionEntity(Point2 defPt1, Point2 defPt2, Point2 dimLineLoc, double rotation, std::string style)
    : defPt1_(defPt1), defPt2_(defPt2), dimLineLoc_(dimLineLoc), rotation_(rotation), styleName_(std::move(style)) {}

std::string LinearDimensionEntity::dimensionText() const {
    layout::DimensionStyle style;
    style.name = styleName_;
    return formatDimensionText(measuredValue(), style);
}

double LinearDimensionEntity::measuredValue() const {
    // Distance mesuree le long de la direction de cote (0 : horizontale,
    // pi/2 : verticale), comme la cotation lineaire d'AutoCAD.
    const double dx = defPt2_.x_ - defPt1_.x_;
    const double dy = defPt2_.y_ - defPt1_.y_;
    return std::abs(dx * std::cos(rotation_) + dy * std::sin(rotation_));
}

void LinearDimensionEntity::applyTransform(const Transform2D& t) {
    defPt1_ = t.transform(defPt1_);
    defPt2_ = t.transform(defPt2_);
    dimLineLoc_ = t.transform(dimLineLoc_);
    // La direction de cote suit la transformation, symetrie comprise : sinon
    // une cote miroir garderait son ancienne direction et mesurerait faux.
    const auto& m = t.matrix();
    const double c = std::cos(rotation_), s = std::sin(rotation_);
    const double dx = m.m00 * c + m.m01 * s;
    const double dy = m.m10 * c + m.m11 * s;
    if (std::hypot(dx, dy) > 1e-12) rotation_ = std::atan2(dy, dx);
}

std::unique_ptr<Entity> LinearDimensionEntity::clone() const {
    return std::make_unique<LinearDimensionEntity>(*this);
}

std::string LinearDimensionEntity::serializeParams() const {
    std::ostringstream ss;
    ss.precision(17);
    ss << defPt1_.x_ << ',' << defPt1_.y_ << ','
       << defPt2_.x_ << ',' << defPt2_.y_ << ','
       << dimLineLoc_.x_ << ',' << dimLineLoc_.y_ << ','
       << rotation_ << ','
       << styleName_;
    return ss.str();
}

void LinearDimensionEntity::writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const {
    writeDimensionDxf(f, *this, layer, colorOverride);
}

std::string LinearDimensionEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << "Cotation linéaire\n";
    ss << "  origine 1 : (" << defPt1_.x_ << ", " << defPt1_.y_ << ")\n";
    ss << "  origine 2 : (" << defPt2_.x_ << ", " << defPt2_.y_ << ")\n";
    ss << "  ligne de cote : (" << dimLineLoc_.x_ << ", " << dimLineLoc_.y_ << ")\n";
    ss << "  rotation : " << rotation_ << "\n";
    ss << "  valeur : " << measuredValue() << "\n";
    ss << "  style : " << styleName_;
    return ss.str();
}

void LinearDimensionEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    add(defPt1_, SnapPointType::Endpoint);
    add(defPt2_, SnapPointType::Endpoint);
    add(dimLineLoc_, SnapPointType::Midpoint);
}

std::vector<Point2> LinearDimensionEntity::tessellate(double maxDeviation) const {
    (void)maxDeviation;
    return dimensionGraphics(*this).path;
}

BoundingBox LinearDimensionEntity::boundingBox() const {
    return dimensionBounds(*this);
}

double LinearDimensionEntity::distanceTo(const Point2& p) const {
    return dimensionDistance(*this, p);
}

} // namespace bcad::geom