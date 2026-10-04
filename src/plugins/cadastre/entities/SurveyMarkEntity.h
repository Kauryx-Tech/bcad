#pragma once

#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/properties/PropertyMap.h"
#include <memory>
#include <string>

namespace bcad::cadastre {

inline geom::TypeId TypeId_SurveyMark{"cadastre.survey_mark"};

class SurveyMarkEntity : public geom::PointEntity {
public:
    SurveyMarkEntity() = default;
    explicit SurveyMarkEntity(const geom::Point2& pos) : geom::PointEntity(pos) { initProperties(); }

    geom::TypeId typeId() const override { return TypeId_SurveyMark; }

    std::unique_ptr<geom::Entity> clone() const override {
        auto c = std::make_unique<SurveyMarkEntity>(*this);
        c->setId(-1);
        return c;
    }

    std::string serializeParams() const override {
        const auto& props = properties();
        return geom::PointEntity::serializeParams() + '|' +
               props.getString("cadastre.mark_type") + '|' +
               props.getString("cadastre.reference") + '|' +
               std::to_string(props.getDouble("cadastre.precision"));
    }

private:
    void initProperties() {
        auto& props = properties();
        props.addEnum("cadastre.mark_type", 0, {"Borne", "Repère", "PI", "Station"});
        props.addString("cadastre.reference", "");
        props.addDouble("cadastre.precision", 0.01)->setUnit("m");
        // Altitude et code terrain du point de leve (import d'un carnet,
        // K-03). Proprietes generiques : elles voyagent dans le fichier sans
        // changer le format du serializer.
        props.addDouble("cadastre.altitude", 0.0)->setUnit("m");
        props.addString("cadastre.code", "");
    }
};

inline bool isSurveyMark(const geom::Entity* e) {
    return e && e->typeId() == TypeId_SurveyMark;
}

} // namespace bcad::cadastre