// AngularDimensionEntity implementation

#include "bcad/geometry/AngularDimensionEntity.h"
#include "bcad/geometry/DimensionGraphics.h"
#include "bcad/layout/DimensionStyle.h"
#include "bcad/geometry/GeometryUtils.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Arc.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/Point.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace bcad::geom {

AngularDimensionEntity::AngularDimensionEntity(Point2 vertex, Point2 start, Point2 end, Point2 dimLineLoc, std::string style)
    : vertex_(vertex), start_(start), end_(end), dimLineLoc_(dimLineLoc), styleName_(std::move(style)) {}

std::string AngularDimensionEntity::dimensionText() const {
    layout::DimensionStyle style;
    style.name = styleName_;
    style.precision = 1;
    style.suffix = "\xC2\xB0";   // °
    return formatDimensionText(measuredValue(), style);
}

double AngularDimensionEntity::measuredValue() const {
    double v1x = start_.x_ - vertex_.x_;
    double v1y = start_.y_ - vertex_.y_;
    double v2x = end_.x_ - vertex_.x_;
    double v2y = end_.y_ - vertex_.y_;
    
    double dot = v1x * v2x + v1y * v2y;
    double det = v1x * v2y - v1y * v2x;
    double ang = std::atan2(std::abs(det), dot) * 180.0 / M_PI;
    return ang;
}

void AngularDimensionEntity::applyTransform(const Transform2D& t) {
    vertex_ = t.transform(vertex_);
    start_ = t.transform(start_);
    end_ = t.transform(end_);
    dimLineLoc_ = t.transform(dimLineLoc_);
}

std::unique_ptr<Entity> AngularDimensionEntity::clone() const {
    return std::make_unique<AngularDimensionEntity>(*this);
}

std::string AngularDimensionEntity::serializeParams() const {
    std::ostringstream ss;
    ss.precision(17);
    ss << vertex_.x_ << ',' << vertex_.y_ << ','
       << start_.x_ << ',' << start_.y_ << ','
       << end_.x_ << ',' << end_.y_ << ','
       << dimLineLoc_.x_ << ',' << dimLineLoc_.y_ << ','
       << styleName_;
    return ss.str();
}

void AngularDimensionEntity::writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const {
    writeDimensionDxf(f, *this, layer, colorOverride);
}

std::string AngularDimensionEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << "Cotation angulaire\n";
    ss << "  sommet : (" << vertex_.x_ << ", " << vertex_.y_ << ")\n";
    ss << "  premier côté : (" << start_.x_ << ", " << start_.y_ << ")\n";
    ss << "  second côté : (" << end_.x_ << ", " << end_.y_ << ")\n";
    ss << "  ligne de cote : (" << dimLineLoc_.x_ << ", " << dimLineLoc_.y_ << ")\n";
    ss << "  valeur : " << measuredValue() << "°\n";
    ss << "  style : " << styleName_;
    return ss.str();
}

void AngularDimensionEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    add(vertex_, SnapPointType::Endpoint);
    add(start_, SnapPointType::Endpoint);
    add(end_, SnapPointType::Endpoint);
    add(dimLineLoc_, SnapPointType::Midpoint);
}

std::vector<Point2> AngularDimensionEntity::tessellate(double maxDeviation) const {
    (void)maxDeviation;
    return dimensionGraphics(*this).path;
}

BoundingBox AngularDimensionEntity::boundingBox() const {
    return dimensionBounds(*this);
}

double AngularDimensionEntity::distanceTo(const Point2& p) const {
    return dimensionDistance(*this, p);
}

} // namespace bcad::geom