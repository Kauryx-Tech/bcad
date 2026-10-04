// AlignedDimensionEntity implementation

#include "bcad/geometry/AlignedDimensionEntity.h"
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

AlignedDimensionEntity::AlignedDimensionEntity(Point2 defPt1, Point2 defPt2, Point2 dimLineLoc, std::string style)
    : defPt1_(defPt1), defPt2_(defPt2), dimLineLoc_(dimLineLoc), styleName_(std::move(style)) {}

std::string AlignedDimensionEntity::dimensionText() const {
    layout::DimensionStyle style;
    style.name = styleName_;
    return formatDimensionText(measuredValue(), style);
}

double AlignedDimensionEntity::measuredValue() const {
    double dx = defPt2_.x_ - defPt1_.x_;
    double dy = defPt2_.y_ - defPt1_.y_;
    return std::hypot(dx, dy);
}

void AlignedDimensionEntity::applyTransform(const Transform2D& t) {
    defPt1_ = t.transform(defPt1_);
    defPt2_ = t.transform(defPt2_);
    dimLineLoc_ = t.transform(dimLineLoc_);
}

std::unique_ptr<Entity> AlignedDimensionEntity::clone() const {
    return std::make_unique<AlignedDimensionEntity>(*this);
}

std::string AlignedDimensionEntity::serializeParams() const {
    std::ostringstream ss;
    ss.precision(17);
    ss << defPt1_.x_ << ',' << defPt1_.y_ << ','
       << defPt2_.x_ << ',' << defPt2_.y_ << ','
       << dimLineLoc_.x_ << ',' << dimLineLoc_.y_ << ','
       << styleName_;
    return ss.str();
}

void AlignedDimensionEntity::writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const {
    writeDimensionDxf(f, *this, layer, colorOverride);
}

std::string AlignedDimensionEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << "Cotation alignée\n";
    ss << "  origine 1 : (" << defPt1_.x_ << ", " << defPt1_.y_ << ")\n";
    ss << "  origine 2 : (" << defPt2_.x_ << ", " << defPt2_.y_ << ")\n";
    ss << "  ligne de cote : (" << dimLineLoc_.x_ << ", " << dimLineLoc_.y_ << ")\n";
    ss << "  valeur : " << measuredValue() << "\n";
    ss << "  style : " << styleName_;
    return ss.str();
}

void AlignedDimensionEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    add(defPt1_, SnapPointType::Endpoint);
    add(defPt2_, SnapPointType::Endpoint);
    add(dimLineLoc_, SnapPointType::Midpoint);
}

std::vector<Point2> AlignedDimensionEntity::tessellate(double maxDeviation) const {
    (void)maxDeviation;
    return dimensionGraphics(*this).path;
}

BoundingBox AlignedDimensionEntity::boundingBox() const {
    return dimensionBounds(*this);
}

double AlignedDimensionEntity::distanceTo(const Point2& p) const {
    return dimensionDistance(*this, p);
}

} // namespace bcad::geom