// Presse-papiers des objets (E-01, COPYCLIP / COPYBASE / PASTECLIP
// d'AutoCAD) : un seul pour toute l'application, donc partage entre les
// dessins ouverts dans les onglets. Il garde des copies (l'original peut etre
// efface ou le dessin ferme), le point de base, et les calques des objets
// pour les recreer dans un dessin qui ne les a pas.
//
// Pur (aucun Qt) : teste par entity_clipboard_test.

#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/geometry/Transform2D.h"
#include "bcad/layers/LayerManager.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace bcad::app {

class EntityClipboard {
public:
    // L'instance de l'application.
    static EntityClipboard& instance() {
        static EntityClipboard clipboard;
        return clipboard;
    }

    // Remplace le contenu par des copies de `entities`, deselectionnees.
    void store(const std::vector<geom::Entity*>& entities, const geom::Point2& base,
               const layers::LayerManager& layers) {
        entities_.clear();
        layers_.clear();
        base_ = base;
        for (const geom::Entity* e : entities) {
            std::unique_ptr<geom::Entity> copy = e->clone();
            copy->selected = false;
            entities_.push_back(std::move(copy));
            const bool known = std::any_of(layers_.begin(), layers_.end(),
                                           [&](const layers::Layer& l) { return l.name == e->layer(); });
            if (!known) {
                if (const layers::Layer* layer = layers.find(e->layer())) layers_.push_back(*layer);
            }
        }
    }

    bool empty() const { return entities_.empty(); }
    std::size_t size() const { return entities_.size(); }
    const geom::Point2& base() const { return base_; }
    const std::vector<std::unique_ptr<geom::Entity>>& entities() const { return entities_; }

    // Copies a coller, le point de base pose sur `insertion`.
    std::vector<std::unique_ptr<geom::Entity>> clonesAt(const geom::Point2& insertion) const {
        const auto shift = geom::Transform2D::translation(insertion.x_ - base_.x_, insertion.y_ - base_.y_);
        std::vector<std::unique_ptr<geom::Entity>> out;
        for (const auto& e : entities_) {
            std::unique_ptr<geom::Entity> copy = e->clone();
            copy->applyTransform(shift);
            out.push_back(std::move(copy));
        }
        return out;
    }

    // Cree dans `layers` les calques des objets copies qui y manquent, avec
    // leurs reglages (couleur, epaisseur, type de ligne, etat) ; les calques
    // existants du dessin cible gardent les leurs, comme AutoCAD.
    void ensureLayers(layers::LayerManager& layers) const {
        for (const layers::Layer& source : layers_) {
            if (layers.find(source.name)) continue;
            layers::Layer& created = layers.createLayer(source.name, source.color);
            created.lineWeight = source.lineWeight;
            created.lineType = source.lineType;
            created.visible = source.visible;
            created.locked = source.locked;
        }
    }

    // Point de base de COPYCLIP : coin inferieur gauche de l'emprise.
    static geom::Point2 lowerLeft(const std::vector<geom::Entity*>& entities) {
        geom::BoundingBox box;
        for (const geom::Entity* e : entities) box.expand(e->boundingBox());
        return box.isValid() ? geom::Point2(box.minX, box.minY) : geom::Point2(0, 0);
    }

private:
    std::vector<std::unique_ptr<geom::Entity>> entities_;
    std::vector<layers::Layer> layers_;
    geom::Point2 base_{0, 0};
};

} // namespace bcad::app
