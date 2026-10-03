#include "Commands.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"

namespace bcad::app {

AddEntityCommand::AddEntityCommand(core::Document* doc, std::unique_ptr<geom::Entity> entity, const QString& text)
    : QUndoCommand(text), doc_(doc), pending_(std::move(entity)) {}

// Retablir rend l'entite SOUS SON IDENTIFIANT : les commandes posterieures de
// la pile (deplacer, calque, propriete) la retrouvent par cet id. Avec un id
// neuf, Retablir une transformation apres Annuler ne trouvait plus rien.
void AddEntityCommand::redo() {
    std::unique_ptr<geom::Entity> toAdd = pending_ ? std::move(pending_) : snapshot_->clone();
    if (entityId_ >= 0) toAdd->setId(entityId_);
    geom::Entity* raw = doc_->addEntity(std::move(toAdd));
    entityId_ = raw->id();
}

void AddEntityCommand::undo() {
    geom::Entity* e = doc_->findEntity(entityId_);
    if (e) snapshot_ = e->clone();
    doc_->removeEntity(entityId_);
}

RemoveEntityCommand::RemoveEntityCommand(core::Document* doc, geom::Entity* entity, const QString& text)
    : QUndoCommand(text), doc_(doc), snapshot_(entity->clone()), entityId_(entity->id()) {}

void RemoveEntityCommand::redo() {
    geom::Entity* e = doc_->findEntity(entityId_);
    if (e) snapshot_ = e->clone();
    doc_->removeEntity(entityId_);
}

void RemoveEntityCommand::undo() {
    auto restored = snapshot_->clone();
    restored->setId(entityId_);
    geom::Entity* raw = doc_->addEntity(std::move(restored));
    entityId_ = raw->id();
}

TransformEntityCommand::TransformEntityCommand(core::Document* doc, geom::Entity* entity,
                                                const geom::AffTransform2& transform, const QString& text)
    : QUndoCommand(text), doc_(doc), entityId_(entity->id()), transform_(transform), inverse_(transform.inverse()) {}

void TransformEntityCommand::redo() { apply(transform_); }
void TransformEntityCommand::undo() { apply(inverse_); }

void TransformEntityCommand::apply(const geom::AffTransform2& t) {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->applyTransform(t);
        doc_->notifyEntityChanged(e);
    }
}

SetLayerCommand::SetLayerCommand(core::Document* doc, geom::Entity* entity, std::string newLayer, const QString& text)
    : QUndoCommand(text), doc_(doc), entityId_(entity->id()), oldLayer_(entity->layer()),
      newLayer_(std::move(newLayer)) {}

void SetLayerCommand::redo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->setLayer(newLayer_);
        doc_->notifyEntityChanged(e);
    }
}

void SetLayerCommand::undo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->setLayer(oldLayer_);
        doc_->notifyEntityChanged(e);
    }
}

SetColorOverrideCommand::SetColorOverrideCommand(core::Document* doc, geom::Entity* entity,
                                                   std::optional<geom::Color> newColor, const QString& text)
    : QUndoCommand(text), doc_(doc), entityId_(entity->id()), oldColor_(entity->colorOverride()),
      newColor_(newColor) {}

void SetColorOverrideCommand::redo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->setColorOverride(newColor_);
        doc_->notifyEntityChanged(e);
    }
}

void SetColorOverrideCommand::undo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->setColorOverride(oldColor_);
        doc_->notifyEntityChanged(e);
    }
}

SetPropertyCommand::SetPropertyCommand(core::Document* doc, geom::Entity* entity, std::string key, std::string newValue,
                                        const QString& text)
    : QUndoCommand(text), doc_(doc), entityId_(entity->id()), key_(std::move(key)),
      newValue_(std::move(newValue)), oldValue_(entity->properties().getString(key_)) {}

void SetPropertyCommand::redo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->properties().setString(key_, newValue_);
        doc_->notifyEntityChanged(e);
    }
}

void SetPropertyCommand::undo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->properties().setString(key_, oldValue_);
        doc_->notifyEntityChanged(e);
    }
}

SetEnumPropertyCommand::SetEnumPropertyCommand(core::Document* doc, geom::Entity* entity, std::string key,
                                               int newValue, const QString& text)
    : QUndoCommand(text), doc_(doc), entityId_(entity->id()), key_(std::move(key)),
      oldValue_(entity->properties().getEnum(key_)), newValue_(newValue) {}

void SetEnumPropertyCommand::redo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->properties().setEnum(key_, newValue_);
        doc_->notifyEntityChanged(e);
    }
}

void SetEnumPropertyCommand::undo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->properties().setEnum(key_, oldValue_);
        doc_->notifyEntityChanged(e);
    }
}

SetEntityPropertyCommand::SetEntityPropertyCommand(core::Document* doc, geom::Entity* entity,
                                                    std::string key,
                                                    const properties::PropertyValue& newValue,
                                                    const QString& text)
    : QUndoCommand(text), doc_(doc), entityId_(entity->id()), key_(std::move(key)),
      oldValue_(entity->properties().getPropertyValue(key_)), newValue_(newValue) {}

void SetEntityPropertyCommand::redo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->properties().set(key_, newValue_);
        doc_->notifyEntityChanged(e);
    }
}

void SetEntityPropertyCommand::undo() {
    if (geom::Entity* e = doc_->findEntity(entityId_)) {
        e->properties().set(key_, oldValue_);
        doc_->notifyEntityChanged(e);
    }
}

} // namespace bcad::app
