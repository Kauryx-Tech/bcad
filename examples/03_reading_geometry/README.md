# Exemple 3 : Lire la géométrie

> Comment utiliser les types géométriques de BCAD.

## Les types géométriques

```cpp
// include/bcad/geometry/Entity.h
namespace bcad::geom {

enum class EntityType { Point, Line, Circle, Arc, Polyline };

class Entity {
    virtual EntityType type() const = 0;
    virtual BoundingBox boundingBox() const = 0;
    virtual void applyTransform(const AffTransform2& t) = 0;
    virtual std::unique_ptr<Entity> clone() const = 0;
    virtual std::vector<Point2> tessellate(double maxDeviation) const = 0;
    virtual double distanceTo(const Point2& p) const = 0;
};

}
```

## Exemple : calculer la longueur totale

```cpp
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"
#include <cmath>

double totalLength(const bcad::core::Document& doc) {
    double total = 0.0;
    
    for (const auto& entity : doc.entities()) {
        if (entity->type() == bcad::geom::EntityType::Line) {
            const auto* line = static_cast<const bcad::geom::LineEntity*>(entity.get());
            total += line->length();
        }
        // Pour un cercle, on pourrait ajouter la circonférence
    }
    
    return total;
}

int main() {
    bcad::core::Document doc;
    
    auto line = std::make_unique<bcad::geom::LineEntity>(
        bcad::geom::Point2(0, 0),
        bcad::geom::Point2(100, 0)
    );
    doc.addEntity(std::move(line));
    
    printf("Longueur totale: %.2f\n", totalLength(doc));
    // Output: Longueur totale: 100.00
    
    return 0;
}
```

## Exemple : tessellation

```cpp
#include "bcad/geometry/Circle.h"

int main() {
    bcad::geom::CircleEntity circle(
        bcad::geom::Point2(0, 0), 10.0
    );
    
    // Tessellation avec une déviation maximale de 0.1
    auto points = circle.tessellate(0.1);
    
    printf("Nombre de points: %zu\n", points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        printf("  [%zu] (%.2f, %.2f)\n", i,
               bcad::CGAL::to_double(points[i].x()),
               bcad::CGAL::to_double(points[i].y()));
    }
    
    return 0;
}
```

## Exemple : intersection point-segment

```cpp
#include "bcad/geometry/Line.h"
#include "bcad/geometry/GeometryUtils.h"

int main() {
    bcad::geom::LineEntity line(
        bcad::geom::Point2(0, 0),
        bcad::geom::Point2(10, 0)
    );
    
    // Distance entre un point et la ligne
    bcad::geom::Point2 p(5, 3);
    double dist = line.distanceTo(p);
    printf("Distance du point à la ligne: %.2f\n", dist);
    // Output: 3.00
    
    return 0;
}
```

## Exemple : transformation

```cpp
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Transform2D.h"

int main() {
    using namespace bcad::geom;
    
    LineEntity line(Point2(0, 0), Point2(10, 0));
    
    // Translation
    AffTransform2 t = AffTransform2::translation(5, 5);
    line.applyTransform(t);
    
    auto bb = line.boundingBox();
    printf("Nouvelle position: (%.2f, %.2f) - (%.2f, %.2f)\n",
           bb.minX, bb.minY, bb.maxX, bb.maxY);
    // Output: (5.00, 5.00) - (15.00, 5.00)
    
    return 0;
}
```

## Note importante : CGAL exposé

> Le code actuel utilise `CGAL::to_double()` pour convertir les `Point2` (qui sont des types CGAL). C'est l'une des violations architecturales à corriger dans la Phase 2 de la roadmap.

Voir `docs/ARCHITECTURE_PRINCIPLES.md` §1.6 et `docs/GEOMETRY_ARCHITECTURE.md` §2.1.

## Voir aussi

- `include/bcad/geometry/Entity.h`
- `include/bcad/geometry/Line.h`
- `include/bcad/geometry/Circle.h`
- `docs/GEOMETRY_ARCHITECTURE.md`