#pragma once

#include <cmath>
#include <ostream>

namespace bcad::geom {

struct Point2 {
    double x_ = 0.0;
    double y_ = 0.0;

    constexpr Point2() = default;
    constexpr Point2(double x, double y) : x_(x), y_(y) {}

    constexpr double x() const { return x_; }
    constexpr double y() const { return y_; }
    constexpr void setX(double v) { x_ = v; }
    constexpr void setY(double v) { y_ = v; }

    constexpr Point2& operator+=(const Point2& other) {
        x_ += other.x_;
        y_ += other.y_;
        return *this;
    }
    constexpr Point2& operator-=(const Point2& other) {
        x_ -= other.x_;
        y_ -= other.y_;
        return *this;
    }
    constexpr Point2& operator*=(double scalar) {
        x_ *= scalar;
        y_ *= scalar;
        return *this;
    }
    constexpr Point2& operator/=(double scalar) {
        x_ /= scalar;
        y_ /= scalar;
        return *this;
    }
};

struct Vector2 {
    double x_ = 0.0;
    double y_ = 0.0;

    constexpr Vector2() = default;
    constexpr Vector2(double x, double y) : x_(x), y_(y) {}
    constexpr explicit Vector2(const Point2& p) : x_(p.x_), y_(p.y_) {}

    constexpr double x() const { return x_; }
    constexpr double y() const { return y_; }
    constexpr void setX(double v) { x_ = v; }
    constexpr void setY(double v) { y_ = v; }

    constexpr Vector2& operator+=(const Vector2& other) {
        x_ += other.x_;
        y_ += other.y_;
        return *this;
    }
    constexpr Vector2& operator-=(const Vector2& other) {
        x_ -= other.x_;
        y_ -= other.y_;
        return *this;
    }
    constexpr Vector2& operator*=(double scalar) {
        x_ *= scalar;
        y_ *= scalar;
        return *this;
    }
    constexpr Vector2& operator/=(double scalar) {
        x_ /= scalar;
        y_ /= scalar;
        return *this;
    }
};

inline constexpr Point2 operator+(Point2 lhs, const Point2& rhs) { lhs += rhs; return lhs; }
// Point2 - Point2 returns Vector2 (not Point2)
inline constexpr Vector2 operator-(const Point2& a, const Point2& b) {
    return Vector2(a.x_ - b.x_, a.y_ - b.y_);
}
inline constexpr Point2 operator*(Point2 lhs, double s) { lhs *= s; return lhs; }
inline constexpr Point2 operator*(double s, Point2 rhs) { rhs *= s; return rhs; }
inline constexpr Point2 operator/(Point2 lhs, double s) { lhs /= s; return lhs; }

inline constexpr Vector2 operator+(Vector2 lhs, const Vector2& rhs) { lhs += rhs; return lhs; }
inline constexpr Vector2 operator-(Vector2 lhs, const Vector2& rhs) { lhs -= rhs; return lhs; }
inline constexpr Vector2 operator*(Vector2 lhs, double s) { lhs *= s; return lhs; }
inline constexpr Vector2 operator*(double s, Vector2 rhs) { rhs *= s; return rhs; }
inline constexpr Vector2 operator/(Vector2 lhs, double s) { lhs /= s; return lhs; }

inline constexpr bool operator==(const Point2& a, const Point2& b) {
    return a.x_ == b.x_ && a.y_ == b.y_;
}
inline constexpr bool operator!=(const Point2& a, const Point2& b) { return !(a == b); }

inline constexpr bool operator==(const Vector2& a, const Vector2& b) {
    return a.x_ == b.x_ && a.y_ == b.y_;
}
inline constexpr bool operator!=(const Vector2& a, const Vector2& b) { return !(a == b); }

inline double distance(const Point2& a, const Point2& b) {
    double dx = a.x_ - b.x_;
    double dy = a.y_ - b.y_;
    return std::sqrt(dx * dx + dy * dy);
}

inline double squaredDistance(const Point2& a, const Point2& b) {
    double dx = a.x_ - b.x_;
    double dy = a.y_ - b.y_;
    return dx * dx + dy * dy;
}

inline constexpr double dot(const Vector2& a, const Vector2& b) {
    return a.x_ * b.x_ + a.y_ * b.y_;
}
inline constexpr double dot(const Point2& a, const Vector2& b) {
    return a.x_ * b.x_ + a.y_ * b.y_;
}
inline constexpr double cross(const Vector2& a, const Vector2& b) {
    return a.x_ * b.y_ - a.y_ * b.x_;
}
inline double length(const Vector2& v) { return std::sqrt(dot(v, v)); }
inline double squaredLength(const Vector2& v) { return dot(v, v); }
inline Vector2 normalize(const Vector2& v) {
    double len = length(v);
    return len > 0 ? v / len : Vector2{};
}

inline std::ostream& operator<<(std::ostream& os, const Point2& p) {
    return os << '(' << p.x_ << ", " << p.y_ << ')';
}

inline std::ostream& operator<<(std::ostream& os, const Vector2& v) {
    return os << '<' << v.x_ << ", " << v.y_ << '>';
}

struct Color {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

    static Color fromRgb255(int r255, int g255, int b255, float a = 1.0f) {
        return Color{ r255 / 255.0f, g255 / 255.0f, b255 / 255.0f, a };
    }
};

} // namespace bcad::geom