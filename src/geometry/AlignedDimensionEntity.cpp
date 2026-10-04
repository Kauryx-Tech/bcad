// AlignedDimensionEntity implementation

#include "bcad/geometry/AlignedDimensionEntity.h"
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
    return std::make_unique<AlignedDimensionEntity>(defPt1_, defPt2_, dimLineLoc_, styleName_);
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
    // Calculate angle of dimension line
    double dx = defPt2_.x_ - defPt1_.x_;
    double dy = defPt2_.y_ - defPt1_.y_;
    double angle = std::atan2(dy, dx);
    
    const Color& c = colorOverride.value_or(Color::fromRgb255(0, 0, 0));
    
    f << "0\nDIMENSION\n";
    f << "5\n" << id() << "\n";
    f << "100\nAcDbEntity\n";
    f << "8\n" << layer << "\n";
    f << "62\n" << static_cast<int>(c.r * 255) << "\n";
    f << "100\nAcDbDimension\n";
    f << "10\n" << defPt1_.x_ << "\n";
    f << "20\n" << defPt1_.y_ << "\n";
    f << "11\n" << defPt2_.x_ << "\n";
    f << "21\n" << defPt2_.y_ << "\n";
    f << "13\n" << dimLineLoc_.x_ << "\n";
    f << "23\n" << dimLineLoc_.y_ << "\n";
    f << "51\n" << angle << "\n";
    f << "70\n1\n";  // aligned dimension type
    f << "71\n5\n";
    f << "72\n0\n";
    f << "73\n1\n";
    f << "100\nAcDbAlignedDimension\n";
    f << "0\n";
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
    std::vector<Point2> pts;
    pts.push_back(defPt1_);
    pts.push_back(defPt2_);
    pts.push_back(dimLineLoc_);
    return pts;
}

BoundingBox AlignedDimensionEntity::boundingBox() const {
    BoundingBox bb;
    bb.expand(defPt1_);
    bb.expand(defPt2_);
    bb.expand(dimLineLoc_);
    return bb;
}

double AlignedDimensionEntity::distanceTo(const Point2& p) const {
    bcad::geom::Point2 closest = bcad::geom::closestPointOnSegment(p, defPt1_, defPt2_);
    return bcad::geom::distance(p, closest);
}

} // namespace bcad::geom