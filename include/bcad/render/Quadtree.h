#pragma once

#include "bcad/geometry/BoundingBox.h"
#include "bcad/geometry/Entity.h"
#include <memory>
#include <vector>

namespace bcad::render {

// Quadtree "lâche" (loose) sur les boîtes englobantes des entités. Pilier à
// la fois du culling du viewport (interroger la bbox de la zone visible) et
// du pick/sélection (interroger une petite bbox autour du curseur) — voir la
// section "Spatial Indexing" du document d'architecture.
class Quadtree {
public:
    explicit Quadtree(geom::BoundingBox worldBounds, int maxItemsPerNode = 8, int maxDepth = 10);
    ~Quadtree();

    void insert(geom::Entity* entity);
    void remove(geom::Entity* entity);
    // À appeler après qu'une entité déjà présente dans l'arbre a été
    // déplacée/redimensionnée.
    void update(geom::Entity* entity);

    void clear();
    void rebuild(const std::vector<geom::Entity*>& entities);

    // Toutes les entités dont la bbox intersecte la région interrogée (un
    // sur-ensemble — le test de collision exact incombe à l'appelant via
    // Entity::distanceTo).
    std::vector<geom::Entity*> query(const geom::BoundingBox& region) const;

    std::size_t size() const { return entityCount_; }
    const geom::BoundingBox& bounds() const { return worldBounds_; }

    // Type de nœud opaque : déclaré ici par anticipation, entièrement défini
    // seulement dans Quadtree.cpp (ses fonctions libres de parcours d'arbre
    // ont besoin d'y accéder, et le C++ n'a pas de "friend pour toute
    // l'unité de traduction" — public+opaque est plus simple que de
    // déclarer chaque fonction auxiliaire amie individuellement).
    struct Node;

private:
    geom::BoundingBox worldBounds_;
    int maxItemsPerNode_;
    int maxDepth_;
    std::unique_ptr<Node> root_;
    std::size_t entityCount_ = 0;
};

} // namespace bcad::render
