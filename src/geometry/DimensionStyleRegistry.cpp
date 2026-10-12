#include "bcad/layout/DimensionStyleRegistry.h"
#include <memory>

namespace bcad::layout {

DimensionStyleRegistry& DimensionStyleRegistry::instance() {
    static DimensionStyleRegistry registry;
    return registry;
}

void DimensionStyleRegistry::registerStyle(std::unique_ptr<DimensionStyle> style) {
    std::lock_guard lock(mutex_);
    if (style && !style->name.empty()) {
        styles_[style->name] = std::move(style);
    }
}

bool DimensionStyleRegistry::unregisterStyle(std::string_view name) {
    std::lock_guard lock(mutex_);
    return styles_.erase(std::string(name)) > 0;
}

void DimensionStyleRegistry::clear() {
    std::lock_guard lock(mutex_);
    styles_.clear();
}

const DimensionStyle* DimensionStyleRegistry::find(std::string_view name) const {
    std::lock_guard lock(mutex_);
    auto it = styles_.find(std::string(name));
    return it != styles_.end() ? it->second.get() : nullptr;
}

const DimensionStyle& DimensionStyleRegistry::getOrDefault(std::string_view name) const {
    if (auto* style = find(name)) return *style;
    static const DimensionStyle standard = DimensionStyle::standard();
    return standard;
}

std::vector<std::string> DimensionStyleRegistry::listNames() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(styles_.size());
    for (const auto& [name, _] : styles_) names.push_back(name);
    return names;
}

} // namespace bcad::layout