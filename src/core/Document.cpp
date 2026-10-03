#include "bcad/core/Document.h"

#include "bcad/events/EventBus.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/index/ISpatialIndex.h"

#include <algorithm>
#include <limits>
#include <map>
#include <mutex>
#include <tuple>

namespace bcad::core {

using geom::BoundingBox;
using geom::Entity;
using geom::Point2;

namespace {
// Valeur par défaut généreuse afin que les premières insertions, avant que
// les vraies dimensions ne soient connues, ne déclenchent pas immédiatement
// un redimensionnement de la racine ; le Quadtree grandit de toute façon à
// la demande.
BoundingBox defaultWorldBounds() {
    return BoundingBox{ -1000.0, -1000.0, 1000.0, 1000.0 };
}
} // namespace

Document::Document() : index_(index::createDefaultSpatialIndex(defaultWorldBounds())) {}

Entity* Document::addEntity(std::unique_ptr<Entity> entity) {
    Entity* raw = nullptr;
    {
        std::unique_lock lock(mutex_);
        // Only assign new ID if entity doesn't already have one (id == -1)
        if (entity->id() == -1) {
            entity->setId(nextId_++);
        }
        if (entity->layer().empty() || layers_.find(entity->layer()) == nullptr) {
            entity->setLayer(layers_.currentLayerName());
        }
        raw = entity.get();
        byId_[raw->id()] = raw;
        entities_.push_back(std::move(entity));
        index_->insert(raw);
    }
    publishEvent(events::EntityAdded{this, raw});
    return raw;
}

void Document::removeEntity(int id) {
    geom::TypeId typeId;
    bool removed = false;
    {
        std::unique_lock lock(mutex_);
        auto it = byId_.find(id);
        if (it != byId_.end()) {
            Entity* raw = it->second;
            typeId = raw->typeId();
            index_->remove(raw);
            byId_.erase(it);
            entities_.erase(std::remove_if(entities_.begin(), entities_.end(),
                                            [&](const auto& e) { return e->id() == id; }),
                             entities_.end());
            removed = true;
        }
    }
    if (removed) {
        publishEvent(events::EntityRemoved{this, id, typeId});
    }
}

Entity* Document::findEntity(int id) const {
    std::shared_lock lock(mutex_);
    auto it = byId_.find(id);
    return it == byId_.end() ? nullptr : it->second;
}

void Document::notifyEntityChanged(Entity* entity) {
    {
        std::unique_lock lock(mutex_);
        index_->update(entity);
    }
    publishEvent(events::EntityModified{this, entity});
}

std::vector<Entity*> Document::entitiesInRegion(const BoundingBox& region) const {
    std::shared_lock lock(mutex_);
    auto hits = index_->query(region);
    std::vector<Entity*> visible;
    visible.reserve(hits.size());
    for (Entity* e : hits) {
        const layers::Layer* layer = layers_.find(e->layer());
        if (layer && layer->visible) visible.push_back(e);
    }
    return visible;
}

Entity* Document::pickEntity(const Point2& p, double tolerance) const {
    std::shared_lock lock(mutex_);
    BoundingBox region = BoundingBox::fromCenterHalfExtent(p, tolerance);
    auto candidates = index_->query(region);

    Entity* best = nullptr;
    double bestDist = std::numeric_limits<double>::infinity();
    for (Entity* e : candidates) {
        const layers::Layer* layer = layers_.find(e->layer());
        if (!layer || !layer->visible || layer->locked) continue;
        double d = e->distanceTo(p);
        if (d <= tolerance && d < bestDist) {
            bestDist = d;
            best = e;
        }
    }
    return best;
}

BoundingBox Document::extents() const {
    std::shared_lock lock(mutex_);
    BoundingBox bb;
    for (const auto& e : entities_) bb.expand(e->boundingBox());
    return bb;
}

void Document::clear() {
    {
        std::unique_lock lock(mutex_);
        entities_.clear();
        byId_.clear();
        index_->clear();
        nextId_ = 1;
        // Vider le document, c'est aussi vider le dossier : « Nouveau » et un
        // import qui remplace le contenu ne doivent pas laisser les attributs
        // du précédent projet accrochés au cartouche.
        properties_ = properties::PropertyMap{};
        sheets_.clear();
    }
    publishEvent(events::DocumentCleared{this});
}

layout::Sheet* Document::addSheet(const std::string& title) {
    std::unique_lock lock(mutex_);
    const auto deja_prise = std::find_if(sheets_.begin(), sheets_.end(),
                                         [&](const auto& feuille) { return feuille->title() == title; });
    if (deja_prise != sheets_.end()) return nullptr;
    auto feuille = std::make_unique<layout::Sheet>();
    feuille->setTitle(title);
    layout::Sheet* raw = feuille.get();
    sheets_.push_back(std::move(feuille));
    return raw;
}

const layout::Sheet* Document::findSheet(const std::string& title) const {
    std::shared_lock lock(mutex_);
    const auto trouve = std::find_if(sheets_.begin(), sheets_.end(),
                                     [&](const auto& feuille) { return feuille->title() == title; });
    return trouve == sheets_.end() ? nullptr : trouve->get();
}

layout::Sheet* Document::findSheet(const std::string& title) {
    std::shared_lock lock(mutex_);
    const auto trouve = std::find_if(sheets_.begin(), sheets_.end(),
                                     [&](const auto& feuille) { return feuille->title() == title; });
    return trouve == sheets_.end() ? nullptr : trouve->get();
}

bool Document::removeSheet(const std::string& title) {
    std::unique_lock lock(mutex_);
    const auto avant = sheets_.size();
    sheets_.erase(std::remove_if(sheets_.begin(), sheets_.end(),
                                 [&](const auto& feuille) { return feuille->title() == title; }),
                  sheets_.end());
    return sheets_.size() != avant;
}

TessellationResult Document::buildTessellation(const BoundingBox& region, double tolerance) const {
    std::shared_lock lock(mutex_);

    TessellationResult result;
    result.region = region;
    result.toleranceUsed = tolerance;

    // Regrouper d'abord par couleur résolue pour que le moteur de rendu
    // puisse émettre un seul appel glMultiDrawArrays par couleur au lieu
    // d'un appel de dessin par entité.
    struct ColorKey {
        float r, g, b, a;
        bool operator<(const ColorKey& o) const {
            return std::tie(r, g, b, a) < std::tie(o.r, o.g, o.b, o.a);
        }
    };
    std::map<ColorKey, ColorBatch> batches;

    for (Entity* e : index_->query(region)) {
        const layers::Layer* layer = layers_.find(e->layer());
        if (!layer || !layer->visible) continue;

        geom::Color color = e->colorOverride().value_or(layer->color);
        ColorKey key{ color.r, color.g, color.b, color.a };
        auto& batch = batches[key];
        batch.color = color;

        auto addRing = [&](const std::vector<Point2>& ring) {
            if (ring.size() < 2) return;
            batch.firsts.push_back(static_cast<std::int32_t>(batch.vertices.size() / 2));
            batch.counts.push_back(static_cast<std::int32_t>(ring.size()));
            for (const auto& p : ring) {
                batch.vertices.push_back(static_cast<float>(p.x_));
                batch.vertices.push_back(static_cast<float>(p.y_));
            }
        };

        // Polygone avec trous : chaque anneau = range GL séparée pour que les
        // trous soient visibles comme des bords distincts (§0.3).
        if (const auto* poly = dynamic_cast<const geom::PolylineEntity*>(e)) {
            if (poly->hasHoles()) {
                addRing(e->tessellate(tolerance));
                for (const auto& hole : poly->holes()) {
                    if (hole.size() < 3) continue;
                    std::vector<Point2> closedHole = hole;
                    closedHole.push_back(closedHole.front());
                    addRing(closedHole);
                }
                continue;
            }
        }

        std::vector<Point2> pts = e->tessellate(tolerance);
        if (pts.size() < 2) continue;
        batch.firsts.push_back(static_cast<std::int32_t>(batch.vertices.size() / 2));
        batch.counts.push_back(static_cast<std::int32_t>(pts.size()));
        for (const auto& p : pts) {
            batch.vertices.push_back(static_cast<float>(p.x_));
            batch.vertices.push_back(static_cast<float>(p.y_));
        }
    }

    result.batches.reserve(batches.size());
    for (auto& [key, batch] : batches) result.batches.push_back(std::move(batch));
    return result;
}

void Document::publishEvent(const events::Event& event) const {
    events::EventBus::instance().publish(event);
}

} // namespace bcad::core