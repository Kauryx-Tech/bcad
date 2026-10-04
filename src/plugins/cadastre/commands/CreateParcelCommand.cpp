#include "CreateParcelCommand.h"
#include "../entities/ParcelEntity.h"
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace bcad::cadastre {

void CreateParcelCommand::execute(core::Document& doc) {
    if (createdId_ >= 0) return;
    auto e = makeParcel(params_);
    if (!e) return;
    // Retablir rend la parcelle sous son premier id : les commandes suivantes
    // de l'historique (deplacer, proprietes) la retrouvent par cet id.
    if (firstId_ >= 0) e->setId(firstId_);
    auto* added = doc.addEntity(std::move(e));
    if (added) createdId_ = firstId_ = added->id();
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
    // Sans contour, pas de parcelle : l'ancien rectangle fixe « A 001 » pose a
    // l'origine n'etait d'aucun usage reel.
    if (params.empty()) return nullptr;
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

// Deux formes d'arguments :
//   - le contour dessine par l'operateur (WorkbenchParams::PickPolygon) :
//     [x1, y1, ..., xn, yn], n >= 3 — section et numero restent a saisir dans
//     le panneau Proprietes, et la regle d'identification le signale ;
//   - la forme texte complete « x,y;x,y;...|section|numero|... » (scripts, tests).
// Rien d'autre ne cree de parcelle.
std::unique_ptr<commands::Command> makeCreateParcel(const std::vector<std::string>& args) {
    if (args.size() >= 6 && args.size() % 2 == 0) {
        std::string vertices;
        for (size_t i = 0; i < args.size(); i += 2) {
            try {
                const double x = std::stod(args[i]);
                const double y = std::stod(args[i + 1]);
                if (!std::isfinite(x) || !std::isfinite(y)) return nullptr;
            } catch (const std::exception&) {
                return nullptr;
            }
            if (!vertices.empty()) vertices += ';';
            vertices += args[i] + ',' + args[i + 1];
        }
        return std::make_unique<CreateParcelCommand>(vertices + "||");
    }
    if (args.size() == 1 && !args[0].empty()) return std::make_unique<CreateParcelCommand>(args[0]);
    return nullptr;
}

} // namespace bcad::cadastre