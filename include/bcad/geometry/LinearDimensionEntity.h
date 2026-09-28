#pragma once

#include "bcad/geometry/DimensionEntity.h"

namespace bcad::geom {

class LinearDimensionEntity : public DimensionEntity {
public:
    LinearDimensionEntity() = default;
    LinearDimensionEntity(Point2 defPt1, Point2 defPt2, Point2 dimLineLoc, double rotation = 0.0, std::string style = "Standard");
    
    Point2 defPt1() const { return defPt1_; }
    Point2 defPt2() const { return defPt2_; }
    void setDefPt1(Point2 p) { defPt1_ = p; }
    void setDefPt2(Point2 p) { defPt2_ = p; }
    
    Point2 dimLineLoc() const { return dimLineLoc_; }
    void setDimLineLoc(Point2 p) { dimLineLoc_ = p; }
    
    double rotation() const { return rotation_; }
    void setRotation(double r) { rotation_ = r; }
    
    TypeId typeId() const override { return TypeId_LinearDimension; }
    EntityType type() const override { return EntityType::Polyline; }
    DimensionEntityType dimensionType() const override { return DimensionEntityType::Linear; }
    
    const std::string& styleName() const override { return styleName_; }
    void setStyleName(std::string name) override { styleName_ = std::move(name); }
    
    std::string dimensionText() const override;
    double measuredValue() const override;
    double distanceTo(const Point2& p) const override;
    
    BoundingBox boundingBox() const override;
    void applyTransform(const Transform2D& t) override;
    std::unique_ptr<Entity> clone() const override;
    
    std::string serializeParams() const override;
    void writeDxf(std::ostream& f, const std::string& layer, const std::optional<Color>& colorOverride) const override;
    std::string geometryInfo() const override;
    
    void doAddSnapCandidates(const Point2& cursor, SnapCallback add) const override;
    std::vector<Point2> tessellate(double maxDeviation) const override;

private:
    Point2 defPt1_;
    Point2 defPt2_;
    Point2 dimLineLoc_;
    double rotation_ = 0.0;
    std::string styleName_ = "Standard";
};

} // namespace bcad::geom