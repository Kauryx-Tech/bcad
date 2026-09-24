#include "bcad/plugin/PluginRegistry.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/TypeId.h"
#include "bcad/core/Document.h"
#include "bcad/commands/Command.h"
#include "bcad/serialization/Serializer.h"
#include "entities/ParcelEntity.h"
#include "entities/BoundaryEntity.h"
#include "entities/SurveyMarkEntity.h"
#include "entities/EasementEntity.h"
#include "entities/SerializerRegistration.h"
#include "commands/CreateParcelCommand.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>

namespace bcad::cadastre {
std::unique_ptr<bcad::commands::Command> makeSplitParcel(
    const std::vector<std::string>& args);
std::unique_ptr<bcad::commands::Command> makeMergeParcels(
    const std::vector<std::string>& args);
std::unique_ptr<bcad::commands::Command> makeEditParcelBoundary(
    const std::vector<std::string>& args);
std::unique_ptr<bcad::commands::Command> makeGeneratePlanSheet(
    const std::vector<std::string>& args);
}

namespace {

using namespace bcad::cadastre;
using namespace bcad::geom;
using namespace bcad::core;
using namespace bcad::commands;
using namespace bcad::serialization;
using namespace bcad::plugin;

// Factories d'entités
std::unique_ptr<Entity> makeParcel(std::string_view params) {
    if (params.empty()) return ParcelEntity::createDefault();
    std::vector<std::string> fields; std::string cur;
    for (char c : params) { if (c == '|') { fields.push_back(cur); cur.clear(); } else cur += c; }
    fields.push_back(cur);
    if (fields.size() < 3) return nullptr;
    std::vector<Point2> vertices;
    std::istringstream vtok(fields[0]); std::string v;
    while (std::getline(vtok, v, ';')) { if (v.empty()) continue; std::istringstream ps(v); double x=0,y=0; char sep=0; if(!(ps>>x>>sep>>y)||sep!=',') return nullptr; vertices.emplace_back(x,y); }
    auto e = std::make_unique<ParcelEntity>(std::move(vertices));
    auto& props = e->properties();
    if (fields.size()>1) props.setString("cadastre.section", fields[1]);
    if (fields.size()>2) props.setString("cadastre.numero", fields[2]);
    if (fields.size()>3) props.setString("cadastre.contenance", fields[3]);
    if (fields.size()>4) props.setString("cadastre.commune", fields[4]);
    if (fields.size()>5) props.setString("cadastre.proprietaire", fields[5]);
    if (fields.size()>6) props.setEnum("cadastre.nature", std::stoi(fields[6]));
    return e;
}

std::unique_ptr<Entity> makeBoundary(std::string_view params) {
    std::vector<std::string> fields; std::string cur;
    for (char c : params) { if (c == '|') { fields.push_back(cur); cur.clear(); } else cur += c; }
    fields.push_back(cur);
    if (fields.size() < 1) return nullptr;
    std::vector<Point2> vertices;
    std::istringstream vtok(fields[0]); std::string v;
    while (std::getline(vtok, v, ';')) { if (v.empty()) continue; std::istringstream ps(v); double x=0,y=0; char sep=0; if(!(ps>>x>>sep>>y)||sep!=',') return nullptr; vertices.emplace_back(x,y); }
    auto e = std::make_unique<BoundaryEntity>(std::move(vertices), false);
    if (fields.size()>1) e->properties().setEnum("cadastre.boundary_type", std::stoi(fields[1]));
    if (fields.size()>2) e->properties().setString("cadastre.reference", fields[2]);
    return e;
}

std::unique_ptr<Entity> makeSurveyMark(std::string_view params) {
    if (params.empty()) return std::make_unique<SurveyMarkEntity>(Point2{0,0});
    std::vector<std::string> fields; std::string cur;
    for (char c : params) { if (c == '|') { fields.push_back(cur); cur.clear(); } else cur += c; }
    fields.push_back(cur);
    if (fields.size() < 3) return nullptr;
    double x = std::stod(fields[0]), y = std::stod(fields[1]);
    auto e = std::make_unique<SurveyMarkEntity>(Point2{x,y});
    if (fields.size()>2) e->properties().setEnum("cadastre.mark_type", std::stoi(fields[2]));
    if (fields.size()>3) e->properties().setString("cadastre.reference", fields[3]);
    if (fields.size()>4) e->properties().setDouble("cadastre.precision", std::stod(fields[4]));
    return e;
}

std::unique_ptr<Entity> makeEasement(std::string_view params) {
    std::vector<std::string> fields; std::string cur;
    for (char c : params) { if (c == '|') { fields.push_back(cur); cur.clear(); } else cur += c; }
    fields.push_back(cur);
    if (fields.size() < 1) return nullptr;
    std::vector<Point2> vertices;
    std::istringstream vtok(fields[0]); std::string v;
    while (std::getline(vtok, v, ';')) { if (v.empty()) continue; std::istringstream ps(v); double x=0,y=0; char sep=0; if(!(ps>>x>>sep>>y)||sep!=',') return nullptr; vertices.emplace_back(x,y); }
    auto e = std::make_unique<EasementEntity>(std::move(vertices));
    if (fields.size()>1) e->properties().setEnum("cadastre.easement_type", std::stoi(fields[1]));
    if (fields.size()>2) e->properties().setString("cadastre.beneficiaire", fields[2]);
    if (fields.size()>3) e->properties().setString("cadastre.reference", fields[3]);
    return e;
}

} // namespace

extern "C" int bcad_plugin_api_version() {
    return bcad::plugin::PLUGIN_API_VERSION;
}

extern "C" bool bcad_plugin_init(PluginRegistry& registry) {
    registry.info().name = "cadastre";
    registry.info().version = "1.0.0";
    registry.info().description = "Module métier cadastre (parcelles, bornes, limites, servitudes)";
    registry.info().author = "bcad";
    
    bool ok = true;
    ok = registry.registerEntityType(TypeId_Parcel, makeParcel) && ok;
    ok = registry.registerEntityType(TypeId_Boundary, makeBoundary) && ok;
    ok = registry.registerEntityType(TypeId_SurveyMark, makeSurveyMark) && ok;
    ok = registry.registerEntityType(TypeId_Easement, makeEasement) && ok;
    
    // Command factory as lambda
    ok = registry.registerCommand("cadastre.create_parcel", bcad::cadastre::makeCreateParcel) && ok;
    ok = registry.registerCommand("cadastre.split_parcel", bcad::cadastre::makeSplitParcel) && ok;
    ok = registry.registerCommand("cadastre.merge_parcels", bcad::cadastre::makeMergeParcels) && ok;
    ok = registry.registerCommand("cadastre.edit_parcel_boundary",
                                  bcad::cadastre::makeEditParcelBoundary) && ok;
    ok = registry.registerCommand("cadastre.generate_plan_sheet",
                                  bcad::cadastre::makeGeneratePlanSheet) && ok;
    
    ok = registerParcelSerializer(registry) && ok;
    ok = registerBoundarySerializer(registry) && ok;
    ok = registerSurveyMarkSerializer(registry) && ok;
    ok = registerEasementSerializer(registry) && ok;
    
    return ok;
}

extern "C" void bcad_plugin_shutdown() {}