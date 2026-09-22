#pragma once

#include "bcad/commands/Command.h"
#include "bcad/geometry/Entity.h"
#include "bcad/geometry/Transform2D.h"
#include "bcad/geometry/GeometryUtils2.h"
#include "bcad/core/Document.h"
#include <memory>
#include <optional>
#include <string>

namespace bcad::commands {

// --- Commandes concrètes ---

// Ajoute une entité au document
class AddEntityCommand : public Command {
public:
    AddEntityCommand(std::unique_ptr<geom::Entity> entity, std::string text = "Add Entity")
        : entity_(std::move(entity)), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        geom::Entity* raw = doc.addEntity(entity_ ? std::move(entity_) : restoredEntity_->clone());
        entityId_ = raw->id();
    }

    void undo(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            // Capture l'entité actuelle pour un futur redo
            restoredEntity_ = e->clone();
        }
        doc.removeEntity(entityId_);
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<AddEntityCommand>(
            restoredEntity_ ? restoredEntity_->clone() : nullptr, entityId_, text_);
    }

    // Pour le constructeur de clone (entité déjà créée)
    AddEntityCommand(std::unique_ptr<geom::Entity> entity, int entityId, std::string text)
        : entity_(std::move(entity)), entityId_(entityId), text_(std::move(text)) {}

    // execute() est utilisé à la fois pour l'exécution initiale et pour le redo.
    // undo() prépare restoredEntity_ pour un futur redo.
    std::unique_ptr<geom::Entity> entity_;
    std::unique_ptr<geom::Entity> restoredEntity_;  // Clone pour undo/redo
    int entityId_ = -1;
    std::string text_;
};

// Supprime une entité du document
class RemoveEntityCommand : public Command {
public:
    RemoveEntityCommand(int entityId, std::string text = "Remove Entity")
        : entityId_(entityId), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            snapshot_ = e->clone();  // Capture le snapshot juste avant la suppression
        }
        doc.removeEntity(entityId_);
    }

    void undo(core::Document& doc) override {
        if (snapshot_) {
            geom::Entity* raw = doc.addEntity(snapshot_->clone());
            entityId_ = raw->id();  // Nouvel ID après recréation
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<RemoveEntityCommand>(entityId_, snapshot_ ? snapshot_->clone() : nullptr, text_);
    }

public:
    // Constructeur pour clone() - prend l'ID et le snapshot (cloné)
    RemoveEntityCommand(int entityId, std::unique_ptr<geom::Entity> snapshot, std::string text)
        : entityId_(entityId), snapshot_(std::move(snapshot)), text_(std::move(text)) {}

    int entityId_ = -1;
    std::unique_ptr<geom::Entity> snapshot_;
    std::string text_;
};

// Transforme une entité (translation, rotation, échelle, etc.)
class TransformEntityCommand : public Command {
public:
    TransformEntityCommand(int entityId, const geom::Transform2D& transform, std::string text = "Transform")
        : entityId_(entityId), transform_(transform), inverse_(transform.inverse()), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override { apply(transform_, doc); }
    void undo(core::Document& doc) override { apply(inverse_, doc); }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const TransformEntityCommand*>(&other)) {
            if (otherCmd->entityId_ == entityId_) {
                transform_ = otherCmd->transform_ * transform_;
                inverse_ = inverse_ * otherCmd->inverse_;
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<TransformEntityCommand>(entityId_, transform_, text_);
    }

private:
    void apply(const geom::Transform2D& t, core::Document& doc) {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            e->applyTransform(t);
            doc.notifyEntityChanged(e);
        }
    }

    int entityId_ = -1;
    geom::Transform2D transform_;
    geom::Transform2D inverse_;
    std::string text_;
};

// Change le calque d'une entité
class SetLayerCommand : public Command {
public:
    SetLayerCommand(int entityId, std::string newLayer, std::string text = "Change Layer")
        : entityId_(entityId), newLayer_(std::move(newLayer)), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            oldLayer_ = e->layer();  // Capture l'ancien calque au moment de l'exécution
            e->setLayer(newLayer_);
            doc.notifyEntityChanged(e);
        }
    }

    void undo(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            e->setLayer(oldLayer_);
            doc.notifyEntityChanged(e);
        }
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const SetLayerCommand*>(&other)) {
            if (otherCmd->entityId_ == entityId_) {
                newLayer_ = otherCmd->newLayer_;
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<SetLayerCommand>(entityId_, oldLayer_, newLayer_, text_);
    }

public:
    // Constructeur pour clone()
    SetLayerCommand(int entityId, std::string oldLayer, std::string newLayer, std::string text)
        : entityId_(entityId), oldLayer_(std::move(oldLayer)), newLayer_(std::move(newLayer)), text_(std::move(text)) {}

private:
    int entityId_ = -1;
    std::string oldLayer_;
    std::string newLayer_;
    std::string text_;
};

