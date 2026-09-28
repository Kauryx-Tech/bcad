#pragma once

#include "bcad/geometry/DimensionEntity.h"
#include "bcad/geometry/Point.h"

namespace bcad::geom {

class AngularDimensionEntity : public DimensionEntity {
public:
    AngularDimensionEntity() = default;
    AngularDimensionEntity(Point2 vertex, Point2 start, Point2 end, Point2 dimLineLoc, std::string style = "Standard");
    
    Point2 vertex() const { return vertex_; }
    Point2 start() const { return start_; }
    Point2 end() const { return end_; }
    void setVertex(Point2 p) { vertex_ = p; }
    void setStart(Point2 p) { start_ = p; }
    void setEnd(Point2 p) { end_ = p; }
    
    Point2 dimLineLoc() const { return dimLineLoc_; }
    void setDimLineLoc(Point2 p) { dimLineLoc_ = p; }
    
    TypeId typeId() const override { return TypeId_AngularDimension; }
    EntityType type() const override { return EntityType::Polyline; }
    DimensionEntityType dimensionType() const override { return DimensionEntityType::Angular; }
    
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
    Point2 vertex_;
    Point2 start_;
    Point2 end_;
    Point2 dimLineLoc_;
    std::string styleName_ = "Standard";
};

} // namespace bcad::geom