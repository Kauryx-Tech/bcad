# Index spatial BCAD

> [!IMPORTANT]
>
> ## Statut : MIXTE — état actuel + architecture cible
>
> La section **1** décrit l'état **réel** (le Core dépend de `render::Quadtree`, `Document.h:67`) et la
> **violation ADR-001/008** associée. Les sections **2 à 8** décrivent l'**architecture cible**
> (`bcad::index`, `ISpatialIndex`) **non implémentée**.

> Abstraction indépendante du renderer. Le Document n'inclut aucun backend d'index.

## 1. Problème actuel

`core::Document` possède `std::unique_ptr<render::Quadtree>`. C'est une **dépendance inversée** : le Core dépend du renderer.

## 2. Architecture cible

```cpp
namespace bcad::index {

class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;

    // Insertion
    virtual void insert(const document::Entity* entity) = 0;

    // Suppression
    virtual void remove(document::EntityId id) = 0;

    // Mise à jour (suite à une transformation)
    virtual void update(const document::Entity* entity) = 0;

    // Requête par région
    virtual std::vector<document::Entity*> query(const geom::BoundingBox3& region) const = 0;

    // Picking
    virtual document::Entity* pick(const geom::Point3& p, double tolerance) const = 0;

    // Reset
    virtual void clear() = 0;

    // Stats (debug)
    virtual size_t size() const = 0;
    virtual std::string backendName() const = 0;
};

}
```

## 3. Backends

### 3.1 Quadtree 2D (défaut)

```cpp
class QuadtreeIndex : public ISpatialIndex {
public:
    explicit QuadtreeIndex(const geom::BoundingBox2& bounds, int maxDepth = 8);

    void insert(const document::Entity* entity) override;
    void remove(document::EntityId id) override;
    void update(const document::Entity* entity) override;
    std::vector<document::Entity*> query(const geom::BoundingBox3& region) const override;
    document::Entity* pick(const geom::Point3& p, double tolerance) const override;

private:
    // utilise les bounds 2D (z=0) pour les entités 2D
    // ignore le z pour les requêtes
};

std::unique_ptr<ISpatialIndex> createQuadtreeIndex2D(const geom::BoundingBox2& bounds);
```

### 3.2 Octree 3D (futur)

```cpp
class OctreeIndex : public ISpatialIndex {
public:
    explicit OctreeIndex(const geom::BoundingBox3& bounds, int maxDepth = 8);
    // ... 3D natif
};

std::unique_ptr<ISpatialIndex> createOctreeIndex3D(const geom::BoundingBox3& bounds);
```

### 3.3 R-tree (futur)

```cpp
class RTreeIndex : public ISpatialIndex {
    // Variante R-tree (peut être boost::geometry::index::rtree)
};
```

### 3.4 BVH (futur)

```cpp
class BVHIndex : public ISpatialIndex {
    // Bounding Volume Hierarchy, optimal pour le ray-tracing
};
```

## 4. Utilisation dans le Document

```cpp
// core/document/Document.h
#include "bcad/index/ISpatialIndex.h"  // abstraction pure

class Document {
public:
    void setSpatialIndex(std::unique_ptr<index::ISpatialIndex> index) {
        index_ = std::move(index);
    }

    index::ISpatialIndex& spatialIndex() { return *index_; }

private:
    std::unique_ptr<index::ISpatialIndex> index_;  // pas un Quadtree concret
};
```

```cpp
// app/MainWindow.cpp (configuration par défaut)
doc.setSpatialIndex(index::createQuadtreeIndex2D({-100, -100, 100, 100}));
```

## 5. Culling

Le SceneExtractor du renderer utilise l'index spatial :

```cpp
// render/SceneExtractor.cpp
std::vector<TessellationInput> SceneExtractor::extract(
    const document::Document& doc, const ICamera& camera, double maxDeviation) const
{
    auto region = camera.visibleRegion();
    auto visible = doc.spatialIndex().query(region);
    // ... tessellate chaque entité visible
}
```

## 6. Implémentation Quadtree

L'implémentation actuelle (dans `render/Quadtree.cpp`) est déplacée vers `index/QuadtreeIndex.cpp` et ne dépend plus de `core/`.

```cpp
class QuadtreeIndex : public ISpatialIndex {
    // ... implementation existante
};
```

## 7. Évolutions

| Backend | 2D | 3D | Performance insert | Performance query |
|---------|-----|-----|-------------------|-------------------|
| Quadtree | ✓ | (z=0) | O(log n) | O(log n + k) |
| Octree | N/A | ✓ | O(log n) | O(log n + k) |
| R-tree | ✓ | ✓ | O(log n) | O(log n + k) |
| BVH | ✓ | ✓ | O(n log n) build | O(log n) |

## 8. Règles

1. Le Document ne dépend que de `ISpatialIndex`, pas d'un backend concret
2. Le backend par défaut est `QuadtreeIndex` (2D)
3. Pour la 3D, basculer vers `OctreeIndex` ou `RTreeIndex`
4. Le `SceneExtractor` du renderer interroge `ISpatialIndex`
5. Le picking CPU utilise `ISpatialIndex::pick()`
6. Le picking GPU peut être implémenté en parallèle