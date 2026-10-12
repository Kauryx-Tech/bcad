#pragma once

#include "bcad/layout/DimensionStyle.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace bcad::layout {

class DimensionStyleRegistry {
public:
    static DimensionStyleRegistry& instance();

    void registerStyle(std::unique_ptr<DimensionStyle> style);
    bool unregisterStyle(std::string_view name);
    void clear();

    const DimensionStyle* find(std::string_view name) const;
    const DimensionStyle& getOrDefault(std::string_view name) const;
    std::vector<std::string> listNames() const;

    DimensionStyleRegistry(const DimensionStyleRegistry&) = delete;
    DimensionStyleRegistry& operator=(const DimensionStyleRegistry&) = delete;

private:
    DimensionStyleRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<DimensionStyle>> styles_;
    mutable std::mutex mutex_;
};

} // namespace bcad::layout