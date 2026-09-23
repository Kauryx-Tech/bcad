# Architecture géométrique BCAD

> [!IMPORTANT]
>
> ## Statut : MIXTE — géométrie 2D implémentée, restauration 3D cible
>
> La **section 1** (« problèmes historiques ») est un **document d'archive** : l'architecture
> **actuelle** (CGAL masqué derrière les types BCAD `Point2`/`Vector2`/`Transform2D`,
> ADR-002, `Entity::tessellate()` + `detail/CgalConversions.h` confiné) ne correspond **plus**
> au « problème CGAL exposé » évoqué dans `ARCHITECTURE_REVIEW.md`. Les **sections 2 à 8**
> décrivent l'architecture **cible** (`ITessellator`, POD tuilés, majeur CGAL Ready)
> **non implémentée**.

> Architecture des types et opérations géométriques. Décrit l'abstraction du kernel géométrique et la préparation 2D/3D.

## 1. État actuel (problème)

CGAL est exposé publiquement dans `include/bcad/geometry/Types.h` :

```cpp
using Point2 = Kernel::Point_2;  // = CGAL::Point_2
using Vector2 = Kernel::Vector_2;
using AffTransform2 = CGAL::Aff_transformation_2<Kernel>;
using Polygon2 = CGAL::Polygon_2<Kernel>;
```

Tout consommateur de `bcad/geometry/Entity.h` inclut CGAL.

## 2. Architecture cible

### 2.1 Principe : CGAL derrière une façade

```
BCAD public API
        │
        ▼
  bcad::geom::Point2    (value type POD)
  bcad::geom::Vector2
  bcad::geom::Transform2
        │
        ▼
  bcad::geom::detail::CGALBackend   (privé)
        │
        ▼
  CGAL (header-only)
```

**Règle :** aucun header CGAL dans `include/bcad/geometry/`. Tout est dans `geometry/detail/`.

### 2.2 Types BCAD publics (value types)

```cpp
namespace bcad::geom {

struct Point2 { double x, y; };
struct Vector2 { double x, y; };
struct BoundingBox2 { double minX, minY, maxX, maxY; };
struct Color { float r, g, b, a; };
struct Segment2 { Point2 a, b; };
struct Triangle2 { Point2 a, b, c; };

struct Point3 { double x, y, z; };
struct Vector3 { double x, y, z; };
struct BoundingBox3 { double minX, minY, minZ, maxX, maxY, maxZ; };
struct Plane3 { Point3 origin; Vector3 normal; };
struct Ray3 { Point3 origin; Vector3 direction; };

// Opaque value types (détails privés)
using Transform2 = detail::Transform2Impl;
using Transform3 = detail::Transform3Impl;
using Polygon2 = detail::Polygon2Impl;
using Polygon3 = detail::Polygon3Impl;

}
```

**Justification :** pas d'interfaces virtuelles (`IPoint2`). Les value types sont simples, performants, et suffisent. Voir `ARCHITECTURE_PRINCIPLES.md` §2.1 (anti-pattern "abstraction excessive").

### 2.3 Transformations

```cpp
namespace bcad::geom {

class Transform2 {
public:
    static Transform2 identity();
    static Transform2 translation(double dx, double dy);
    static Transform2 rotation(double angleRad, const Point2& center);
    static Transform2 scale(double sx, double sy, const Point2& center);
    static Transform2 mirrorX();
    static Transform2 mirrorY();
    Point2 apply(const Point2& p) const;
    Vector2 apply(const Vector2& v) const;
};

class Transform3 {
public:
    static Transform3 identity();
    static Transform3 translation(double dx, double dy, double dz);
    static Transform3 rotation(double angleRad, const Vector3& axis, const Point3& center);
    static Transform3 scale(double sx, double sy, double sz, const Point3& center);
    Point3 apply(const Point3& p) const;
    Vector3 apply(const Vector3& v) const;
};

}
```

