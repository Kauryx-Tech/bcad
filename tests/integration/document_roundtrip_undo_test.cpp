#include "bcad/core/Document.h"
#include "bcad/io/Database.h"
#include "bcad/commands/Command.h"
#include "bcad/geometry/Line.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>

using namespace bcad;

namespace {
class AddLineCommand : public bcad::commands::Command {
public:
    AddLineCommand(core::Document& doc, geom::Point2 a, geom::Point2 b)
        : doc_(doc), a_(a), b_(b), id_(-1) {}

    std::string_view text() const override { return "add_line"; }

    void execute(core::Document& doc) override {
        auto line = std::make_unique<geom::LineEntity>(a_, b_);
        id_ = doc_.addEntity(std::move(line))->id();
    }

    void undo(core::Document& doc) override {
        if (id_ >= 0) {
            doc.removeEntity(id_);
            id_ = -1;
        }
    }

    std::unique_ptr<Command> clone() const override {
        return std::make_unique<AddLineCommand>(doc_, a_, b_);
    }

private:
    core::Document& doc_;
    geom::Point2 a_;
    geom::Point2 b_;
    int id_;
};
} // namespace

int main() {
    core::Document doc;

    // 1) Draw
    doc.addEntity(std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(10, 5)));

    // 2) Save
    const auto path = std::filesystem::temp_directory_path() / "bcad_roundtrip_undo_test.bcad";
    std::filesystem::remove(path);
    assert(io::Database::save(path.string(), doc));

    // 3) Load
    core::Document loaded;
    assert(io::Database::load(path.string(), loaded));
    assert(loaded.entities().size() == 1);

    // 4) Undo/Redo
    bcad::commands::CommandStack stack;
    stack.setDocument(loaded);
    stack.push(std::make_unique<AddLineCommand>(loaded, geom::Point2(1, 1), geom::Point2(4, 9)));
    assert(loaded.entities().size() == 2);

    stack.undo();
    assert(loaded.entities().size() == 1);

    stack.redo();
    assert(loaded.entities().size() == 2);

    std::filesystem::remove(path);
    std::cout << "roundtrip + undo/redo OK\n";
    return 0;
}
