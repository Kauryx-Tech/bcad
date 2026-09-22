#pragma once

#include "bcad/commands/Command.h"
#include "bcad/commands/ConcreteCommands.h"
#include <QUndoCommand>
#include <QUndoStack>
#include <memory>

namespace bcad::core {
class Document;
}

namespace bcad::app {

// Adaptateur QUndoCommand pour les commandes pures C++
class QtCommandAdapter : public QUndoCommand {
public:
    QtCommandAdapter(bcad::core::Document* doc, std::unique_ptr<bcad::commands::Command> cmd, const QString& text)
        : QUndoCommand(text), doc_(doc), cmd_(std::move(cmd)) {}

    void redo() override { cmd_->execute(*doc_); }
    void undo() override { cmd_->undo(*doc_); }

    std::unique_ptr<bcad::commands::Command> cloneCommand() const {
        // Only clone if the command supports it (has clone() method)
        // For now, return nullptr - proper cloning would need RTTI
        return nullptr;
    }

    bool mergeWith(const QUndoCommand* other) override {
        if (auto* otherAdapter = dynamic_cast<const QtCommandAdapter*>(other)) {
            return cmd_->mergeWith(*otherAdapter->cmd_);
        }
        return false;
    }

private:
    bcad::core::Document* doc_;
    std::unique_ptr<bcad::commands::Command> cmd_;
};

// Pile d'undo/redo qui gère les commandes C++ et Qt
class CommandStack : public QObject {
    Q_OBJECT
public:
    explicit CommandStack(QObject* parent = nullptr) : QObject(parent), undoStack_(new QUndoStack(this)) {}

    void setDocument(bcad::core::Document* doc) { doc_ = doc; }

    void push(std::unique_ptr<bcad::commands::Command> cmd, const QString& text = {}) {
        auto adapter = new QtCommandAdapter(doc_, std::move(cmd), text.isEmpty() ? QString::fromStdString(std::string(cmd->text())) : text);
        undoStack_->push(adapter);
        emit changed();
    }

    void undo() {
        if (undoStack_->canUndo()) undoStack_->undo();
        emit changed();
    }

    void redo() {
        if (undoStack_->canRedo()) undoStack_->redo();
        emit changed();
    }

    bool canUndo() const { return undoStack_->canUndo(); }
    bool canRedo() const { return undoStack_->canRedo(); }

    QString undoText() const { return undoStack_->undoText(); }
    QString redoText() const { return undoStack_->redoText(); }

    QUndoStack* stack() { return undoStack_; }

signals:
    void changed();

private:
    bcad::core::Document* doc_ = nullptr;
    QUndoStack* undoStack_;
};

} // namespace bcad::app