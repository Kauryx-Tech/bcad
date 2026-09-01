# Exemple 2 : Explorer le Document

> Comment comprendre et utiliser `bcad::core::Document`.

## Le Document dans le code actuel

```cpp
// include/bcad/core/Document.h
namespace bcad::core {

class Document {
public:
    Document();
    
    // Ajouter une entité
    geom::Entity* addEntity(std::unique_ptr<geom::Entity> entity);
    void removeEntity(int id);
    geom::Entity* findEntity(int id) const;
    
    // Calques
    layers::LayerManager& layerManager();
    
    // Sélection
    geom::Entity* pickEntity(const geom::Point2& p, double tolerance) const;
    
    // Extension du dessin
    geom::BoundingBox extents() const;
};

}
```

## Exemple : créer un document avec des entités

```cpp
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Circle.h"

int main() {
    bcad::core::Document doc;
    
    // Créer une ligne
    auto line = std::make_unique<bcad::geom::LineEntity>(
        bcad::geom::Point2(0, 0),
        bcad::geom::Point2(100, 100)
    );
    line->setLayer("0");
    
    // Ajouter au document
    geom::Entity* added = doc.addEntity(std::move(line));
    printf("Ligne ajoutée avec ID: %d\n", added->id());
    
    // Créer un cercle
    auto circle = std::make_unique<bcad::geom::CircleEntity>(
        bcad::geom::Point2(50, 50), 25.0
    );
    circle->setLayer("0");
    doc.addEntity(std::move(circle));
    
    // Obtenir toutes les entités
    for (const auto& entity : doc.entities()) {
        printf("Entité ID=%d, type=%s\n",
               entity->id(),
               bcad::geom::entityTypeName(entity->type()));
    }
    
    // Sélectionner une entité
    geom::Entity* picked = doc.pickEntity(bcad::geom::Point2(50, 50), 10.0);
    if (picked) {
        printf("Entité sélectionnée: %s\n", 
               bcad::geom::entityTypeName(picked->type()));
    }
    
    // Extension du dessin
    auto bb = doc.extents();
    printf("Bounding box: (%.2f, %.2f) - (%.2f, %.2f)\n",
           bb.minX, bb.minY, bb.maxX, bb.maxY);
    
    return 0;
}
```

## Exemple : utiliser les calques

```cpp
#include "bcad/layers/LayerManager.h"

int main() {
    bcad::layers::LayerManager lm;
    
    // Créer des calques
    lm.createLayer("Murs", bcad::geom::Color::fromRgb255(0, 0, 255));
    lm.createLayer("Portes", bcad::geom::Color::fromRgb255(255, 0, 0));
    
    // Lister les calques
    for (const auto& layer : lm.layers()) {
        printf("Calque: %s\n", layer.name.c_str());
    }
    
    return 0;
}
```

## Violations architecturales connues

Le document actuel **viole** les principes architecturaux :

1. **Document → Render** : `Document.h` inclut `<bcad/render/Quadtree.h>`
2. **Document → Tessellation** : `Document.h` inclut `<bcad/render/TessellationTypes.h>`

Ces inclusions seront supprimées dans la Phase 3 de la roadmap.

## Voir aussi

- `docs/DOCUMENT_MODEL.md` — modèle cible
- `docs/ARCHITECTURE_ROADMAP.md` — Phase 3
- `include/bcad/core/Document.h` — code source