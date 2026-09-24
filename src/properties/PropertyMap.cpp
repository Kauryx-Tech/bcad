#include "bcad/properties/PropertyMap.h"
#include "bcad/events/EventBus.h"
#include <algorithm>
#include <cassert>
#include <sstream>
#include <stdexcept>

namespace bcad::properties {

using geom::Color;

// Property implementation

Property::Property(const std::string& name, PropertyType type, PropertyValue initial)
    : name_(name), type_(type), value_(std::move(initial)) {
}

std::unique_ptr<Property> Property::clone() const {
    auto copy = std::make_unique<Property>(name_, type_, value_);
    copy->unit_ = unit_;
    copy->description_ = description_;
    copy->min_ = min_;
    copy->max_ = max_;
    copy->enumValues_ = enumValues_;
    copy->readOnly_ = readOnly_;
    copy->hasRange_ = hasRange_;
    return copy;
}

double Property::asDouble() const {
    return std::get<double>(value_);
}

int Property::asInt() const {
    return std::get<int>(value_);
}

std::string Property::asString() const {
    return std::get<std::string>(value_);
}

bool Property::asBool() const {
    return std::get<bool>(value_);
}

geom::Color Property::asColor() const {
    if (std::holds_alternative<bcad::geom::Color>(value_)) {
        return std::get<bcad::geom::Color>(value_);
    }
    return geom::Color{};
}

int Property::asEnum() const {
    if (std::holds_alternative<EnumIndex>(value_)) {
        return std::get<EnumIndex>(value_).value;
    }
    return 0;
}

void Property::setFromDouble(double v) {
    assert(type_ == PropertyType::Double);
    if (hasRange_ && (v < min_ || v > max_)) {
        throw std::out_of_range("Value out of range");
    }
    value_ = v;
}

void Property::setFromInt(int v) {
    assert(type_ == PropertyType::Int);
    value_ = v;
}

void Property::setFromString(const std::string& v) {
    assert(type_ == PropertyType::String);
    value_ = v;
}

void Property::setFromBool(bool v) {
    assert(type_ == PropertyType::Bool);
    value_ = v;
}

void Property::setFromColor(const geom::Color& v) {
    assert(type_ == PropertyType::Color);
    value_ = v;
}

void Property::setFromEnum(int v) {
    assert(type_ == PropertyType::Enum);
    if (v < 0 || v >= static_cast<int>(enumValues_.size())) {
        throw std::out_of_range("Enum value out of range");
    }
    value_ = EnumIndex(v);
}

bool Property::validate() const {
    if (type_ == PropertyType::Double && hasRange_) {
        double v = asDouble();
        if (v < min_ || v > max_) return false;
    }
    if (type_ == PropertyType::Enum) {
        int v = asEnum();
        if (v < 0 || v >= static_cast<int>(enumValues_.size())) return false;
    }
    return true;
}

// PropertyMap implementation

PropertyMap::PropertyMap(const PropertyMap& other) {
    for (const auto& [name, prop] : other.properties_) {
        properties_.emplace(name, prop->clone());
    }
}

PropertyMap& PropertyMap::operator=(const PropertyMap& other) {
    if (this == &other) return *this;
    properties_.clear();
    for (const auto& [name, prop] : other.properties_) {
        properties_.emplace(name, prop->clone());
    }
    return *this;
}

Property* PropertyMap::addDouble(const std::string& name, double initial) {
    auto prop = std::make_unique<Property>(name, PropertyType::Double, PropertyValue(initial));
    Property* ptr = prop.get();
    properties_.emplace(name, std::move(prop));
    return ptr;
}

Property* PropertyMap::addInt(const std::string& name, int initial) {
    auto prop = std::make_unique<Property>(name, PropertyType::Int, PropertyValue(initial));
    Property* ptr = prop.get();
    properties_.emplace(name, std::move(prop));
    return ptr;
}

Property* PropertyMap::addString(const std::string& name, const std::string& initial) {
    auto prop = std::make_unique<Property>(name, PropertyType::String, PropertyValue(initial));
    Property* ptr = prop.get();
    properties_.emplace(name, std::move(prop));
    return ptr;
}

Property* PropertyMap::addBool(const std::string& name, bool initial) {
    auto prop = std::make_unique<Property>(name, PropertyType::Bool, PropertyValue(initial));
    Property* ptr = prop.get();
    properties_.emplace(name, std::move(prop));
    return ptr;
}

Property* PropertyMap::addColor(const std::string& name, const geom::Color& initial) {
    auto prop = std::make_unique<Property>(name, PropertyType::Color, PropertyValue(initial));
    Property* ptr = prop.get();
    properties_.emplace(name, std::move(prop));
    return ptr;
}

Property* PropertyMap::addEnum(const std::string& name, int initial, const std::vector<std::string>& values) {
    auto prop = std::make_unique<Property>(name, PropertyType::Enum, PropertyValue(EnumIndex(initial)));
    prop->setEnumValues(values);
    Property* ptr = prop.get();
    properties_.emplace(name, std::move(prop));
    return ptr;
}

Property* PropertyMap::get(const std::string& name) {
    auto it = properties_.find(name);
    return it != properties_.end() ? it->second.get() : nullptr;
}

const Property* PropertyMap::get(const std::string& name) const {
    auto it = properties_.find(name);
    return it != properties_.end() ? it->second.get() : nullptr;
}

const Property* PropertyMap::find(const std::string& name) const {
    auto it = properties_.find(name);
    return it != properties_.end() ? it->second.get() : nullptr;
}

bool PropertyMap::has(const std::string& name) const {
    return properties_.find(name) != properties_.end();
}

void PropertyMap::remove(const std::string& name) {
    if (auto* p = find(name)) {
        PropertyValue oldValue = p->value();
        if (properties_.erase(name)) {
            bcad::events::EventBus::instance().publish(
                bcad::events::PropertyChanged{nullptr, nullptr, name, oldValue, PropertyValue{}}
            );
        }
    }
}

double PropertyMap::getDouble(const std::string& name, double def) const {
    auto* p = find(name);
    return p ? p->asDouble() : def;
}

int PropertyMap::getInt(const std::string& name, int def) const {
    auto* p = find(name);
    return p ? p->asInt() : def;
}

std::string PropertyMap::getString(const std::string& name) const {
    auto* p = find(name);
    return p ? p->asString() : "";
}

bool PropertyMap::getBool(const std::string& name, bool def) const {
    auto* p = find(name);
    return p ? p->asBool() : def;
}

geom::Color PropertyMap::getColor(const std::string& name) const {
    auto* p = find(name);
    return p ? p->asColor() : geom::Color{};
}

int PropertyMap::getEnum(const std::string& name, int def) const {
    auto* p = find(name);
    return p ? p->asEnum() : def;
}

void PropertyMap::setDouble(const std::string& name, double v) {
    if (auto* p = get(name)) {
        if (!p->isReadOnly()) {
            PropertyValue oldValue = p->value();
            p->setFromDouble(v);
            if (oldValue.index() != p->value().index() ||
                (oldValue.index() == 0 && std::get<double>(oldValue) != std::get<double>(p->value()))) {
                bcad::events::EventBus::instance().publish(
                    bcad::events::PropertyChanged{nullptr, nullptr, name, oldValue, p->value()}
                );
            }
        }
    }
}

void PropertyMap::setInt(const std::string& name, int v) {
    if (auto* p = get(name)) {
        if (!p->isReadOnly()) {
            PropertyValue oldValue = p->value();
            p->setFromInt(v);
            if (oldValue.index() != p->value().index() ||
                (oldValue.index() == 1 && std::get<int>(oldValue) != std::get<int>(p->value()))) {
                bcad::events::EventBus::instance().publish(
                    bcad::events::PropertyChanged{nullptr, nullptr, name, oldValue, p->value()}
                );
            }
        }
    }
}

void PropertyMap::setString(const std::string& name, const std::string& v) {
    if (auto* p = get(name)) {
        if (!p->isReadOnly()) {
            PropertyValue oldValue = p->value();
            p->setFromString(v);
            if (oldValue.index() != p->value().index() ||
                (oldValue.index() == 2 && std::get<std::string>(oldValue) != std::get<std::string>(p->value()))) {
                bcad::events::EventBus::instance().publish(
                    bcad::events::PropertyChanged{nullptr, nullptr, name, oldValue, p->value()}
                );
            }
        }
    }
}

void PropertyMap::setBool(const std::string& name, bool v) {
    if (auto* p = get(name)) {
        if (!p->isReadOnly()) {
            PropertyValue oldValue = p->value();
            p->setFromBool(v);
            if (oldValue.index() != p->value().index() ||
                (oldValue.index() == 3 && std::get<bool>(oldValue) != std::get<bool>(p->value()))) {
                bcad::events::EventBus::instance().publish(
                    bcad::events::PropertyChanged{nullptr, nullptr, name, oldValue, p->value()}
                );
            }
        }
    }
}

void PropertyMap::setColor(const std::string& name, const geom::Color& v) {
    if (auto* p = get(name)) {
        if (!p->isReadOnly()) {
            PropertyValue oldValue = p->value();
            p->setFromColor(v);
            if (oldValue.index() != p->value().index() ||
                (oldValue.index() == 4 && std::get<geom::Color>(oldValue) != std::get<geom::Color>(p->value()))) {
                bcad::events::EventBus::instance().publish(
                    bcad::events::PropertyChanged{nullptr, nullptr, name, oldValue, p->value()}
                );
            }
        }
    }
}

void PropertyMap::setEnum(const std::string& name, int v) {
    if (auto* p = get(name)) {
        if (!p->isReadOnly()) {
            PropertyValue oldValue = p->value();
            p->setFromEnum(v);
            if (oldValue.index() != p->value().index() ||
                (oldValue.index() == 5 && std::get<EnumIndex>(oldValue) != std::get<EnumIndex>(p->value()))) {
                bcad::events::EventBus::instance().publish(
                    bcad::events::PropertyChanged{nullptr, nullptr, name, oldValue, p->value()}
                );
            }
        }
    }
}

std::vector<std::string> PropertyMap::listNames() const {
    std::vector<std::string> names;
    names.reserve(properties_.size());
    for (const auto& [name, _] : properties_) {
        names.push_back(name);
    }
    return names;
}

std::string PropertyMap::serialize() const {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [name, prop] : properties_) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << name << "\":";
        switch (prop->type()) {
            case PropertyType::Double:
                oss << prop->asDouble();
                break;
            case PropertyType::Int:
                oss << prop->asInt();
                break;
            case PropertyType::String:
                oss << "\"" << prop->asString() << "\"";
                break;
            case PropertyType::Bool:
                oss << (prop->asBool() ? "true" : "false");
                break;
            case PropertyType::Color: {
                auto c = prop->asColor();
                oss << "{\"r\":" << c.r << ",\"g\":" << c.g << ",\"b\":" << c.b << ",\"a\":" << c.a << "}";
                break;
            }
            case PropertyType::Enum:
                oss << prop->asEnum();
                break;
        }
    }
    oss << "}";
    return oss.str();
}

