#pragma once

#include <string_view>
#include <functional>
#include <cstring>

namespace bcad::geom {

// Identifiant de type d'entité extensible par plugins.
// Utilise une chaîne stable (ex: "bcad.Point", "bcad.Line") pour l'interopérabilité.
// Stocke un pointeur vers une chaîne statique pour permettre constexpr.
struct TypeId {
    const char* value = nullptr;

    constexpr TypeId() = default;
    constexpr TypeId(const char* str) : value(str) {}
    constexpr TypeId(std::string_view str) : value(str.data()) {}

    constexpr bool operator==(const TypeId& other) const { 
        return std::strcmp(value, other.value) == 0; 
    }
    constexpr bool operator!=(const TypeId& other) const { 
        return std::strcmp(value, other.value) != 0; 
    }
    constexpr bool operator<(const TypeId& other) const { 
        return std::strcmp(value, other.value) < 0; 
    }

    constexpr bool operator==(std::string_view other) const { 
        return std::strcmp(value, other.data()) == 0; 
    }
    constexpr bool operator==(const char* other) const { 
        return std::strcmp(value, other) == 0; 
    }

    constexpr bool operator!=(std::string_view other) const { 
        return std::strcmp(value, other.data()) != 0; 
    }
    constexpr bool operator!=(const char* other) const { 
        return std::strcmp(value, other) != 0; 
    }

    constexpr bool operator<(std::string_view other) const { 
        return std::strcmp(value, other.data()) < 0; 
    }
    constexpr bool operator<(const char* other) const { 
        return std::strcmp(value, other) < 0; 
    }

    constexpr explicit operator bool() const { return value && *value != '\0'; }
    constexpr std::string_view str() const { return value ? std::string_view(value) : std::string_view(); }
    constexpr const char* c_str() const { return value; }
};

inline std::size_t hash_value(const TypeId& id) {
    return std::hash<std::string_view>{}(id.value ? std::string_view(id.value) : std::string_view());
}

} // namespace bcad::geom

namespace std {
template<>
struct hash<bcad::geom::TypeId> {
    std::size_t operator()(const bcad::geom::TypeId& id) const noexcept {
        return std::hash<std::string_view>{}(id.value ? std::string_view(id.value) : std::string_view());
    }
};
}