#pragma once

// Dimension entity types for DXF DIMENSION support.
// Base class and specific types: Linear, Aligned, Angular, Radius, Diameter.

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/Point.h"
#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Transform2D.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/layout/DimensionStyle.h"
#include <string>
#include <vector>
#include <optional>

namespace bcad::geom {

// Arrowhead types for dimension lines
enum class DimensionArrowhead : uint8_t {
    ClosedFilled = 0,
    ClosedBlank = 1,
    Closed = 2,
    Dot = 3,
    DotSmall = 4,
    DotBlank = 5,
    Origin = 6,
    Origin2 = 7,
    Open = 8,
    Open90 = 9,
    Open30 = 10,
    ClosedSmall = 11,
    Triangle = 12,
    TriangleSmall = 13,
    TriangleBlank = 14,
    Integral = 15,
    None = 16
};

// Dimension entity types
enum class DimensionEntityType : uint8_t {
    Linear = 0,       // horizontal/vertical/rotated linear
    Aligned = 1,      // aligned to entity (parallel to measured segment)
    Angular = 2,      // angular (3-point)
    Radius = 3,       // radius (circle/arc)
    Diameter = 4      // diameter (circle/arc)
};

// Dimension entity base class
class DimensionEntity : public Entity {
public:
    virtual ~DimensionEntity();

    // Dimension type
    virtual DimensionEntityType dimensionType() const = 0;
    
    // Style name (references DimensionStyle by name)
    virtual const std::string& styleName() const = 0;
    virtual void setStyleName(std::string name) = 0;
    
    // Override: text displaying the dimension value
    virtual std::string dimensionText() const = 0;
    
    // Override: measurement value in world units
    virtual double measuredValue() const = 0;
    
    // Override: tessellate for rendering (dimension line + extension lines + arrows + text)
    virtual std::vector<Point2> tessellate(double maxDeviation) const = 0;
    
    // Distance for picking
    virtual double distanceTo(const Point2& p) const = 0;

    // PropertyMap access
    properties::PropertyMap& properties() override;
    const properties::PropertyMap& properties() const override;

protected:
    // Helper to draw arrowhead at point, pointing along direction
    static std::vector<Point2> makeArrowhead(const Point2& tip, const Point2& direction, double size, layout::ArrowheadType type);
    
    // Helper to format dimension text with style
    std::string formatDimensionText(double value, const layout::DimensionStyle& style) const;
    
    // PropertyMap for dimension properties
    mutable properties::PropertyMap properties_;
    
    // Dimension-specific data
    std::string styleName_ = "Standard";
};

} // namespace bcad::geom