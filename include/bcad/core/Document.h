#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/layers/LayerManager.h"
#include "bcad/render/Quadtree.h"
#include "bcad/render/TessellationTypes.h"
#include <functional>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace bcad::core {

// Le modèle en mémoire d'un dessin : les entités, leurs calques, et l'index
// spatial maintenu synchronisé avec les deux. Tout le reste (moteur de rendu,
// outils, E/S DXF/SQLite) opère sur un Document.
class Document {
public:
    Document();

    geom::Entity* addEntity(std::unique_ptr<geom::Entity> entity);
    void removeEntity(int id);
    geom::Entity* findEntity(int id) const;

    // À appeler après avoir modifié une entité déjà possédée par le document
    // (déplacement, rotation, édition de sommet, ...) pour que l'index
    // spatial reste correct.
    void notifyEntityChanged(geom::Entity* entity);

    const std::vector<std::unique_ptr<geom::Entity>>& entities() const { return entities_; }

    std::vector<geom::Entity*> entitiesInRegion(const geom::BoundingBox& region) const;

    // Entité la plus proche de `p` dans un rayon de `tolerance` unités monde,
    // ou nullptr. Les calques verrouillés/masqués sont ignorés.
    geom::Entity* pickEntity(const geom::Point2& p, double tolerance) const;

    geom::BoundingBox extents() const;

    layers::LayerManager& layerManager() { return layers_; }
    const layers::LayerManager& layerManager() const { return layers_; }

    const render::Quadtree& spatialIndex() const { return *index_; }

    void clear();

    // Construit des lots de sommets prêts pour le GPU pour chaque entité
    // visible qui intersecte `region`, regroupés par couleur résolue. Peut
    // être appelé en toute sécurité depuis un thread de tessellation en
    // arrière-plan pendant que le thread GUI modifie le document : les
    // lectures prennent un verrou partagé, les mutations (add/remove/
    // notifyEntityChanged) prennent un verrou exclusif.
    render::TessellationResult buildTessellation(const geom::BoundingBox& region, double tolerance) const;

    // Déclenché sur tout changement structurel (ajout/suppression/
    // transformation) ; la GUI s'y accroche pour déclencher un rafraîchissement
    // au lieu de faire un polling à chaque frame.
    std::function<void()> onChanged;

private:
    void notifyChanged();

    std::vector<std::unique_ptr<geom::Entity>> entities_;
    std::unordered_map<int, geom::Entity*> byId_;
    layers::LayerManager layers_;
    std::unique_ptr<render::Quadtree> index_;
    int nextId_ = 1;
    mutable std::shared_mutex mutex_;
};

} // namespace bcad::core
