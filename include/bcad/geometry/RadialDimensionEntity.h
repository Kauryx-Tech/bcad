#pragma once

#include "bcad/geometry/DimensionEntity.h"
#include "bcad/geometry/Point.h"

namespace bcad::geom {

class RadialDimensionEntity : public DimensionEntity {
public:
    enum class RadialType { Radius, Diameter };
    
    RadialDimensionEntity() = default;
    RadialDimensionEntity(Point2 center, Point2 chordPoint, RadialType type, Point2 dimLineLoc, std::string style = "Standard");
    
    Point2 center() const { return center_; }
    Point2 chordPoint() const { return chordPoint_; }
    void setCenter(Point2 p) { center_ = p; }
    void setChordPoint(Point2 p) { chordPoint_ = p; }
    
    RadialType radialType() const { return radialType_; }
    void setRadialType(RadialType t) { radialType_ = t; }
    
    Point2 dimLineLoc() const { return dimLineLoc_; }
    void setDimLineLoc(Point2 p) { dimLineLoc_ = p; }
    
    TypeId typeId() const override { 
        return radialType_ == RadialType::Radius ? TypeId_RadiusDimension : TypeId_DiameterDimension; 
    }
    EntityType type() const override { return EntityType::Polyline; }
    DimensionEntityType dimensionType() const override { 
        return radialType_ == RadialType::Radius ? DimensionEntityType::Radius : DimensionEntityType::Diameter; 
    }
    
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
    Point2 center_;
    Point2 chordPoint_;
    RadialType radialType_ = RadialType::Radius;
    Point2 dimLineLoc_;
    std::string styleName_ = "Standard";
};

} // namespace bcad::geom