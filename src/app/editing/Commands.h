#pragma once

#include "bcad/core/Document.h"
#include "bcad/geometry/Types.h"
#include "bcad/properties/PropertyTypes.h"
#include <QUndoCommand>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bcad::geom { class TextEntity; }

namespace bcad::app {

// Ajoute une entité au redo, la retire à l'undo. Prend possession d'une
// entité fraîchement créée pour le premier redo() ; après un undo, redo()
// rajoute un clone du dernier état connu (Document::removeEntity détruit
// l'original), sous le MÊME identifiant, que les commandes suivantes visent.
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

// Image miroir de AddEntityCommand : retire au redo, rajoute un clone à l'undo,
// sous le même identifiant.
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

// Entités déjà ajoutées au document par un import (IFileImporter) : la
// commande ne les crée pas, elle les rend annulables. Le premier redo — celui
// de QUndoStack::push — ne fait rien ; annuler les retire, rétablir les remet
// sous leurs identifiants.
class RecordedAdditionCommand : public QUndoCommand {
public:
    RecordedAdditionCommand(core::Document* doc, std::vector<int> ids, const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    std::vector<int> ids_;
    std::vector<std::unique_ptr<geom::Entity>> snapshots_;
    bool firstRedo_ = true;
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

// Remplace le contenu d'un texte au redo, restaure le précédent à l'undo.
class SetTextCommand : public QUndoCommand {
public:
    SetTextCommand(core::Document* doc, geom::TextEntity* text, std::string newText, const QString& label);

    void redo() override;
    void undo() override;

private:
    void apply(const std::string& value);

    core::Document* doc_;
    int entityId_;
    std::string oldText_;
    std::string newText_;
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

// Modifie une propriété de type string d'une entité (clé déclarée par un plugin,
// ex. « domain.field »).
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

// Modifie une propriété typée d'une entité (via PropertyMap, supporte tous les types).
// Wrappé depuis la commande Core pure C++ via QtCommandAdapter.
class SetEntityPropertyCommand : public QUndoCommand {
public:
    SetEntityPropertyCommand(core::Document* doc, geom::Entity* entity, std::string key,
                              const properties::PropertyValue& newValue, const QString& text);

    void redo() override;
    void undo() override;

private:
    core::Document* doc_;
    int entityId_;
    std::string key_;
    properties::PropertyValue oldValue_;
    properties::PropertyValue newValue_;
};

} // namespace bcad::app
