// RadialDimensionEntity implementation

#include "bcad/geometry/RadialDimensionEntity.h"
#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/layout/DimensionStyle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/Point.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace bcad::geom {

RadialDimensionEntity::RadialDimensionEntity(Point2 center, Point2 chordPoint, RadialType type, Point2 dimLineLoc, std::string style)
    : center_(center), chordPoint_(chordPoint), radialType_(type), dimLineLoc_(dimLineLoc), styleName_(std::move(style)) {}

std::string RadialDimensionEntity::dimensionText() const {
    layout::DimensionStyle style;
    style.name = styleName_;
    style.prefix = radialType_ == RadialType::Radius ? "R " : "\xC3\x98 ";   // Ø
    return formatDimensionText(measuredValue(), style);
}

double RadialDimensionEntity::measuredValue() const {
    double dx = chordPoint_.x_ - center_.x_;
    double dy = chordPoint_.y_ - center_.y_;
    double radius = std::hypot(dx, dy);
    return radialType_ == RadialType::Diameter ? radius * 2.0 : radius;
}

void RadialDimensionEntity::applyTransform(const Transform2D& t) {
    center_ = t.transform(center_);
    chordPoint_ = t.transform(chordPoint_);
    dimLineLoc_ = t.transform(dimLineLoc_);
}

std::unique_ptr<Entity> RadialDimensionEntity::clone() const {
    return std::make_unique<RadialDimensionEntity>(*this);
}

std::string RadialDimensionEntity::serializeParams() const {
    std::ostringstream ss;
    ss.precision(17);
    ss << center_.x_ << ',' << center_.y_ << ','
       << chordPoint_.x_ << ',' << chordPoint_.y_ << ','
       << static_cast<int>(radialType_) << ','
       << dimLineLoc_.x_ << ',' << dimLineLoc_.y_ << ','
       << styleName_;
    return ss.str();
}

void RadialDimensionEntity::writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const {
    writeDimensionDxf(f, *this, layer, colorOverride);
}

std::string RadialDimensionEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << (radialType_ == RadialType::Radius ? "Cotation de rayon" : "Cotation de diamètre") << "\n";
    ss << "  centre : (" << center_.x_ << ", " << center_.y_ << ")\n";
    ss << "  point du cercle : (" << chordPoint_.x_ << ", " << chordPoint_.y_ << ")\n";
    ss << "  ligne de cote : (" << dimLineLoc_.x_ << ", " << dimLineLoc_.y_ << ")\n";
    ss << "  valeur : " << measuredValue() << "\n";
    ss << "  style : " << styleName_;
    return ss.str();
}

void RadialDimensionEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    add(center_, SnapPointType::Center);
    add(chordPoint_, SnapPointType::Endpoint);
    add(dimLineLoc_, SnapPointType::Midpoint);
}

std::vector<Point2> RadialDimensionEntity::tessellate(double maxDeviation) const {
    (void)maxDeviation;
    return dimensionGraphics(*this).path;
}

BoundingBox RadialDimensionEntity::boundingBox() const {
    return dimensionBounds(*this);
}

double RadialDimensionEntity::distanceTo(const Point2& p) const {
    return dimensionDistance(*this, p);
}

} // namespace bcad::geom