# CGAL Migration Guide

## Overview

This document describes the strategy for migrating CGAL (Computational Geometry Algorithms Library) usage from public headers to internal implementation details only.

## Migration Status

**Current State**: ✅ Complete - CGAL is fully confined to internal implementation.

- Public headers (`include/bcad/geometry/*.h`) contain **zero** CGAL types or includes
- All CGAL usage is in `src/geometry/*.cpp` and `include/bcad/geometry/detail/CgalConversions.h`
- Public API uses pure BCAD types: `Point2`, `Vector2`, `Transform2D`, `BoundingBox`

## Migration Strategy

### 1. Public Value Types (Complete)

Created pure C++ value types to replace CGAL kernel types:

| CGAL Type | BCAD Replacement | Header |
|-----------|------------------|--------|
| `Kernel::Point_2` | `bcad::geom::Point2` | `Point.h` |
| `Kernel::Vector_2` | `bcad::geom::Vector2` | `Point.h` |
| `Aff_transformation_2` | `bcad::geom::Transform2D` | `Transform2D.h` |
| `Polygon_2` | `std::vector<Point2>` | `Types.h` |
| `Polygon_with_holes_2` | `PolygonWithHoles2` struct | `Types.h` |

### 2. Internal Conversion Layer (Complete)

File: `include/bcad/geometry/detail/CgalConversions.h`

Provides bidirectional conversion between BCAD and CGAL types:

```cpp
// Point conversions
CgalPoint2 toCgal(const Point2& p);
Point2 fromCgal(const CgalPoint2& p);

// Vector conversions
CgalVector2 toCgal(const Vector2& v);
Vector2 fromCgal(const CgalVector2& v);

// Transform conversions
CgalAffTransform2 toCgal(const Transform2D& t);
Transform2D fromCgal(const CgalAffTransform2& t);

// Polygon conversions
CgalPolygon2 toCgalPolygon(const PolylineEntity& poly);
PolylineEntity fromCgalPolygon(const CgalPolygon2& poly);
```

### 3. Implementation Migration (Complete)

All geometry algorithms moved to internal implementation:

| Algorithm | Implementation File | Uses CGAL Internally |
|-----------|---------------------|---------------------|
| Boolean Operations | `BooleanOps.cpp` | ✅ |
| Delaunay Triangulation | `Triangulation.cpp` | ✅ |
| Constrained Triangulation | `Triangulation.cpp` | ✅ |
| Snap/Intersection | `SnapGeometry.cpp` | ✅ |

## Legacy Compatibility Header

For modules that haven't been migrated yet, a temporary compatibility header is provided:

```cpp
// include/bcad/geometry/legacy_cgal.h
// #warning "This header is deprecated. Migrate to BCAD public types."
// Provides CGAL type aliases for backward compatibility during migration.
```

See `include/bcad/geometry/legacy_cgal.h` for details.

## Migration Checklist for New Code

- [ ] Use `bcad::geom::Point2` / `Vector2` instead of `CGAL::Point_2` / `CGAL::Vector_2`
- [ ] Use `bcad::geom::Transform2D` instead of `CGAL::Aff_transformation_2`
- [ ] Use `std::vector<Point2>` instead of `CGAL::Polygon_2`
- [ ] Include `bcad/geometry/Point.h` / `Transform2D.h` instead of CGAL headers
- [ ] If CGAL is absolutely needed, confine to `.cpp` file and use `detail::toCgal()` / `detail::fromCgal()`

## Verification

Run architecture check:
```bash
./scripts/check_arch.sh
```

This verifies:
- No CGAL types in public headers (`include/bcad/geometry/`)
- No CGAL includes in public headers (except `detail/`)
- All public geometry uses BCAD types only