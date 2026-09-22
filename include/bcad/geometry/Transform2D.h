#pragma once

#include "bcad/geometry/Point.h"
#include <cmath>

namespace bcad::geom {

struct BoundingBox;

class Transform2D {
public:
    struct Matrix {
        double m00 = 1.0, m01 = 0.0, m02 = 0.0;
        double m10 = 0.0, m11 = 1.0, m12 = 0.0;

        constexpr Matrix() = default;
        constexpr Matrix(double a, double b, double c, double d, double e, double f)
            : m00(a), m01(b), m02(c), m10(d), m11(e), m12(f) {}
    };

    constexpr Transform2D() = default;
    explicit constexpr Transform2D(const Matrix& m) : m_(m) {}

    constexpr const Matrix& matrix() const { return m_; }

    Point2 transform(const Point2& p) const {
        return Point2(
            m_.m00 * p.x_ + m_.m01 * p.y_ + m_.m02,
            m_.m10 * p.x_ + m_.m11 * p.y_ + m_.m12
        );
    }

    Vector2 transform(const Vector2& v) const {
        return Vector2(
            m_.m00 * v.x_ + m_.m01 * v.y_,
            m_.m10 * v.x_ + m_.m11 * v.y_
        );
    }

    BoundingBox transform(const BoundingBox& bbox) const;

    Transform2D operator*(const Transform2D& other) const {
        return Transform2D(Matrix{
            m_.m00 * other.m_.m00 + m_.m01 * other.m_.m10,
            m_.m00 * other.m_.m01 + m_.m01 * other.m_.m11,
            m_.m00 * other.m_.m02 + m_.m01 * other.m_.m12 + m_.m02,
            m_.m10 * other.m_.m00 + m_.m11 * other.m_.m10,
            m_.m10 * other.m_.m01 + m_.m11 * other.m_.m11,
            m_.m10 * other.m_.m02 + m_.m11 * other.m_.m12 + m_.m12
        });
    }

    Transform2D& operator*=(const Transform2D& other) { return *this = *this * other; }

    static Transform2D identity() { return Transform2D{}; }

    static Transform2D translation(double dx, double dy) {
        return Transform2D(Matrix{1, 0, dx, 0, 1, dy});
    }

    static Transform2D rotation(double radians, const Point2& pivot = Point2{0, 0}) {
        double c = std::cos(radians);
        double s = std::sin(radians);
        Transform2D toOrigin = translation(-pivot.x_, -pivot.y_);
        Transform2D rot(Matrix{c, -s, 0, s, c, 0});
        Transform2D back = translation(pivot.x_, pivot.y_);
        return back * rot * toOrigin;
    }

    static Transform2D scaling(double factor, const Point2& pivot = Point2{0, 0}) {
        return scaling(factor, factor, pivot);
    }

    static Transform2D scaling(double sx, double sy, const Point2& pivot = Point2{0, 0}) {
        Transform2D toOrigin = translation(-pivot.x_, -pivot.y_);
        Transform2D scale(Matrix{sx, 0, 0, 0, sy, 0});
        Transform2D back = translation(pivot.x_, pivot.y_);
        return back * scale * toOrigin;
    }

    static Transform2D mirrorX() { return Transform2D(Matrix{1, 0, 0, 0, -1, 0}); }
    static Transform2D mirrorY() { return Transform2D(Matrix{-1, 0, 0, 0, 1, 0}); }

    static Transform2D mirrorAcrossLine(const Point2& p1, const Point2& p2) {
        double angle = std::atan2(p2.y_ - p1.y_, p2.x_ - p1.x_);
        double c2 = std::cos(2.0 * angle);
        double s2 = std::sin(2.0 * angle);
        Transform2D reflect(Matrix{c2, s2, 0, s2, -c2, 0});
        Transform2D toOrigin = translation(-p1.x_, -p1.y_);
        Transform2D back = translation(p1.x_, p1.y_);
        return back * reflect * toOrigin;
    }

    Transform2D inverse() const {
        double det = m_.m00 * m_.m11 - m_.m01 * m_.m10;
        if (std::abs(det) < 1e-15) return identity();
        double invDet = 1.0 / det;
        return Transform2D(Matrix{
            m_.m11 * invDet, -m_.m01 * invDet,
            (m_.m01 * m_.m12 - m_.m11 * m_.m02) * invDet,
            -m_.m10 * invDet, m_.m00 * invDet,
            (m_.m10 * m_.m02 - m_.m00 * m_.m12) * invDet
        });
    }

private:
    Matrix m_;
};

} // namespace bcad::geom