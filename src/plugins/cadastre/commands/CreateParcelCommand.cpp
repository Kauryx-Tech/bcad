#include "CreateParcelCommand.h"
#include "../entities/ParcelEntity.h"
#include <sstream>

namespace bcad::cadastre {

void CreateParcelCommand::execute(core::Document& doc) {
    if (createdId_ >= 0) return;
    auto e = makeParcel(params_);
    if (!e) return;
    auto* added = doc.addEntity(std::move(e));
    if (added) createdId_ = added->id();
}

void CreateParcelCommand::undo(core::Document& doc) {
    if (createdId_ >= 0) {
        doc.removeEntity(createdId_);
        createdId_ = -1;
    }
}

std::unique_ptr<commands::Command> CreateParcelCommand::clone() const {
    auto c = std::make_unique<CreateParcelCommand>(params_);
    c->createdId_ = createdId_;
    return c;
}

std::unique_ptr<geom::Entity> CreateParcelCommand::makeParcel(std::string_view params) {
    if (params.empty()) {
        return ParcelEntity::createDefault();
    }
    std::vector<std::string> fields;
    std::string cur;
    for (char c : params) {
        if (c == '|') { fields.push_back(cur); cur.clear(); }
        else cur += c;
    }
    fields.push_back(cur);
    if (fields.size() < 3) return nullptr;

    std::vector<geom::Point2> vertices;
    std::istringstream vtok(fields[0]);
    std::string v;
    while (std::getline(vtok, v, ';')) {
        if (v.empty()) continue;
        std::istringstream ps(v);
        double x = 0, y = 0; char sep = 0;
        if (!(ps >> x >> sep >> y) || sep != ',') return nullptr;
        vertices.emplace_back(x, y);
    }
    auto e = std::make_unique<ParcelEntity>(std::move(vertices));
    auto& props = e->properties();
    if (fields.size() > 1) props.setString("cadastre.section", fields[1]);
    if (fields.size() > 2) props.setString("cadastre.numero", fields[2]);
    if (fields.size() > 3) props.setString("cadastre.contenance", fields[3]);
    if (fields.size() > 4) props.setString("cadastre.commune", fields[4]);
    if (fields.size() > 5) props.setString("cadastre.proprietaire", fields[5]);
    if (fields.size() > 6) props.setEnum("cadastre.nature", std::stoi(fields[6]));
    return e;
}

std::unique_ptr<commands::Command> makeCreateParcel(const std::vector<std::string>& args) {
    std::string params = args.empty() ? "" : args[0];
    return std::make_unique<CreateParcelCommand>(params);
}

} // namespace bcad::cadastre