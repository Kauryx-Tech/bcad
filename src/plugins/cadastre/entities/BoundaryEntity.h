#pragma once

#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/properties/PropertyMap.h"
#include <memory>
#include <string>
#include <vector>

namespace bcad::cadastre {

inline geom::TypeId TypeId_Boundary{"cadastre.boundary"};

class BoundaryEntity : public geom::PolylineEntity {
public:
    BoundaryEntity() = default;
    explicit BoundaryEntity(std::vector<geom::Point2> vertices, bool closed = false)
        : geom::PolylineEntity(std::move(vertices), closed) {
        initProperties();
    }

    geom::TypeId typeId() const override { return TypeId_Boundary; }

    std::unique_ptr<geom::Entity> clone() const override {
        auto c = std::make_unique<BoundaryEntity>(*this);
        c->setId(-1);
        return c;
    }

    std::string serializeParams() const override {
        return PolylineEntity::serializeParams() + '|' +
               properties().getString("cadastre.boundary_type") + '|' +
               properties().getString("cadastre.reference");
    }

private:
    void initProperties() {
        auto& props = properties();
        props.addEnum("cadastre.boundary_type", 0, {"Administrative", "Physique", "Naturelle", "Conventionnelle"});
        props.addString("cadastre.reference", "");
    }
};

inline bool isCadastreBoundary(const geom::Entity* e) {
    return e && e->typeId() == TypeId_Boundary;
}

} // namespace bcad::cadastre