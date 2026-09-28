// LinearDimensionEntity implementation

#include "bcad/geometry/LinearDimensionEntity.h"
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
    double dx = defPt2_.x_ - defPt1_.x_;
    double dy = defPt2_.y_ - defPt1_.y_;
    return std::hypot(dx, dy);
}

void LinearDimensionEntity::applyTransform(const Transform2D& t) {
    defPt1_ = t.transform(defPt1_);
    defPt2_ = t.transform(defPt2_);
    dimLineLoc_ = t.transform(dimLineLoc_);
    // Extract rotation from transform matrix
    double det = t.matrix().m00 * t.matrix().m11 - t.matrix().m01 * t.matrix().m10;
    if (det > 0) {
        double rot = std::atan2(t.matrix().m10, t.matrix().m00);
        rotation_ += rot;
    }
}

std::unique_ptr<Entity> LinearDimensionEntity::clone() const {
    return std::make_unique<LinearDimensionEntity>(defPt1_, defPt2_, dimLineLoc_, rotation_, styleName_);
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
    // DXF DIMENSION entity: linear (type 0)
    // 0 DIMENSION
    // 100 AcDbEntity
    // 8 layer
    // 100 AcDbDimension
    // 10 defPt1.x
    // 20 defPt1.y
    // 11 defPt2.x
    // 21 defPt2.y
    // 13 dimLineLoc.x
    // 23 dimLineLoc.y
    // 51 rotation (radians)
    // 70 dimension type (0 = linear)
    // 71 attachment point
    // 72 line spacing style
    // 73 line spacing factor
    // 100 AcDbAlignedDimension (if aligned) or AcDbRotatedDimension (if rotated)
    // 50 angle of dimension line (for rotated)
    
    const Color& c = colorOverride.value_or(Color::fromRgb255(0, 0, 0));
    
    f << "0\nDIMENSION\n";
    f << "5\n" << id() << "\n";  // handle placeholder
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
    f << "51\n" << rotation_ << "\n";  // rotation angle
    f << "70\n0\n";  // linear dimension type
    f << "71\n5\n";  // attachment point
    f << "72\n0\n";  // line spacing style
    f << "73\n1\n";  // line spacing factor
    f << "100\nAcDbRotatedDimension\n";
    f << "50\n" << rotation_ << "\n";  // angle of dimension line
    f << "0\n";
}

std::string LinearDimensionEntity::geometryInfo() const {
    std::ostringstream ss;
    ss.precision(3);
    ss << "LinearDimension\n";
    ss << "  defPt1: (" << defPt1_.x_ << ", " << defPt1_.y_ << ")\n";
    ss << "  defPt2: (" << defPt2_.x_ << ", " << defPt2_.y_ << ")\n";
    ss << "  dimLineLoc: (" << dimLineLoc_.x_ << ", " << dimLineLoc_.y_ << ")\n";
    ss << "  rotation: " << rotation_ << "\n";
    ss << "  value: " << measuredValue() << "\n";
    ss << "  style: " << styleName_;
    return ss.str();
}

void LinearDimensionEntity::doAddSnapCandidates(const Point2& cursor, SnapCallback add) const {
    add(defPt1_, SnapPointType::Endpoint);
    add(defPt2_, SnapPointType::Endpoint);
    add(dimLineLoc_, SnapPointType::Midpoint);
}

std::vector<Point2> LinearDimensionEntity::tessellate(double maxDeviation) const {
    // Dimension line + extension lines + arrows + text
    std::vector<Point2> pts;
    
    // For simplicity, return the measured segment + dimension line location
    pts.push_back(defPt1_);
    pts.push_back(defPt2_);
    pts.push_back(dimLineLoc_);
    
    return pts;
}

BoundingBox LinearDimensionEntity::boundingBox() const {
    BoundingBox bb;
    bb.expand(defPt1_);
    bb.expand(defPt2_);
    bb.expand(dimLineLoc_);
    return bb;
}

double LinearDimensionEntity::distanceTo(const Point2& p) const {
    // Distance to the dimension line segment
    Point2 closest = closestPointOnSegment(p, defPt1_, defPt2_);
    return distance(p, closest);
}

} // namespace bcad::geom