# Systèmes de coordonnées BCAD

> Préparation de la transition 2D → 3D. Documenter les systèmes et les transformations.

## 1. Systèmes

### 1.1 World Coordinate System (WCS)

Le repère global du document. Par convention en BCAD :

- 2D : WCS = (X, Y), Z = 0
- 3D : WCS = (X, Y, Z), main droite

### 1.2 Local Coordinate System (LCS)

Repère attaché à une entité ou un groupe. Permet de positionner un objet relativement à un autre.

```cpp
struct CoordinateSystem {
    geom::Point3 origin;
    geom::Vector3 xAxis;
    geom::Vector3 yAxis;
    geom::Vector3 zAxis;  // 3D
    // xAxis, yAxis, zAxis sont orthonormés
};
```

### 1.3 Object Coordinate System (OCS)

Repère intrinsèque à l'objet. Pour une polyligne, c'est le repère du premier sommet.

### 1.4 User Coordinate System (UCS)

Repère défini par l'utilisateur dans l'UI. Permet de dessiner dans un plan arbitraire.

```cpp
class UCSRegistry {
public:
    static UCSRegistry& global();
    void setActive(const CoordinateSystem& ucs);
    const CoordinateSystem& active() const;
    CoordinateSystem worldToUser(const geom::Point3& p) const;
    geom::Point3 userToWorld(const geom::Point3& p) const;
};
```

## 2. Types géométriques 2D/3D

### 2.1 Types 2D

```cpp
namespace bcad::geom {
struct Point2 { double x, y; };
struct Vector2 { double x, y; };
struct BoundingBox2 { double minX, minY, maxX, maxY; };
struct Segment2 { Point2 a, b; };
struct Triangle2 { Point2 a, b, c; };
class Transform2 { /* ... */ };
}
```

### 2.2 Types 3D

```cpp
namespace bcad::geom {
struct Point3 { double x, y, z; };
struct Vector3 { double x, y, z; };
struct BoundingBox3 { double minX, minY, minZ, maxX, maxY, maxZ; };
struct Segment3 { Point3 a, b; };
struct Triangle3 { Point3 a, b, c; };
struct Plane3 { Point3 origin; Vector3 normal; };
struct Ray3 { Point3 origin; Vector3 direction; };
class Transform3 { /* ... */ };
}
```

### 2.3 Stratégie

**Types dimensionnés explicites** (pas de template). Voir `GEOMETRY_ARCHITECTURE.md` §5.1.

## 3. Conversions 2D ↔ 3D

Une entité 2D a une représentation 3D implicite avec Z = 0 :

```cpp
Point3 to3D(const Point2& p, double z = 0.0) {
    return {p.x, p.y, z};
}

Point2 to2D(const Point3& p) {
    return {p.x, p.y};
}
```

**Justification :** le Document, l'index spatial, et la persistence utilisent des types 3D. Les entités 2D sont des entités 3D contraintes à Z = 0.

## 4. Camera abstraite

Voir `RENDERING_ARCHITECTURE.md` §2. La caméra est abstraite :

- `Camera2D` : projection orthographique 2D
- `Camera3D` : projection orthographique ou perspective 3D

```cpp
class ICamera {
    // ...
};

class Camera2D : public ICamera { /* orthographic 2D */ };
class Camera3D : public ICamera { /* orthographic or perspective 3D */ };
```

## 5. Transformations globales

Les transformations (translation, rotation, scale) sont de type `Transform3` (3D unifié) :

```cpp
class Transform3 {
public:
    static Transform3 identity();
    static Transform3 translation(double dx, double dy, double dz = 0);
    static Transform3 rotation(double angleRad, const Vector3& axis, const Point3& center = {});
    static Transform3 scale(double sx, double sy, double sz = 1.0, const Point3& center = {});
    static Transform3 mirrorXY();
    // ...

    Point3 apply(const Point3& p) const;
    Vector3 apply(const Vector3& v) const;
};
```

**Justification :** un seul type de transformation évite les conversions 2D↔3D dans le code.

## 6. Règles

1. Le Document utilise des types 3D en interne (Z=0 pour 2D)
2. Le renderer supporte Camera2D et Camera3D
3. Les transformations sont unifiées en `Transform3`
4. Les conversions 2D/3D sont explicites, pas automatiques
5. Les entités 2D et 3D cohabitent dans le même Document
6. L'index spatial est 3D, gère les deux