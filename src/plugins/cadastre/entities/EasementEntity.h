#pragma once

#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/properties/PropertyMap.h"
#include <memory>
#include <string>
#include <vector>

namespace bcad::cadastre {

inline geom::TypeId TypeId_Easement{"cadastre.easement"};

class EasementEntity : public geom::PolylineEntity {
public:
    EasementEntity() = default;
    explicit EasementEntity(std::vector<geom::Point2> vertices)
        : geom::PolylineEntity(std::move(vertices), false) { initProperties(); }

    geom::TypeId typeId() const override { return TypeId_Easement; }

    std::unique_ptr<geom::Entity> clone() const override {
        auto c = std::make_unique<EasementEntity>(*this);
        c->setId(-1);
        return c;
    }

    std::string serializeParams() const override {
        const auto& props = properties();
        return PolylineEntity::serializeParams() + '|' +
               props.getString("cadastre.easement_type") + '|' +
               props.getString("cadastre.beneficiaire") + '|' +
               props.getString("cadastre.reference");
    }

private:
    void initProperties() {
        auto& props = properties();
        props.addEnum("cadastre.easement_type", 0, {"Passage", "Vues", "Égouts", "Réseaux", "Autre"});
        props.addString("cadastre.beneficiaire", "");
        props.addString("cadastre.reference", "");
    }
};

inline bool isEasement(const geom::Entity* e) {
    return e && e->typeId() == TypeId_Easement;
}

} // namespace bcad::cadastre