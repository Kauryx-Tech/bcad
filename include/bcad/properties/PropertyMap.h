#pragma once

#include "bcad/geometry/Point.h"
#include "bcad/events/EventBus.h"
#include "bcad/properties/PropertyTypes.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace bcad::properties {

class Property {
public:
    Property(const std::string& name, PropertyType type, PropertyValue initial = {});
    virtual ~Property() = default;

    const std::string& name() const { return name_; }
    PropertyType type() const { return type_; }

    virtual std::unique_ptr<Property> clone() const;

    // Metadata
    void setUnit(const std::string& u) { unit_ = u; }
    const std::string& unit() const { return unit_; }
    void setDescription(const std::string& d) { description_ = d; }
    const std::string& description() const { return description_; }

    // Constraints
    void setRange(double min, double max) {
        min_ = min;
        max_ = max;
        hasRange_ = true;
    }
    void setEnumValues(const std::vector<std::string>& values) {
        enumValues_ = values;
    }
    void setReadOnly(bool ro) { readOnly_ = ro; }
    bool isReadOnly() const { return readOnly_; }

    // Accessors
    const PropertyValue& value() const { return value_; }
    PropertyValue& value() { return value_; }

    // Type-safe accessors
    double asDouble() const;
    int asInt() const;
    std::string asString() const;
    bool asBool() const;
    bcad::geom::Color asColor() const;
    int asEnum() const;
    const std::vector<std::string>& enumValues() const { return enumValues_; }

    // Mutation
    void setFromDouble(double v);
    void setFromInt(int v);
    void setFromString(const std::string& v);
    void setFromBool(bool v);
    void setFromColor(const bcad::geom::Color& v);
    void setFromEnum(int v);

    // Validation
    bool validate() const;

public:
    double min() const { return min_; }
    double max() const { return max_; }
    bool hasRange() const { return hasRange_; }

protected:
    std::string name_;
    PropertyType type_;
    PropertyValue value_;
    std::string unit_;
    std::string description_;
    double min_ = 0.0;
    double max_ = 0.0;
    std::vector<std::string> enumValues_;
    bool readOnly_ = false;
    bool hasRange_ = false;
    };

class PropertyMap {
public:
    PropertyMap() = default;
    PropertyMap(const PropertyMap& other);
    PropertyMap& operator=(const PropertyMap& other);
    PropertyMap(PropertyMap&&) noexcept = default;
    PropertyMap& operator=(PropertyMap&&) noexcept = default;

    // Creation
    Property* addDouble(const std::string& name, double initial = 0.0);
    Property* addInt(const std::string& name, int initial = 0);
    Property* addString(const std::string& name, const std::string& initial = "");
    Property* addBool(const std::string& name, bool initial = false);
    Property* addColor(const std::string& name, const bcad::geom::Color& initial = bcad::geom::Color{});
    Property* addEnum(const std::string& name, int initial = 0, const std::vector<std::string>& values = {});

    // Access
    Property* get(const std::string& name);
    const Property* get(const std::string& name) const;
    const Property* find(const std::string& name) const;
    bool has(const std::string& name) const;
    void remove(const std::string& name);

    // Helpers
    double getDouble(const std::string& name, double def = 0.0) const;
    int getInt(const std::string& name, int def = 0) const;
    std::string getString(const std::string& name) const;
    bool getBool(const std::string& name, bool def = false) const;
    bcad::geom::Color getColor(const std::string& name) const;
    int getEnum(const std::string& name, int def = 0) const;

    void setDouble(const std::string& name, double v);
    void setInt(const std::string& name, int v);
    void setString(const std::string& name, const std::string& v);
    void setBool(const std::string& name, bool v);
    void setColor(const std::string& name, const bcad::geom::Color& v);
    void setEnum(const std::string& name, int v);

    // Generic variant-based access (for generic UI/commands)
    PropertyValue getPropertyValue(const std::string& name) const;
    void set(const std::string& name, const PropertyValue& v);

    std::vector<std::string> listNames() const;

// Events
    bcad::events::EventBus& onChanged() { return bcad::events::EventBus::instance(); }

    // Serialization
    std::string serialize() const;
    void deserialize(const std::string& data);

private:
    std::unordered_map<std::string, std::unique_ptr<Property>> properties_;
};

} // namespace bcad::properties