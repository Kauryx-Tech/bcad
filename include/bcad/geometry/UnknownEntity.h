#pragma once

#include "bcad/geometry/Entity.h"
#include "bcad/properties/PropertyMap.h"
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace bcad::geom {

// Entite d'un module absent, conservee sans etre comprise.
//
// Le chargeur `.bcad` rencontre un `type_id` qu'aucun serializer enregistre ne
// sait lire : c'est le cas normal d'un poste ou le module n'est pas installe.
// Abandonner l'entite a ce moment-la rendait un simple ouvrir/enregistrer
// destructeur pour le travail d'un collegue equipe d'un module. Ici le type, la
// chaine de parametres brute et les proprietes sont conserves tels quels, et
// reecrits tels quels.
//
// L'objet ne redevient pas l'entite reelle dans la session courante : la
// reconversion exigerait le code du module, qui est justement absent. Elle a
// lieu a la rouverture, une fois le module charge, quand son serializer retrouve
// le type.
//
// Consequence assumee : l'entite est invisible et incrochable a l'ecran (aucune
// emprise, aucune tessellation), mais elle reste comptee, listee et enregistree.
class UnknownEntity final : public Entity {
public:
    UnknownEntity(TypeId originalTypeId, std::string payload)
        : originalTypeId_(std::move(originalTypeId)), payload_(std::move(payload)) {}

    // Le type reel du fichier, pas un type de repli : c'est lui qui est reecrit.
    TypeId typeId() const override { return originalTypeId_; }

    // Aucun equivalent natif : le type n'est pas dans l'enum historique, et
    // c'est volontaire — typeId() porte l'identifiant veritable.
    EntityType type() const override { return static_cast<EntityType>(-1); }

    // Emprise invalide : l'index spatial ignore une emprise vide a l'insertion,
    // l'entite est donc presentee au document sans fausser les bornes du dessin.
    BoundingBox boundingBox() const override { return BoundingBox{}; }

    // Pas de transformation : appliquer une matrice a un contenu qu'on ne lit
    // pas le corromprait sans moyen de revenir en arriere.
    void applyTransform(const Transform2D&) override {}

    std::unique_ptr<Entity> clone() const override {
        return std::make_unique<UnknownEntity>(*this);
    }

    std::vector<Point2> tessellate(double /*maxDeviation*/) const override { return {}; }

    // Infini : aucune tolerance de selection n'attrape ce qui n'a pas de
    // geometrie lisible. L'entite se supprime et se deplace avec le dessin,
    // elle ne se picore pas.
    double distanceTo(const Point2&) const override {
        return std::numeric_limits<double>::infinity();
    }

    // Le payload est le contenu du fichier, octet pour octet.
    std::string serializeParams() const override { return payload_; }

    // Le DXF n'a pas ou loger un payload opaque : ce contenu est perdu a
    // l'export d'change, jamais a l'enregistrement natif.
    void writeDxf(std::ostream& /*f*/, const std::string& /*layer*/,
                  const std::optional<Color>& /*colorOverride*/) const override {}

    std::string geometryInfo() const override {
        std::ostringstream ss;
        ss << "Contenu conserve, module absent\ntype : " << originalTypeId_.value
           << "\nparametres : " << payload_.size() << " octets";
        return ss.str();
    }

    const TypeId& originalTypeId() const { return originalTypeId_; }
    const std::string& payload() const { return payload_; }

    properties::PropertyMap& properties() override { return properties_; }
    const properties::PropertyMap& properties() const override { return properties_; }

private:
    void doAddSnapCandidates(const Point2& /*cursor*/, SnapCallback /*add*/) const override {}

    TypeId originalTypeId_;
    std::string payload_;
    properties::PropertyMap properties_;
};

} // namespace bcad::geom
