#pragma once

#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/properties/PropertyMap.h"
#include <memory>
#include <string>
#include <vector>

namespace bcad::cadastre {

inline geom::TypeId TypeId_Parcel{"cadastre.parcel"};

class ParcelEntity : public geom::PolylineEntity {
public:
    ParcelEntity() = default;
    explicit ParcelEntity(std::vector<geom::Point2> vertices)
        : geom::PolylineEntity(std::move(vertices), true) {
        initProperties();
    }
    ParcelEntity(const ParcelEntity&) = default;
    ParcelEntity(ParcelEntity&&) noexcept = default;
    ParcelEntity& operator=(const ParcelEntity&) = default;
    ParcelEntity& operator=(ParcelEntity&&) noexcept = default;
    ~ParcelEntity() override = default;

    geom::TypeId typeId() const override { return TypeId_Parcel; }

    std::unique_ptr<geom::Entity> clone() const override {
        auto c = std::make_unique<ParcelEntity>(*this);
        c->setId(-1);
        return c;
    }

    std::string serializeParams() const override {
        return PolylineEntity::serializeParams() + '|' +
               properties().getString("cadastre.section") + '|' +
               properties().getString("cadastre.numero");
    }

    static std::unique_ptr<ParcelEntity> createDefault() {
        std::vector<geom::Point2> v = {{0,0},{10,0},{10,5},{0,5}};
        auto e = std::make_unique<ParcelEntity>(std::move(v));
        e->properties().setString("cadastre.section", "A");
        e->properties().setString("cadastre.numero", "001");
        e->properties().setString("cadastre.contenance", "50m²");
        return e;
    }

private:
    void initProperties() {
        auto& props = properties();
        props.addString("cadastre.section", "");
        props.addString("cadastre.numero", "");
        props.addString("cadastre.contenance", "");
        props.addString("cadastre.commune", "");
        props.addString("cadastre.proprietaire", "");
        props.addEnum("cadastre.nature", 0, {"Bâtie", "Non bâtie", "Agricole", "Domaine public"});
    }
};

inline bool isCadastreParcel(const geom::Entity* e) {
    if (!e) return false;
    if (e->typeId() == TypeId_Parcel) return true;
    if (e->typeId() != geom::TypeId_Polyline) return false;
    const auto* poly = static_cast<const geom::PolylineEntity*>(e);
    return poly->closed() && poly->properties().has("cadastre.section");
}

} // namespace bcad::cadastre