## 3. Opérations géométriques

| Opération | 2D | 3D | Implémentation |
|-----------|-----|-----|----------------|
| Distance point-point | ✓ | ✓ | std::sqrt |
| Distance point-segment | ✓ | ✓ | projection |
| Distance point-polygone | ✓ | ✗ | inside/outside |
| Angle entre vecteurs | ✓ | ✓ | dot product |
| Projection point-droite | ✓ | ✓ | dot/cross |
| Union/Intersection/Diff polygones | ✓ | ✗ | CGAL Boolean_set_operations_2 |
| Triangulation Delaunay | ✓ | ✗ | CGAL Delaunay_triangulation_2 |
| Triangulation contrainte | ✓ | ✗ | CGAL CDT_2 |
| Tessellation adaptative | ✓ | ✓ | detail:: |

## 4. Précision numérique

**Kernel :** `CGAL::Exact_predicates_inexact_constructions_kernel`
- Prédicats exacts (intersection, orientation) : robustes
- Constructions inexactes (double) : rapides

```cpp
// include/bcad/geometry/Tolerance.h
namespace bcad::geom {
struct Tolerance {
    static constexpr double kLinear = 1e-9;            // longueurs / coordonnées
    static constexpr double kDegenerateLength = 1e-12; // garde-fou avant division
    static constexpr double kAngular = 1e-9;           // angles (radians)
};
bool nearlyZero(double v, double tol = Tolerance::kLinear);
bool nearlyEqual(double a, double b, double tol = Tolerance::kLinear);
}
```

## 5. Préparation 2D/3D

**Choix :** types dimensionnés explicites (pas de templates) :
- `Point2` / `Point3` (pas `template<int D> Point`)
- `Vector2` / `Vector3`
- `Transform2` / `Transform3`
- `BoundingBox2` / `BoundingBox3`

Plus simple, plus lisible, pas d'overhead template.

## 6. Découplage avec le renderer

Le renderer ne connaît pas CGAL :

> Cible : l'`Entity` référencée ci-dessous vit dans `bcad::document`
> (aujourd'hui `bcad::geom::Entity`) — voir `ENTITY_MODEL.md` §2.1.

```cpp
namespace bcad::render {

struct TessellationInput {
    std::vector<float> vertices;    // x, y, z, [r, g, b, a]
    std::vector<uint32_t> indices;
};

class ITessellator {
public:
    virtual ~ITessellator() = default;
    virtual TessellationInput tessellate(const document::Entity& entity, double maxDeviation) = 0;
};

}
```

L'implémentation par défaut `CGALTessellator` utilise CGAL en interne.

## 7. Graphe cible

```
include/bcad/geometry/
├── Point2.h
├── Vector2.h
├── Point3.h
├── Vector3.h
├── BoundingBox.h
├── Transform2.h
├── Transform3.h
├── Polygon2.h
├── Polygon3.h
├── Color.h
├── Segment.h
├── Triangle.h
├── Plane.h
├── Ray.h
├── GeometryUtils.h
├── BooleanOps.h
├── Triangulation.h
└── Tessellation.h

src/bcad/geometry/detail/
├── CGALPoint2.h        (conversions)
├── CGALPolygon2.h
├── CGALBooleanOps.cpp
├── CGALTriangulation.cpp
└── CGALTessellation.cpp
```

**Règle :** aucun header CGAL dans `include/bcad/geometry/`.

## 8. FAQ

**Q : Pourquoi pas d'interfaces virtuelles pour Point2 ?**
R : Value types simples et performants. Pas d'overhead. Voir §2.2.

**Q : Comment changer de kernel ?**
R : Modifier `src/bcad/geometry/detail/`. L'API publique reste inchangée.

**Q : Comment ajouter la 3D ?**
R : Utiliser les types `Point3`, `Vector3`, `Transform3`. Le kernel 3D peut être CGAL, Eigen, ou un autre.