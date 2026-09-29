#include "bcad/commands/Command.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "../entities/ParcelEntity.h"
#include "../layout/CadastreSheet.h"
#include "../ParcelOps.h"
#include "bcad/layout/PdfExport.h"
#include "bcad/layout/Scale.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include "bcad/validation/Diagnostics.h"

#include <filesystem>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace bcad::cadastre {

namespace {

std::unique_ptr<ParcelEntity> asParcel(const geom::PolylineEntity& source) {
    auto result = std::make_unique<ParcelEntity>(source.vertices());
    result->properties() = source.properties();
    return result;
}

std::unique_ptr<geom::Entity> cloneEntity(const geom::Entity& entity) {
    return entity.clone();
}

class SplitParcelCommand final : public commands::Command {
public:
    SplitParcelCommand(int entityId, geom::PolylineEntity line)
        : entityId_(entityId), line_(std::move(line)) {}

    std::string_view text() const override { return "cadastre.split_parcel"; }

    void execute(core::Document& doc) override {
        if (!createdIds_.empty()) return;
        auto* entity = doc.findEntity(entityId_);
        auto* parcel = entity ? dynamic_cast<ParcelEntity*>(entity) : nullptr;
        if (!parcel) return;
        original_ = parcel->clone();
        auto split = splitParcel(*parcel, line_);
        if (!split) return;

        doc.removeEntity(entityId_);
        auto first = asParcel(split->first);
        auto second = asParcel(split->second);
        auto* firstAdded = doc.addEntity(std::move(first));
        auto* secondAdded = doc.addEntity(std::move(second));
        if (!firstAdded || !secondAdded) {
            if (firstAdded) doc.removeEntity(firstAdded->id());
            if (secondAdded) doc.removeEntity(secondAdded->id());
            doc.addEntity(original_->clone());
            original_.reset();
            return;
        }
        createdIds_.push_back(firstAdded->id());
        createdIds_.push_back(secondAdded->id());
    }

    void undo(core::Document& doc) override {
        if (createdIds_.empty() || !original_) return;
        for (int id : createdIds_) doc.removeEntity(id);
        doc.addEntity(original_->clone());
        createdIds_.clear();
    }

    std::unique_ptr<commands::Command> clone() const override {
        auto copy = std::make_unique<SplitParcelCommand>(entityId_, line_);
        return copy;
    }

private:
    int entityId_;
    geom::PolylineEntity line_;
    std::unique_ptr<geom::Entity> original_;
    std::vector<int> createdIds_;
};

class MergeParcelsCommand final : public commands::Command {
public:
    explicit MergeParcelsCommand(std::vector<int> entityIds)
        : entityIds_(std::move(entityIds)) {}

    std::string_view text() const override { return "cadastre.merge_parcels"; }

    void execute(core::Document& doc) override {
        if (createdId_ >= 0 || entityIds_.size() < 2) return;
        auto* firstEntity = doc.findEntity(entityIds_[0]);
        auto* secondEntity = doc.findEntity(entityIds_[1]);
        auto* first = firstEntity ? dynamic_cast<ParcelEntity*>(firstEntity) : nullptr;
        auto* second = secondEntity ? dynamic_cast<ParcelEntity*>(secondEntity) : nullptr;
        if (!first || !second) return;
        originals_.clear();
        originals_.push_back(first->clone());
        originals_.push_back(second->clone());
        auto merged = mergeParcels(*first, *second);
        if (!merged) return;

        doc.removeEntity(entityIds_[0]);
        doc.removeEntity(entityIds_[1]);
        auto* added = doc.addEntity(asParcel(*merged));
        if (!added) {
            doc.addEntity(originals_[0]->clone());
            doc.addEntity(originals_[1]->clone());
            originals_.clear();
            return;
        }
        createdId_ = added->id();
    }

    void undo(core::Document& doc) override {
        if (createdId_ < 0) return;
        doc.removeEntity(createdId_);
        for (const auto& original : originals_) doc.addEntity(original->clone());
        createdId_ = -1;
    }

    std::unique_ptr<commands::Command> clone() const override {
        return std::make_unique<MergeParcelsCommand>(entityIds_);
    }

private:
    std::vector<int> entityIds_;
    std::vector<std::unique_ptr<geom::Entity>> originals_;
    int createdId_ = -1;
};

class EditParcelBoundaryCommand final : public commands::Command {
public:
    EditParcelBoundaryCommand(int entityId, std::vector<geom::Point2> vertices)
        : entityId_(entityId), newVertices_(std::move(vertices)) {}

    std::string_view text() const override { return "cadastre.edit_parcel_boundary"; }

    void execute(core::Document& doc) override {
        auto* entity = doc.findEntity(entityId_);
        auto* parcel = entity ? dynamic_cast<ParcelEntity*>(entity) : nullptr;
        if (!parcel || newVertices_.size() < 3) return;
        if (oldVertices_.empty()) oldVertices_ = parcel->vertices();
        parcel->vertices() = newVertices_;
        doc.notifyEntityChanged(parcel);
    }

    void undo(core::Document& doc) override {
        auto* entity = doc.findEntity(entityId_);
        auto* parcel = entity ? dynamic_cast<ParcelEntity*>(entity) : nullptr;
        if (!parcel || oldVertices_.empty()) return;
        parcel->vertices() = oldVertices_;
        doc.notifyEntityChanged(parcel);
    }

