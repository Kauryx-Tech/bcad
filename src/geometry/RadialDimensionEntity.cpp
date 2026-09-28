// RadialDimensionEntity implementation

#include "bcad/geometry/RadialDimensionEntity.h"
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
    return std::make_unique<RadialDimensionEntity>(center_, chordPoint_, radialType_, dimLineLoc_, styleName_);
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
    // Calculate angle from center to chord point
    double dx = chordPoint_.x_ - center_.x_;
    double dy = chordPoint_.y_ - center_.y_;
    double angle = std::atan2(dy, dx);
    
    const Color& c = colorOverride.value_or(Color::fromRgb255(0, 0, 0));
    
    f << "0\nDIMENSION\n";
    f << "5\n" << id() << "\n";
    f << "100\nAcDbEntity\n";
    f << "8\n" << layer << "\n";
    f << "62\n" << static_cast<int>(c.r * 255) << "\n";
    f << "100\nAcDbDimension\n";
    f << "10\n" << center_.x_ << "\n";
    f << "20\n" << center_.y_ << "\n";
    f << "11\n" << chordPoint_.x_ << "\n";
    f << "21\n" << chordPoint_.y_ << "\n";
    f << "13\n" << dimLineLoc_.x_ << "\n";
    f << "23\n" << dimLineLoc_.y_ << "\n";
    f << "51\n0\n";  // rotation
    f << "70\n" << (radialType_ == RadialType::Radius ? 3 : 4) << "\n";  // 3 = radius, 4 = diameter
    f << "71\n5\n";
    f << "72\n0\n";
    f << "73\n1\n";
    f << "100\nAcDbRadialDimension\n";
    f << "0\n";
}

std::string RadialDimensionEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << (radialType_ == RadialType::Radius ? "RadiusDimension" : "DiameterDimension") << "\n";
    ss << "  center: (" << center_.x_ << ", " << center_.y_ << ")\n";
    ss << "  chordPoint: (" << chordPoint_.x_ << ", " << chordPoint_.y_ << ")\n";
    ss << "  dimLineLoc: (" << dimLineLoc_.x_ << ", " << dimLineLoc_.y_ << ")\n";
    ss << "  value: " << measuredValue() << "\n";
    ss << "  style: " << styleName_;
    return ss.str();
}

void RadialDimensionEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    add(center_, SnapPointType::Center);
    add(chordPoint_, SnapPointType::Endpoint);
    add(dimLineLoc_, SnapPointType::Midpoint);
}

std::vector<Point2> RadialDimensionEntity::tessellate(double maxDeviation) const {
    std::vector<Point2> pts;
    pts.push_back(center_);
    pts.push_back(chordPoint_);
    pts.push_back(dimLineLoc_);
    return pts;
}

BoundingBox RadialDimensionEntity::boundingBox() const {
    BoundingBox bb;
    bb.expand(center_);
    bb.expand(chordPoint_);
    bb.expand(dimLineLoc_);
    return bb;
}

double RadialDimensionEntity::distanceTo(const Point2& p) const {
    double dx = p.x_ - center_.x_;
    double dy = p.y_ - center_.y_;
    double distToCenter = std::hypot(dx, dy);
    double radius = measuredValue();
    if (radialType_ == RadialType::Diameter) {
        radius /= 2.0;
    }
    return std::abs(distToCenter - radius);
}

} // namespace bcad::geom