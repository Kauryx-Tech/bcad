// Modification annulable en un seul pas (docs/DESIGN_SYSTEM.md, « Code
// commun ») : ajouts, retraits et transformations d'une operation, groupes
// dans une macro de la pile d'annulation — ou appliques directement au
// document quand il n'y a pas de pile (tests, scripts). Un outil n'ecrit plus
// ce double chemin lui-meme.
//
//   EditBatch batch(doc_, undoStack_, tr("Raccord"));
//   batch.add(std::move(arc));
//   batch.remove(original);
//
// La macro se ferme a la destruction. Les objets ajoutes gardent leur calque :
// la regle « un nouvel objet va sur le calque courant » est celle des outils
// de dessin (Viewport::commitEntity), pas celle d'une copie.

#pragma once

#include "editing/Commands.h"

#include "bcad/core/Document.h"
#include "bcad/geometry/Transform2D.h"

#include <QString>
#include <QUndoStack>

#include <memory>

namespace bcad::app {

class EditBatch {
public:
    EditBatch(core::Document* doc, QUndoStack* stack, QString label)
        : doc_(doc), stack_(stack), label_(std::move(label)) {
        if (stack_) stack_->beginMacro(label_);
    }
    ~EditBatch() {
        if (stack_) stack_->endMacro();
    }
    EditBatch(const EditBatch&) = delete;
    EditBatch& operator=(const EditBatch&) = delete;

    void add(std::unique_ptr<geom::Entity> entity) {
        if (stack_) stack_->push(new AddEntityCommand(doc_, std::move(entity), label_));
        else doc_->addEntity(std::move(entity));
    }
    void remove(geom::Entity* entity) {
        if (stack_) stack_->push(new RemoveEntityCommand(doc_, entity, label_));
        else doc_->removeEntity(entity->id());
    }
    void transform(geom::Entity* entity, const geom::Transform2D& transform) {
        if (stack_) {
            stack_->push(new TransformEntityCommand(doc_, entity, transform, label_));
        } else {
            entity->applyTransform(transform);
            doc_->notifyEntityChanged(entity);
        }
    }
    // Remplace `original` par `replacement` (meme operation, un seul pas).
    void replace(geom::Entity* original, std::unique_ptr<geom::Entity> replacement) {
        remove(original);
        add(std::move(replacement));
    }

private:
    core::Document* doc_;
    QUndoStack* stack_;
    QString label_;
};

} // namespace bcad::app
