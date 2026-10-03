#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace bcad::core {
class Document;
}

namespace bcad::commands {

// Interface pure pour une commande annulable/refaisable.
// Pas de dépendance à Qt — l'adaptateur Qt se trouve dans app/QtCommandAdapter.h
class Command {
public:
    virtual ~Command() = default;

    // Texte descriptif pour l'UI (ex: "Move", "Add Line").
    virtual std::string_view text() const = 0;

    // Exécute la commande (redo).
    virtual void execute(core::Document& doc) = 0;

    // Annule la commande (undo).
    virtual void undo(core::Document& doc) = 0;

    // Tente de fusionner avec une autre commande du même type.
    // Retourne true si la fusion a réussi (l'autre commande est absorbée).
    virtual bool mergeWith(const Command& other) { return false; }

    // Crée une copie de la commande pour la fusion/redo.
    virtual std::unique_ptr<Command> clone() const = 0;
};

// Transaction = séquence de commandes atomiques.
// L'ensemble s'exécute ou s'annule entièrement.
class Transaction : public Command {
public:
    Transaction() = default;
    explicit Transaction(std::string text) : text_(std::move(text)) {}

    void add(std::unique_ptr<Command> cmd) {
        commands_.push_back(std::move(cmd));
    }

    std::string_view text() const override { return text_; }

    void execute(core::Document& doc) override {
        for (auto& cmd : commands_) cmd->execute(doc);
    }

    void undo(core::Document& doc) override {
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
            (*it)->undo(doc);
        }
    }

    bool mergeWith(const Command& other) override {
        if (auto* otherTx = dynamic_cast<const Transaction*>(&other)) {
            for (const auto& cmd : otherTx->commands_) {
                commands_.push_back(cmd->clone());
            }
            return true;
        }
        return false;
    }

    std::unique_ptr<Command> clone() const override {
        auto copy = std::make_unique<Transaction>(text_);
        for (const auto& cmd : commands_) {
            copy->commands_.push_back(cmd->clone());
        }
        return copy;
    }

protected:
    std::string text_;
    std::vector<std::unique_ptr<Command>> commands_;
};

// Gestionnaire de pile d'undo/redo pur C++.
    class CommandStack {
    public:
        static constexpr std::size_t kMaxHistory = 100;

        struct LogEntry {
            std::string text;
            std::chrono::system_clock::time_point timestamp;
            bool isUndo;

            LogEntry() = default;
            LogEntry(std::string_view t, std::chrono::system_clock::time_point ts, bool u)
                : text(t), timestamp(ts), isUndo(u) {}
        };

        void push(std::unique_ptr<Command> cmd) {
            // Tronque l'historique de redo
            if (current_ + 1 < history_.size()) {
                history_.resize(current_ + 1);
            }
            
            // Essayer de fusionner avec la commande précédente si possible
            if (!history_.empty()) {
                Command* prev = history_.back().get();
                if (prev->mergeWith(*cmd)) {
                    // Fusion réussie : la commande précédente absorbe la nouvelle
                    cmd->execute(*doc_);
                    log_.push_back(LogEntry{cmd->text(), std::chrono::system_clock::now(), false});
                    return;  // La nouvelle commande est absorbée, pas ajoutée à l'historique
                }
            }
            
            // Pas de fusion possible : ajouter normalement
            cmd->execute(*doc_);
            log_.push_back(LogEntry{cmd->text(), std::chrono::system_clock::now(), false});
            history_.push_back(std::move(cmd));
            ++current_;
            // Élaguer les entrées les plus anciennes si la limite est atteinte.
            if (history_.size() > kMaxHistory) {
                const std::size_t excess = history_.size() - kMaxHistory;
                history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(excess));
                current_ = (current_ >= excess) ? current_ - excess : 0;
            }
        }

        void undo() {
            if (current_ > 0) {
                --current_;
                history_[current_]->undo(*doc_);
                log_.push_back(LogEntry{history_[current_]->text(), std::chrono::system_clock::now(), true});
            }
        }

        void redo() {
            if (current_ < history_.size()) {
                history_[current_]->execute(*doc_);
                log_.push_back(LogEntry{history_[current_]->text(), std::chrono::system_clock::now(), false});
                ++current_;
            }
        }

        bool canUndo() const { return current_ > 0; }
        bool canRedo() const { return current_ < history_.size(); }

        std::string_view undoText() const {
            return canUndo() ? history_[current_ - 1]->text() : "";
        }
        std::string_view redoText() const {
            return canRedo() ? history_[current_]->text() : "";
        }

        // Transaction log access
        const std::vector<LogEntry>& log() const { return log_; }
        void clearLog() { log_.clear(); }

        void setDocument(core::Document& doc) { doc_ = &doc; }

    private:
        core::Document* doc_ = nullptr;
        std::vector<std::unique_ptr<Command>> history_;
        std::vector<LogEntry> log_;
        std::size_t current_ = 0;
    };

} // namespace bcad::commands