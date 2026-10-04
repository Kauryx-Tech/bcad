// Convertir des polylignes fermees en parcelles (K-02).
//
// Un plan existant arrive en polylignes (import DXF, dessin a la main) : sans
// cette commande, il fallait redessiner chaque parcelle. La geometrie est
// gardee telle quelle — contour, trous, calque, couleur — et les attributs
// cadastraux de la parcelle restent a saisir ; une polyligne ouverte, ou deja
// une parcelle, est laissee de cote.

#include "../entities/ParcelEntity.h"

#include "bcad/commands/Command.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Polyline.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace bcad::cadastre {

namespace {

class ConvertToParcelCommand final : public commands::Command {
public:
    explicit ConvertToParcelCommand(std::vector<int> ids) : ids_(std::move(ids)) {}

    std::string_view text() const override { return "cadastre.convert_to_parcel"; }

    void execute(core::Document& doc) override {
        if (!converted_.empty()) return;
        for (size_t i = 0; i < ids_.size(); ++i) {
            auto* entity = doc.findEntity(ids_[i]);
            // Seule une polyligne de l'hote, fermee, devient parcelle.
            if (!entity || entity->typeId() != geom::TypeId_Polyline) continue;
            const auto* source = static_cast<const geom::PolylineEntity*>(entity);
            if (!source->closed() || source->vertices().size() < 3) continue;

            auto parcel = std::make_unique<ParcelEntity>(source->vertices());
            for (const auto& hole : source->holes()) parcel->addHole(hole);
            parcel->setLayer(source->layer());
            if (source->colorOverride()) parcel->setColorOverride(source->colorOverride());

            Conversion done;
            done.original = source->clone();
            done.originalId = ids_[i];
            doc.removeEntity(ids_[i]);
            // Retablir rend chaque parcelle sous l'id de sa premiere creation.
            if (i < parcelIds_.size()) parcel->setId(parcelIds_[i]);
            done.parcelId = doc.addEntity(std::move(parcel))->id();
            converted_.push_back(std::move(done));
        }
        parcelIds_.clear();
        for (const auto& done : converted_) parcelIds_.push_back(done.parcelId);
    }

    void undo(core::Document& doc) override {
        for (auto& done : converted_) {
            doc.removeEntity(done.parcelId);
            auto restored = done.original->clone();
            restored->setId(done.originalId);
            doc.addEntity(std::move(restored));
        }
        converted_.clear();
    }

    std::unique_ptr<commands::Command> clone() const override {
        return std::make_unique<ConvertToParcelCommand>(ids_);
    }

private:
    struct Conversion {
        std::unique_ptr<geom::Entity> original;
        int originalId = -1;
        int parcelId = -1;
    };
    std::vector<int> ids_;
    std::vector<Conversion> converted_;
    std::vector<int> parcelIds_;
};

} // namespace

// Arguments : les ids des entites selectionnees (WorkbenchParams::SelectionIds).
std::unique_ptr<commands::Command> makeConvertToParcel(const std::vector<std::string>& args) {
    std::vector<int> ids;
    try {
        for (const auto& arg : args) ids.push_back(std::stoi(arg));
    } catch (const std::exception&) {
        return nullptr;
    }
    if (ids.empty()) return nullptr;
    return std::make_unique<ConvertToParcelCommand>(std::move(ids));
}

} // namespace bcad::cadastre