// Change la couleur de substitution d'une entité
class SetColorOverrideCommand : public Command {
public:
    SetColorOverrideCommand(int entityId, std::optional<geom::Color> newColor, std::string text = "Change Color")
        : entityId_(entityId), newColor_(newColor), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            oldColor_ = e->colorOverride();  // Capture l'ancienne couleur
            e->setColorOverride(newColor_);
            doc.notifyEntityChanged(e);
        }
    }

    void undo(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            e->setColorOverride(oldColor_);
            doc.notifyEntityChanged(e);
        }
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const SetColorOverrideCommand*>(&other)) {
            if (otherCmd->entityId_ == entityId_) {
                newColor_ = otherCmd->newColor_;
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<SetColorOverrideCommand>(entityId_, oldColor_, newColor_, text_);
    }

public:
    // Constructeur pour clone()
    SetColorOverrideCommand(int entityId, std::optional<geom::Color> oldColor, std::optional<geom::Color> newColor, std::string text)
        : entityId_(entityId), oldColor_(oldColor), newColor_(newColor), text_(std::move(text)) {}

private:
    int entityId_ = -1;
    std::optional<geom::Color> oldColor_;
    std::optional<geom::Color> newColor_;
    std::string text_;
};

// --- Vague 1: Modification Commands ---

// Déplace une ou plusieurs entités
class MoveCommand : public Command {
public:
    MoveCommand(std::vector<int> entityIds, geom::Vector2 delta, std::string text = "Move")
        : entityIds_(std::move(entityIds)), delta_(delta), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto transform = geom::Transform2D::translation(delta_.x_, delta_.y_);
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                e->applyTransform(geom::Transform2D::translation(delta_.x_, delta_.y_));
                doc.notifyEntityChanged(e);
            }
        }
    }

    void undo(core::Document& doc) override {
        auto inverse = geom::Transform2D::translation(-delta_.x_, -delta_.y_);
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                e->applyTransform(inverse);
                doc.notifyEntityChanged(e);
            }
        }
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const MoveCommand*>(&other)) {
            if (otherCmd->entityIds_ == entityIds_) {
                delta_ = geom::Vector2(delta_.x_ + otherCmd->delta_.x_, delta_.y_ + otherCmd->delta_.y_);
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<MoveCommand>(entityIds_, delta_, text_);
    }

private:
    std::vector<int> entityIds_;
    geom::Vector2 delta_;
    std::string text_;
};

// Copie une ou plusieurs entités avec un déplacement
class CopyCommand : public Command {
public:
    CopyCommand(std::vector<int> entityIds, geom::Vector2 delta, std::string text = "Copy")
        : entityIds_(entityIds), delta_(delta), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        createdIds_.clear();
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                auto clone = e->clone();
                clone->applyTransform(geom::Transform2D::translation(delta_.x_, delta_.y_));
                geom::Entity* raw = doc.addEntity(std::move(clone));
                createdIds_.push_back(raw->id());
            }
        }
    }

    void undo(core::Document& doc) override {
        for (int id : createdIds_) {
            doc.removeEntity(id);
        }
        createdIds_.clear();
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const CopyCommand*>(&other)) {
            if (otherCmd->entityIds_ == entityIds_ && otherCmd->delta_ == delta_) {
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        auto copy = std::make_unique<CopyCommand>(entityIds_, delta_, text_);
        copy->createdIds_ = createdIds_;
        return copy;
    }

private:
    std::vector<int> entityIds_;
    geom::Vector2 delta_;
    std::vector<int> createdIds_;
    std::string text_;
};

// Rotation d'une ou plusieurs entités
class RotateCommand : public Command {
public:
    RotateCommand(std::vector<int> entityIds, geom::Point2 center, double angleRad, std::string text = "Rotate")
        : entityIds_(entityIds), center_(center), angleRad_(angleRad), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto transform = geom::Transform2D::rotation(angleRad_, center_);
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                e->applyTransform(transform);
                doc.notifyEntityChanged(e);
            }
        }
    }

    void undo(core::Document& doc) override {
        auto inverse = geom::Transform2D::rotation(-angleRad_, center_);
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                e->applyTransform(inverse);
                doc.notifyEntityChanged(e);
            }
        }
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const RotateCommand*>(&other)) {
            if (otherCmd->entityIds_ == entityIds_ && otherCmd->center_ == center_) {
                angleRad_ += otherCmd->angleRad_;
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<RotateCommand>(entityIds_, center_, angleRad_, text_);
    }

private:
    std::vector<int> entityIds_;
    geom::Point2 center_;
    double angleRad_;
    std::string text_;
};