    std::unique_ptr<commands::Command> clone() const override {
        return std::make_unique<EditParcelBoundaryCommand>(entityId_, newVertices_);
    }

private:
    int entityId_;
    std::vector<geom::Point2> newVertices_;
    std::vector<geom::Point2> oldVertices_;
};

class GeneratePlanSheetCommand final : public commands::Command {
public:
    explicit GeneratePlanSheetCommand(std::string outputPath)
        : outputPath_(std::move(outputPath)) {}

    std::string_view text() const override { return "cadastre.generate_plan_sheet"; }

    void execute(core::Document& doc) override {
        if (outputPath_.empty()) return;
        createdOutput_ = !std::filesystem::exists(outputPath_);
        const auto bbox = doc.extents();
        if (!bbox.isValid()) return;

        layout::Sheet sheet(layout::PaperFormat::A3, layout::Orientation::Paysage);
        layout::Viewport viewport;
        viewport.setSource(bbox);

        // Les meubles sont résolus par le module : attributs du dossier
        // d'abord, valeur commune aux parcelles ensuite. Ni titre inventé, ni
        // première parcelle qui dicte ce que la feuille affirme (ADR-017).
        // L'échelle se choisit sur la place réellement laissée par les
        // gabarits, pas sur la feuille entière : sinon le plan tombe dessus.
        // Elle revient en littéral dans le cartouche une fois connue.
        layout::PdfExportOptions options;
        options.outputPath = outputPath_;
        options.sheet = sheet;
        options.viewport = viewport;
        options.document = &doc;
        options.permittedScales = defaultPermittedScales();
        layout::applyFittingScale(options);

        layout::FurnitureTemplate cartoucheGabarit = defaultCartoucheTemplate();
        for (auto& attendu : cartoucheGabarit.fields) {
            if (attendu.key.empty())
                attendu.literal =
                    layout::scaleText(static_cast<int>(options.viewport.scale()));
        }
        std::vector<validation::Diagnostic> diagnostics;
        options.meubles.push_back(buildCartoucheFurniture(doc, cartoucheGabarit, diagnostics));
        options.tables.push_back(
            buildNomenclatureFurniture(doc, defaultNomenclatureTemplate()));

        const auto furniture = buildSheetFurniture(doc);
        options.labels = furniture.labels;
        options.bornes = furniture.bornes;
        std::string error;
        generated_ = layout::exportPdf(options, &error);
    }

    void undo(core::Document&) override {
        if (generated_ && createdOutput_) {
            std::error_code error;
            std::filesystem::remove(outputPath_, error);
        }
        generated_ = false;
    }

    std::unique_ptr<commands::Command> clone() const override {
        return std::make_unique<GeneratePlanSheetCommand>(outputPath_);
    }

private:
    std::string outputPath_;
    bool createdOutput_ = false;
    bool generated_ = false;
};

} // namespace

std::unique_ptr<commands::Command> makeSplitParcel(const std::vector<std::string>& args) {
    if (args.size() < 5) return nullptr;
    try {
        const int id = std::stoi(args[0]);
        const double x1 = std::stod(args[1]);
        const double y1 = std::stod(args[2]);
        const double x2 = std::stod(args[3]);
        const double y2 = std::stod(args[4]);
        if (!std::isfinite(x1) || !std::isfinite(y1)
            || !std::isfinite(x2) || !std::isfinite(y2)
            || (x1 == x2 && y1 == y2)) return nullptr;
        return std::make_unique<SplitParcelCommand>(
            id, geom::PolylineEntity({{x1, y1}, {x2, y2}}, false));
    } catch (const std::exception&) {
        return nullptr;
    }
}

std::unique_ptr<commands::Command> makeMergeParcels(const std::vector<std::string>& args) {
    std::vector<int> ids;
    try {
        for (const auto& arg : args) ids.push_back(std::stoi(arg));
    } catch (const std::exception&) {
        return nullptr;
    }
    if (ids.size() < 2) return nullptr;
    return std::make_unique<MergeParcelsCommand>(std::move(ids));
}

std::unique_ptr<commands::Command> makeEditParcelBoundary(const std::vector<std::string>& args) {
    if (args.size() < 7 || (args.size() - 1) % 2 != 0) return nullptr;
    try {
        const int id = std::stoi(args.front());
        std::vector<geom::Point2> vertices;
        vertices.reserve((args.size() - 1) / 2);
        for (std::size_t i = 1; i < args.size(); i += 2) {
            const double x = std::stod(args[i]);
            const double y = std::stod(args[i + 1]);
            if (!std::isfinite(x) || !std::isfinite(y)) return nullptr;
            vertices.emplace_back(x, y);
        }
        return std::make_unique<EditParcelBoundaryCommand>(id, std::move(vertices));
    } catch (const std::exception&) {
        return nullptr;
    }
}

std::unique_ptr<commands::Command> makeGeneratePlanSheet(
    const std::vector<std::string>& args) {
    return std::make_unique<GeneratePlanSheetCommand>(
        args.empty() ? "plan_cadastral.pdf" : args.front());
}

} // namespace bcad::cadastre
