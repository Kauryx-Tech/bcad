#pragma once

#include <string>
#include <string_view>
#include <functional>

namespace bcad::geom {

// Identifiant de type d'entité extensible par plugins.
// Utilise une chaîne stable (ex: "bcad.Point", "bcad.Line") pour l'interopérabilité.
struct TypeId {
    std::string value;

    constexpr TypeId() = default;
    constexpr TypeId(const char* str) : value(str) {}
    constexpr TypeId(std::string_view str) : value(str) {}
    constexpr TypeId(std::string str) : value(std::move(str)) {}

    constexpr bool operator==(const TypeId& other) const { return value == other.value; }
    constexpr bool operator!=(const TypeId& other) const { return value != other.value; }
    constexpr bool operator<(const TypeId& other) const { return value < other.value; }

    constexpr explicit operator bool() const { return !value.empty(); }
    constexpr std::string_view str() const { return value; }
    constexpr const char* c_str() const { return value.c_str(); }
};

inline std::size_t hash_value(const TypeId& id) {
    return std::hash<std::string_view>{}(id.value);
}

} // namespace bcad::geom

namespace std {
template<>
struct hash<bcad::geom::TypeId> {
    std::size_t operator()(const bcad::geom::TypeId& id) const noexcept {
        return std::hash<std::string_view>{}(id.value);
    }
};
}