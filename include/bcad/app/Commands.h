#pragma once

#include "bcad/core/Document.h"
#include "bcad/geometry/Types.h"
#include "bcad/properties/PropertyTypes.h"
#include <QUndoCommand>
#include <memory>
#include <optional>
#include <string>

namespace bcad::app {

// Ajoute une entité au redo, la retire à l'undo. Prend possession d'une
// entité fraîchement créée pour le premier redo() ; après un undo, redo()
// rajoute un clone du dernier état connu, car Document::removeEntity détruit
// l'original et les identifiants d'entité sont réattribués à chaque insertion.
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

// Image miroir de AddEntityCommand : retire au redo, rajoute un clone à l'undo.
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

// Applique `transform` au redo, son inverse à l'undo. L'entité est modifiée
// en place (id stable entre undo/redo), contrairement à Add/Remove.
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

// Définit le calque d'une entité au redo, restaure le précédent à l'undo.
// Utilisé par le panneau de propriétés.
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

// Définit (ou efface) la couleur de substitution d'une entité au redo,
// restaure la précédente à l'undo. `std::nullopt` signifie "ByLayer" (pas de
// substitution).
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

// Modifie une propriété de type string d'une entité (ex: cadastre.section, cadastre.numero, etc.)
// au redo, restaure la valeur précédente à l'undo.
class SetPropertyCommand : public QUndoCommand {
public:
    SetPropertyCommand(core::Document* doc, geom::Entity* entity, std::string key, std::string newValue,
                        const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    int entityId_;
    std::string key_;
    std::string newValue_;
    std::string oldValue_;
};

class SetEnumPropertyCommand : public QUndoCommand {
public:
    SetEnumPropertyCommand(core::Document* doc, geom::Entity* entity, std::string key, int newValue,
                           const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    int entityId_;
    std::string key_;
    int oldValue_;
    int newValue_;
};

} // namespace bcad::app
