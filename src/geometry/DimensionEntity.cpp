// DimensionEntity implementation

#include "bcad/geometry/DimensionEntity.h"
#include "bcad/layout/DimensionStyle.h"
#include "bcad/geometry/GeometryUtils.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace bcad::geom {

DimensionEntity::~DimensionEntity() = default;

// Helper: format dimension text with style
std::string DimensionEntity::formatDimensionText(double value, const layout::DimensionStyle& style) const {
    char buf[64];
    double displayValue = value;
    
    // Apply rounding if specified
    if (style.roundOff > 0) {
        double factor = std::pow(10.0, style.roundOff);
        displayValue = std::round(displayValue * factor) / factor;
    }
    
    char fmt[32];
    if (style.precision >= 0) {
        std::snprintf(fmt, sizeof(fmt), "%%.%df", style.precision);
    } else {
        std::snprintf(fmt, sizeof(fmt), "%%.0f");
    }
    std::snprintf(buf, sizeof(buf), fmt, displayValue);
    std::string result = buf;
    
    // Suppress trailing zeros if requested
    if (style.suppressZero) {
        size_t pos = result.find('.');
        if (pos != std::string::npos) {
            // Remove trailing zeros
            result.erase(result.find_last_not_of('0') + 1, std::string::npos);
            // Remove trailing dot
            if (result.back() == '.') result.pop_back();
        }
    }
    
    // Add prefix/suffix
    if (!style.prefix.empty()) result = style.prefix + result;
    if (!style.suffix.empty()) result = result + style.suffix;
    
    return result;
}

// Helper: arrowhead geometry
std::vector<Point2> DimensionEntity::makeArrowhead(const Point2& tip, const Point2& direction, double size, layout::ArrowheadType type) {
    std::vector<Point2> pts;
    if (type == layout::ArrowheadType::None || size <= 0.0) return pts;
    
    // Normalize direction
    double len = std::hypot(direction.x_, direction.y_);
    if (len == 0.0) return pts;
    
    double dx = direction.x_ / len;
    double dy = direction.y_ / len;
    double perpX = -dy;
    double perpY = dx;
    
    double baseX = tip.x_ - dx * size;
    double baseY = tip.y_ - dy * size;
    
    switch (type) {
        case layout::ArrowheadType::ClosedFilled:
        case layout::ArrowheadType::Closed:
            pts = {{tip.x_, tip.y_},
                   {baseX + perpX * size * 0.5, baseY + perpY * size * 0.5},
                   {baseX - perpX * size * 0.5, baseY - perpY * size * 0.5}};
            break;
        case layout::ArrowheadType::ClosedBlank:
            pts = {{tip.x_, tip.y_},
                   {baseX + perpX * size * 0.5, baseY + perpY * size * 0.5},
                   {baseX - perpX * size * 0.5, baseY - perpY * size * 0.5}};
            break;
        case layout::ArrowheadType::Dot:
        case layout::ArrowheadType::DotSmall:
            pts = {{tip.x_, tip.y_}};
            break;
        case layout::ArrowheadType::Open:
        case layout::ArrowheadType::Open90:
            pts = {{tip.x_, tip.y_},
                   {baseX + perpX * size * 0.5, baseY + perpY * size * 0.5}};
            break;
        case layout::ArrowheadType::Triangle:
            pts = {{tip.x_, tip.y_},
                   {baseX + perpX * size * 0.5, baseY + perpY * size * 0.5},
                   {baseX - perpX * size * 0.5, baseY - perpY * size * 0.5}};
            break;
        default:
            break;
    }
    return pts;
}

properties::PropertyMap& DimensionEntity::properties() {
    return properties_;
}

const properties::PropertyMap& DimensionEntity::properties() const {
    return properties_;
}

} // namespace bcad::geom