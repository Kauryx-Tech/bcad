#pragma once

#include "bcad/commands/Command.h"
#include "bcad/core/Document.h"
#include <memory>
#include <string>
#include <vector>

namespace bcad::cadastre {

class CreateParcelCommand : public commands::Command {
public:
    explicit CreateParcelCommand(std::string params = "") : params_(std::move(params)) {}

    std::string_view text() const override { return "cadastre.create_parcel"; }

    void execute(core::Document& doc) override;
    void undo(core::Document& doc) override;
    std::unique_ptr<commands::Command> clone() const override;

private:
    std::string params_;
    int createdId_ = -1;

    static std::unique_ptr<geom::Entity> makeParcel(std::string_view params);
};

std::unique_ptr<commands::Command> makeCreateParcel(const std::vector<std::string>& args);

} // namespace bcad::cadastre