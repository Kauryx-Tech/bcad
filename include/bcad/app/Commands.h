#pragma once

#include "bcad/core/Document.h"
#include "bcad/geometry/Types.h"
#include <QUndoCommand>
#include <memory>
#include <optional>
#include <string>

namespace bcad::app {

// Adds an entity on redo, removes it on undo. Takes ownership of a
// freshly-created entity for the first redo(); after an undo, redo() re-adds
// a clone of the last-known state, since Document::removeEntity destroys the
// original and entity ids are reassigned on every insert.
class AddEntityCommand : public QUndoCommand {
public:
    AddEntityCommand(core::Document* doc, std::unique_ptr<geom::Entity> entity, const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    std::unique_ptr<geom::Entity> pending_;
    std::unique_ptr<geom::Entity> snapshot_;
    int entityId_ = -1;
};

// Mirror image of AddEntityCommand: removes on redo, re-adds a clone on undo.
class RemoveEntityCommand : public QUndoCommand {
public:
    RemoveEntityCommand(core::Document* doc, geom::Entity* entity, const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    std::unique_ptr<geom::Entity> snapshot_;
    int entityId_;
};

// Applies `transform` on redo, its inverse on undo. The entity is mutated in
// place (id stable across undo/redo), unlike Add/Remove.
class TransformEntityCommand : public QUndoCommand {
public:
    TransformEntityCommand(core::Document* doc, geom::Entity* entity, const geom::AffTransform2& transform,
                            const QString& text);

    void redo() override;
    void undo() override;

private:
    void apply(const geom::AffTransform2& t);

    core::Document* doc_;
    int entityId_;
    geom::AffTransform2 transform_;
    geom::AffTransform2 inverse_;
};

// Sets an entity's layer on redo, restores the previous one on undo. Used by
// the properties panel.
class SetLayerCommand : public QUndoCommand {
public:
    SetLayerCommand(core::Document* doc, geom::Entity* entity, std::string newLayer, const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    int entityId_;
    std::string oldLayer_;
    std::string newLayer_;
};

// Sets (or clears) an entity's color override on redo, restores the
// previous one on undo. `std::nullopt` means "ByLayer" (no override).
class SetColorOverrideCommand : public QUndoCommand {
public:
    SetColorOverrideCommand(core::Document* doc, geom::Entity* entity, std::optional<geom::Color> newColor,
                             const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    int entityId_;
    std::optional<geom::Color> oldColor_;
    std::optional<geom::Color> newColor_;
};

} // namespace bcad::app