// Échelle (mise à l'échelle) d'une ou plusieurs entités
class ScaleCommand : public Command {
public:
    ScaleCommand(std::vector<int> entityIds, geom::Point2 basePoint, double scaleX, double scaleY,
                 std::string text = "Scale")
        : entityIds_(entityIds), basePoint_(basePoint), scaleX_(scaleX), scaleY_(scaleY), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        geom::Transform2D transform = geom::Transform2D::scaling(scaleX_, scaleY_, basePoint_);
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                e->applyTransform(transform);
                doc.notifyEntityChanged(e);
            }
        }
    }

    void undo(core::Document& doc) override {
        geom::Transform2D transform = geom::Transform2D::scaling(scaleX_, scaleY_, basePoint_);
        geom::Transform2D inverse = transform.inverse();
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                e->applyTransform(inverse);
                doc.notifyEntityChanged(e);
            }
        }
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const ScaleCommand*>(&other)) {
            if (otherCmd->entityIds_ == entityIds_ && otherCmd->basePoint_ == basePoint_) {
                scaleX_ *= otherCmd->scaleX_;
                scaleY_ *= otherCmd->scaleY_;
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<ScaleCommand>(entityIds_, basePoint_, scaleX_, scaleY_, text_);
    }

private:
    std::vector<int> entityIds_;
    geom::Point2 basePoint_;
    double scaleX_;
    double scaleY_;
    std::string text_;
};

// Miroir (symétrie) par rapport à une droite
class MirrorCommand : public Command {
public:
    MirrorCommand(std::vector<int> entityIds, geom::Point2 p1, geom::Point2 p2,
                  std::string text = "Mirror")
        : entityIds_(entityIds), p1_(p1), p2_(p2), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto mirror = geom::Transform2D::mirrorAcrossLine(p1_, p2_);
        for (int id : entityIds_) {
            if (auto* e = doc.findEntity(id)) {
                e->applyTransform(mirror);
                doc.notifyEntityChanged(e);
            }
        }
    }

    void undo(core::Document& doc) override {
        // Le miroir est sa propre inverse
        execute(doc);
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherCmd = dynamic_cast<const MirrorCommand*>(&other)) {
            if (otherCmd->entityIds_ == entityIds_ && otherCmd->p1_ == p1_ && otherCmd->p2_ == p2_) {
                return true; // Miroir appliqué deux fois = identité, on peut fusionner en no-op
            }
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<MirrorCommand>(entityIds_, p1_, p2_, text_);
    }

private:
    std::vector<int> entityIds_;
    geom::Point2 p1_;
    geom::Point2 p2_;
    std::string text_;
};

// --- Vague 2: Commandes de découpe et offset ---

// Coupe (Trim) : coupe des entités à une frontière
class TrimCommand : public Command {
public:
    TrimCommand(int entityId, int boundaryEntityId, geom::Point2 pickPoint, std::string text = "Trim")
        : entityId_(entityId), boundaryEntityId_(boundaryEntityId), pickPoint_(pickPoint), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto* entity = doc.findEntity(entityId_);
        auto* boundary = doc.findEntity(boundaryEntityId_);
        if (!entity || !boundary) return;

        // Capture snapshot for undo
        snapshot_ = entity->clone();

        bool trimmed = false;
        if (auto* line = dynamic_cast<geom::LineEntity*>(entity)) {
            trimmed = geom::trimLine(*line, *boundary, pickPoint_);
        } else if (auto* poly = dynamic_cast<geom::PolylineEntity*>(entity)) {
            trimmed = geom::trimPolyline(*poly, *boundary, pickPoint_);
        }
        // TODO: add ArcEntity, CircleEntity support

        if (trimmed) {
            doc.notifyEntityChanged(entity);
        }
    }

    void undo(core::Document& doc) override {
        if (snapshot_) {
            auto* entity = doc.findEntity(entityId_);
            if (!entity) return;

            int id = entity->id();
            std::string layer = entity->layer();
            std::optional<geom::Color> color = entity->colorOverride();
            bool selected = entity->selected;

            doc.removeEntity(id);

            auto restored = snapshot_->clone();
            restored->setId(id);
            restored->setLayer(layer);
            restored->setColorOverride(color);
            restored->selected = selected;
            doc.addEntity(std::move(restored));
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<TrimCommand>(entityId_, boundaryEntityId_, pickPoint_, text_);
    }

private:
    int entityId_ = -1;
    int boundaryEntityId_ = -1;
    geom::Point2 pickPoint_;
    std::string text_;
    std::unique_ptr<geom::Entity> snapshot_;
};

