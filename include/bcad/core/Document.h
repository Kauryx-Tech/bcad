#pragma once

#include "bcad/core/TessellationTypes.h"
#include "bcad/geometry/Entity.h"
#include "bcad/index/ISpatialIndex.h"
#include "bcad/layers/LayerManager.h"
#include "bcad/events/EventBus.h"
#include "bcad/layout/Sheet.h"
#include "bcad/properties/PropertyMap.h"
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

    // Les attributs du dossier : ce que le document porte, à côté du dessin —
    // projet, phase, géomètre, numéro de dossier. Le cartouche en a besoin, et
    // sans eux il ne peut rien afficher que ne dicte une parcelle.
    //
    // Ils ne sont pas dessinés, pas indexés, pas dans `extents()` : ils ne
    // passent par aucun des chemins géométriques de cette classe. Et ils ne
    // sont PAS encore sauvegardés — le format ne connaît pas de table pour eux
    // avant v3 (ADR-017). Les clés sont du vocabulaire de module : l'hôte ne
    // nomme aucun champ, il porte le conteneur (ADR-016 §4).
    properties::PropertyMap& properties() { return properties_; }
    const properties::PropertyMap& properties() const { return properties_; }

    // Les feuilles de mise en page : l'espace papier du document, a cote du
    // dessin et hors de lui (ADR-017 decision 1). Une feuille est designee par le
    // nom que l'operateur lui donne, et elle tient ses vues et ses meubles.
    //
    // Elles ne sont ni dessinees ni indexees : `extents()`, l'index spatial, la
    // tessellation et le picking ne les voient pas, et une vue grande comme une
    // commune ne change rien aux mesures du dessin. Comme les attributs du
    // dossier, elles ne sont PAS encore sauvegardees : la table qui les recoit
    // est ecrite (v3) mais pas encore lue ni remplie.
    const std::vector<std::unique_ptr<layout::Sheet>>& sheets() const { return sheets_; }
    // nullptr si ce nom est deja porte par une feuille : c'est par lui que
    // l'operateur designe l'une, et par lui que le fichier les distinguera.
    layout::Sheet* addSheet(const std::string& title);
    layout::Sheet* findSheet(const std::string& title);
    const layout::Sheet* findSheet(const std::string& title) const;
    bool removeSheet(const std::string& title);

    const index::ISpatialIndex& spatialIndex() const { return *index_; }

    void clear();

    // Construit des lots de sommets prêts pour le GPU pour chaque entité
    // visible qui intersecte `region`, regroupés par couleur résolue. Peut
    // être appelé en toute sécurité depuis un thread de tessellation en
    // arrière-plan pendant que le thread GUI modifie le document : les
    // lectures prennent un verrou partagé, les mutations (add/remove/
    // notifyEntityChanged) prennent un verrou exclusif.
    TessellationResult buildTessellation(const geom::BoundingBox& region, double tolerance) const;

private:
    void publishEvent(const events::Event& event) const;

    std::vector<std::unique_ptr<geom::Entity>> entities_;
    std::unordered_map<int, geom::Entity*> byId_;
    layers::LayerManager layers_;
    properties::PropertyMap properties_;
    std::vector<std::unique_ptr<layout::Sheet>> sheets_;
    std::unique_ptr<index::ISpatialIndex> index_;
    int nextId_ = 1;
    mutable std::shared_mutex mutex_;
};

} // namespace bcad::core
