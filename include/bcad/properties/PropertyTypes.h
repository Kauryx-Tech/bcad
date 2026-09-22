#pragma once

#include "bcad/geometry/Point.h"
#include <variant>

namespace bcad::properties {

// Strong type for enum index to avoid ambiguity with Int in variant
struct EnumIndex {
    int value = 0;
    constexpr EnumIndex() = default;
    constexpr EnumIndex(int v) : value(v) {}
    constexpr operator int() const { return value; }
};

// Property value type - uses variant for type-safe storage
// EnumIndex is used instead of int for Enum to avoid ambiguity with Int
using PropertyValue = std::variant<double, int, std::string, bool, bcad::geom::Color, EnumIndex>;

} // namespace bcad::properties