// Prolonge (Extend) : prolonge une entité jusqu'à une frontière
class ExtendCommand : public Command {
public:
    ExtendCommand(int entityId, int boundaryEntityId, geom::Point2 pickPoint, std::string text = "Extend")
        : entityId_(entityId), boundaryEntityId_(boundaryEntityId), pickPoint_(pickPoint), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto* entity = doc.findEntity(entityId_);
        auto* boundary = doc.findEntity(boundaryEntityId_);
        if (!entity || !boundary) return;

        snapshot_ = entity->clone();

        bool extended = false;
        if (auto* line = dynamic_cast<geom::LineEntity*>(entity)) {
            extended = geom::extendLine(*line, *boundary, pickPoint_);
        } else if (auto* poly = dynamic_cast<geom::PolylineEntity*>(entity)) {
            // TODO: geom::extendPolyline
        }
        // TODO: add ArcEntity, CircleEntity support

        if (extended) {
            doc.notifyEntityChanged(entity);
        }
    }

    void undo(core::Document& doc) override {
        if (snapshot_) {
            auto* entity = doc.findEntity(entityId_);
            if (!entity) return;

            int id = entity->id();
            std::string layer = entity->layer();
            std::optional<geom::Color> color = entity->colorOverride();
            bool selected = entity->selected;

            doc.removeEntity(id);

            auto restored = snapshot_->clone();
            restored->setId(id);
            restored->setLayer(layer);
            restored->setColorOverride(color);
            restored->selected = selected;
            doc.addEntity(std::move(restored));
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<ExtendCommand>(entityId_, boundaryEntityId_, pickPoint_, text_);
    }

private:
    int entityId_ = -1;
    int boundaryEntityId_ = -1;
    geom::Point2 pickPoint_;
    std::string text_;
    std::unique_ptr<geom::Entity> snapshot_;
};

// Coupe (Break) : coupe une entité en deux
class BreakCommand : public Command {
public:
    BreakCommand(int entityId, geom::Point2 breakPoint, std::string text = "Break")
        : entityId_(entityId), breakPoint_(breakPoint), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto* entity = doc.findEntity(entityId_);
        if (!entity) return;

        snapshot_ = entity->clone();
        createdIds_.clear();

        if (auto* line = dynamic_cast<geom::LineEntity*>(entity)) {
            auto [l1, l2] = geom::breakLine(*line, breakPoint_);
            if (l1 && l2) {
                // Remove original, add two new lines
                doc.removeEntity(entityId_);
                geom::Entity* raw1 = doc.addEntity(std::move(l1));
                geom::Entity* raw2 = doc.addEntity(std::move(l2));
                createdIds_.push_back(raw1->id());
                createdIds_.push_back(raw2->id());
                entityId_ = raw2->id();  // Keep track for potential future operations
            }
        } else if (auto* poly = dynamic_cast<geom::PolylineEntity*>(entity)) {
            geom::breakPolyline(*poly, breakPoint_);
            doc.notifyEntityChanged(poly);
        }
        // TODO: add ArcEntity, CircleEntity support
    }

    void undo(core::Document& doc) override {
        if (snapshot_) {
            // Remove created entities
            for (int id : createdIds_) {
                if (doc.findEntity(id)) doc.removeEntity(id);
            }
            createdIds_.clear();
            // Restore original
            geom::Entity* raw = doc.addEntity(snapshot_->clone());
            entityId_ = raw->id();
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        auto cmd = std::make_unique<BreakCommand>(entityId_, breakPoint_, text_);
        cmd->createdIds_ = createdIds_;
        return cmd;
    }

private:
    int entityId_ = -1;
    geom::Point2 breakPoint_;
    std::string text_;
    std::unique_ptr<geom::Entity> snapshot_;
    int createdId_ = -1;
    std::vector<int> createdIds_;
};

// Offset : creates a parallel entity at given distance
class OffsetCommand : public Command {
public:
    OffsetCommand(int entityId, double distance, geom::Point2 sidePoint, std::string text = "Offset")
        : entityId_(entityId), distance_(distance), sidePoint_(sidePoint), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto* entity = doc.findEntity(entityId_);
        if (!entity) return;

        createdId_ = -1;

