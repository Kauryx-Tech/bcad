# Architecture de rendu BCAD

> Abstraction du rendu. Le Core ne dépend pas du backend de rendu.

## 1. Séparation

```
Document
   ↓
Scene extraction (Document → RenderScene)
   ↓
Render representation (RenderScene, Renderable objects)
   ↓
Rendering API (IRenderBackend)
   ↓
Backend (OpenGL, Vulkan, WebGPU, ...)
```

## 2. Architecture cible

```cpp
namespace bcad::render {

// Camera abstraite
class ICamera {
public:
    virtual ~ICamera() = default;
    virtual geom::Point3 worldToScreen(const geom::Point3& p) const = 0;
    virtual geom::Point3 screenToWorld(const geom::Point2& s, double z = 0) const = 0;
    virtual geom::BoundingBox3 visibleRegion() const = 0;
    virtual void pan(const geom::Vector2& delta) = 0;
    virtual void zoom(double factor, const geom::Point2& center) = 0;
    virtual void zoomToFit(const geom::BoundingBox3& region) = 0;
};

class Camera2D : public ICamera { /* orthographic 2D */ };
class Camera3D : public ICamera { /* perspective or orthographic 3D */ };

// Tessellator
struct TessellationInput {
    std::vector<float> vertices;     // x, y, z, [r, g, b, a]
    std::vector<uint32_t> indices;
};

class ITessellator {
public:
    virtual ~ITessellator() = default;
    virtual TessellationInput tessellate(
        const document::Entity& entity,
        double maxDeviation) const = 0;
};

// Backend de rendu
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;
    virtual bool initialize(void* windowHandle) = 0;
    virtual void shutdown() = 0;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    virtual void setCamera(const ICamera& camera) = 0;

    virtual void uploadScene(const std::vector<TessellationInput>& scene) = 0;
    virtual void draw() = 0;

    virtual void setClearColor(const geom::Color& c) = 0;
    virtual void resize(int width, int height) = 0;
};

}
```

## 3. Backends

### OpenGL 3.3 (par défaut)

```cpp
class OpenGLBackend : public IRenderBackend {
    // ... utilise GLSL, VBO/VAO, glDrawElements
};
```

### Vulkan (futur)

```cpp
class VulkanBackend : public IRenderBackend {
    // ... utilise SPIR-V, command buffers, descriptor sets
};
```

## 4. Scene extraction

```cpp
namespace bcad::render {

class SceneExtractor {
public:
    std::vector<TessellationInput> extract(
        const document::Document& doc,
        const ICamera& camera,
        double maxDeviation) const;
};

}
```

**Étapes :**
1. Interroger l'index spatial pour la région visible
2. Pour chaque entité visible, appeler `tessellate(maxDeviation)`
3. Regrouper par couleur (ColorBatching) pour réduire les draw calls
4. Retourner la liste de `TessellationInput`

## 5. Tessellation

Voir `GEOMETRY_ARCHITECTURE.md` §6 pour le détail de l'interface `ITessellator`. L'implémentation par défaut utilise CGAL.

```cpp
class CGALTessellator : public ITessellator {
    TessellationInput tessellate(const document::Entity& entity, double maxDeviation) const override {
        // utilise geometry::detail::CGALBackend
    }
};
```

## 6. LOD

Le niveau de détail est calculé à partir du zoom de la caméra :

```cpp
double LevelOfDetail::toleranceForZoom(double zoom) {
    // zoom = pixels par unité monde
    // tolérance = déviation cible en pixels / zoom
    return targetPixelDeviation / zoom;
}
```

Plus la caméra est dézoomée, plus la tolérance est grande, donc moins de segments dans la tessellation.

## 7. Frustum / spatial culling

L'index spatial abstrait (`ISpatialIndex`) gère le culling :

```cpp
// Dans SceneExtractor
auto region = camera.visibleRegion();
auto visible = doc.spatialIndex().query(region);
```

Voir `SPATIAL_INDEX.md`.

## 8. Threading

L'extraction de scène est potentiellement coûteuse. Elle peut être faite hors du thread GL :

```cpp
// app/Viewport.cpp
void Viewport::extractScene() {
    // Lance sur un worker thread
    future_ = std::async([this]() {
        return sceneExtractor_.extract(doc_, camera_, tolerance_);
    });
}

void Viewport::paintGL() {
    if (future_.valid() && future_.wait_for(0ms) == future_status::ready) {
        scene_ = future_.get();
        backend_->uploadScene(scene_);
    }
    backend_->draw();
}
```

## 9. Picking

```cpp
class IRenderBackend {
    // ...
    virtual document::EntityId pick(const geom::Point2& screenPos,
                                    double tolerance) const = 0;
};
```

Implémentation possible :
- CPU : utiliser `ISpatialIndex::pick()`
- GPU : color-pick rendering, lire le pixel sous le curseur

## 10. 2D vs 3D

Le même `IRenderBackend` peut gérer 2D et 3D :

```cpp
// 2D : Camera2D + z constant
// 3D : Camera3D + z variable
// Tessellation : ITessellator 2D ou 3D selon l'entité
```

L'extraction de scène regarde le type d'entité (`Entity2D` vs `Entity3D`) et utilise le tessellator approprié.

## 11. Règles

1. Le Core n'inclut aucun header de rendering
2. `IRenderBackend` est une interface abstraite pure
3. `OpenGLBackend` est l'implémentation par défaut
4. Vulkan/WebGPU sont des implémentations futures
5. La tessellation est faite hors du thread GL
6. Le culling est délégué à `ISpatialIndex`
7. Le picking peut être CPU ou GPU
8. Le LOD est piloté par la caméra