void PropertyMap::deserialize(const std::string& data) {
    // Simple JSON parsing - in production, use a proper JSON library
    // This is a minimal implementation for basic types
    std::string s = data;
    // Remove braces
    if (s.size() >= 2 && s.front() == '{' && s.back() == '}') {
        s = s.substr(1, s.size() - 2);
    }
    
    // Simple parsing - split by comma at top level
    size_t pos = 0;
    while (pos < s.size()) {
        // Find key
        while (pos < s.size() && std::isspace(s[pos])) ++pos;
        if (pos >= s.size()) break;
        
        if (s[pos] != '"') break;
        ++pos;
        size_t keyStart = pos;
        while (pos < s.size() && s[pos] != '"') ++pos;
        std::string key = s.substr(keyStart, pos - keyStart);
        ++pos; // skip closing quote
        
        // Skip colon
        while (pos < s.size() && std::isspace(s[pos])) ++pos;
        if (pos < s.size() && s[pos] == ':') ++pos;
        while (pos < s.size() && std::isspace(s[pos])) ++pos;
        
        // Parse value based on type (simplified)
        // This is a minimal implementation - real code would use a JSON library
        if (pos < s.size() && s[pos] == '"') {
            // String
            ++pos;
            size_t valStart = pos;
            while (pos < s.size() && s[pos] != '"') ++pos;
            std::string val = s.substr(valStart, pos - valStart);
            if (auto* p = get(key)) p->setFromString(val);
            ++pos;
        } else if (pos < s.size() && (s[pos] == 't' || s[pos] == 'f')) {
            // Bool
            bool val = (s[pos] == 't');
            if (auto* p = get(key)) p->setFromBool(val);
            pos += val ? 4 : 5;
        } else {
            // Number or enum
            size_t valStart = pos;
            while (pos < s.size() && s[pos] != ',' && s[pos] != '}') ++pos;
            std::string valStr = s.substr(valStart, pos - valStart);
            if (auto* p = get(key)) {
                if (p->type() == PropertyType::Double) p->setFromDouble(std::stod(valStr));
                else if (p->type() == PropertyType::Int) p->setFromInt(std::stoi(valStr));
                else if (p->type() == PropertyType::Enum) p->setFromEnum(std::stoi(valStr));
            }
        }
        
        while (pos < s.size() && (std::isspace(s[pos]) || s[pos] == ',')) ++pos;
    }
}

} // namespace bcad::properties