        if (auto* line = dynamic_cast<geom::LineEntity*>(entity)) {
            if (auto offset = geom::offsetLine(*line, distance_, sidePoint_)) {
                geom::Entity* raw = doc.addEntity(std::move(offset));
                createdId_ = raw->id();
            }
        } else if (auto* circle = dynamic_cast<geom::CircleEntity*>(entity)) {
            if (auto offset = geom::offsetCircle(*circle, distance_, sidePoint_)) {
                geom::Entity* raw = doc.addEntity(std::move(offset));
                createdId_ = raw->id();
            }
        } else if (auto* poly = dynamic_cast<geom::PolylineEntity*>(entity)) {
            if (auto offset = geom::offsetPolyline(*poly, distance_, sidePoint_)) {
                geom::Entity* raw = doc.addEntity(std::move(offset));
                createdId_ = raw->id();
            }
        }
        // TODO: add ArcEntity support
    }

    void undo(core::Document& doc) override {
        if (createdId_ != -1) {
            doc.removeEntity(createdId_);
            createdId_ = -1;
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<OffsetCommand>(entityId_, distance_, sidePoint_, text_);
    }

private:
    int entityId_ = -1;
    double distance_ = 0.0;
    geom::Point2 sidePoint_;
    std::string text_;
    int createdId_ = -1;
};

// --- Phase 1: Drawing Commands ---

// Dessine une ligne
class LineCommand : public Command {
public:
    LineCommand(geom::Point2 start, geom::Point2 end, std::string text = "Draw Line")
        : start_(start), end_(end), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto entity = std::make_unique<geom::LineEntity>(start_, end_);
        geom::Entity* raw = doc.addEntity(std::move(entity));
        entityId_ = raw->id();
    }

    void undo(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            restoredEntity_ = e->clone();
            doc.removeEntity(entityId_);
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<LineCommand>(start_, end_, text_);
    }

private:
    geom::Point2 start_;
    geom::Point2 end_;
    int entityId_ = -1;
    std::unique_ptr<geom::Entity> restoredEntity_;
    std::string text_;
};

// Dessine un cercle
class CircleCommand : public Command {
public:
    CircleCommand(geom::Point2 center, double radius, std::string text = "Draw Circle")
        : center_(center), radius_(radius), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto entity = std::make_unique<geom::CircleEntity>(center_, radius_);
        geom::Entity* raw = doc.addEntity(std::move(entity));
        entityId_ = raw->id();
    }

    void undo(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            restoredEntity_ = e->clone();
            doc.removeEntity(entityId_);
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<CircleCommand>(center_, radius_, text_);
    }

private:
    geom::Point2 center_;
    double radius_;
    int entityId_ = -1;
    std::unique_ptr<geom::Entity> restoredEntity_;
    std::string text_;
};

// Dessine un arc
class ArcCommand : public Command {
public:
    ArcCommand(geom::Point2 center, double radius, double startAngle, double endAngle,
               std::string text = "Draw Arc")
        : center_(center), radius_(radius), startAngle_(startAngle), endAngle_(endAngle), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto entity = std::make_unique<geom::ArcEntity>(center_, radius_, startAngle_, endAngle_);
        geom::Entity* raw = doc.addEntity(std::move(entity));
        entityId_ = raw->id();
    }

    void undo(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            restoredEntity_ = e->clone();
            doc.removeEntity(entityId_);
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<ArcCommand>(center_, radius_, startAngle_, endAngle_, text_);
    }

private:
    geom::Point2 center_;
    double radius_;
    double startAngle_;
    double endAngle_;
    int entityId_ = -1;
    std::unique_ptr<geom::Entity> restoredEntity_;
    std::string text_;
};

// Dessine une polyligne
class PolylineCommand : public Command {
public:
    PolylineCommand(std::vector<geom::Point2> points, std::string text = "Draw Polyline")
        : points_(std::move(points)), text_(std::move(text)) {}

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        auto entity = std::make_unique<geom::PolylineEntity>(points_, false);
        geom::Entity* raw = doc.addEntity(std::move(entity));
        entityId_ = raw->id();
    }

    void undo(core::Document& doc) override {
        if (geom::Entity* e = doc.findEntity(entityId_)) {
            restoredEntity_ = e->clone();
            doc.removeEntity(entityId_);
        }
    }

    bool mergeWith(const Command& other) override { return false; }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<PolylineCommand>(points_, text_);
    }

private:
    std::vector<geom::Point2> points_;
    int entityId_ = -1;
    std::unique_ptr<geom::Entity> restoredEntity_;
    std::string text_;
};

} // namespace bcad::commands