#include "bcad/io/Database.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/serialization/Serializer.h"
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace bcad::io {

using namespace geom;
using namespace core;

namespace {

constexpr int kCurrentSchemaVersion = 1;

struct SqliteHandle {
    sqlite3* db = nullptr;
    ~SqliteHandle() { if (db) sqlite3_close(db); }
};

struct StmtHandle {
    sqlite3_stmt* stmt = nullptr;
    ~StmtHandle() { if (stmt) sqlite3_finalize(stmt); }
};

void exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

// Helper local pour détecter une parcelle cadastre sans dépendre du module cadastre
// Vérifie : typeId == "cadastre.parcel" OU polyline fermée avec props cadastre.*
inline bool isCadastreParcelLocal(const geom::Entity* e) {
    if (!e) return false;
    if (e->typeId().value == "cadastre.parcel") return true;
    if (e->typeId() != geom::TypeId_Polyline) return false;
    const auto* poly = static_cast<const geom::PolylineEntity*>(e);
    return poly->closed() && poly->properties().has("cadastre.section");
}

std::string serializeParams(const Entity& e) {
    const auto* serializer = serialization::SerializerRegistry::find(e.typeId());
    if (!serializer) return {};
    return serializer->serialize(e);
}

// Pour les polylignes enrichies F1 (typeId encore bcad.Polyline) : le
// serializer natif polyline ne voit pas les props cadastre ; on délègue
// au serializer cadastre quand l'entité est reconnue comme parcelle.
std::string serializeParamsOrCadastre(const Entity& e) {
    if (isCadastreParcelLocal(&e) &&
        serialization::SerializerRegistry::contains(geom::TypeId{"cadastre.parcel"})) {
        return serialization::SerializerRegistry::find(geom::TypeId{"cadastre.parcel"})->serialize(e);
    }
    return serializeParams(e);
}

std::vector<double> parseCsvDoubles(const std::string& s) {
    std::vector<double> out;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) out.push_back(std::stod(token));
    return out;
}

std::unique_ptr<Entity> deserializeEntity(TypeId typeId, const std::string& params) {
    const auto* serializer = serialization::SerializerRegistry::find(typeId);
    if (!serializer) return nullptr;
    return serializer->deserialize(params);
}

// Map un TypeId vers l'entier historique de la colonne `type` (format de
// fichier legacy, voir le switch inverse dans load()). Retourne nullopt pour
// un type sans equivalent natif historique (impossible a persister tel quel).
std::optional<int> legacyTypeInt(std::string_view typeId) {
    if (typeId == TypeId_Point.value) return 0;
    if (typeId == TypeId_Line.value) return 1;
    if (typeId == TypeId_Circle.value) return 2;
    if (typeId == TypeId_Arc.value) return 3;
    if (typeId == TypeId_Polyline.value) return 4;
    if (typeId == TypeId_Text.value) return 6;
    if (typeId == "cadastre.parcel") return 5; // type cadastre
    return std::nullopt;
}

// Idem legacyTypeInt, mais reconnaît aussi les polylignes enrichies F1
// (typeId reste bcad.Polyline tant qu'elles n'ont pas été promues).
std::optional<int> legacyTypeFor(const Entity& e) {
    if (auto t = legacyTypeInt(e.typeId().value)) return t;
    if (isCadastreParcelLocal(&e)) return 5;
    return std::nullopt;
}

} // namespace

bool Database::save(const std::string& path, const Document& doc) {
    std::remove(path.c_str());

    SqliteHandle h;
    if (sqlite3_open(path.c_str(), &h.db) != SQLITE_OK) return false;

    try {
        exec(h.db, "PRAGMA journal_mode=WAL;");
        exec(h.db, "PRAGMA user_version = 1;");
        exec(h.db,
             "CREATE TABLE layers ("
             " name TEXT PRIMARY KEY, color_r REAL, color_g REAL, color_b REAL,"
             " line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER);");
        exec(h.db,
             "CREATE TABLE entities ("
             " id INTEGER PRIMARY KEY, type INTEGER, layer TEXT,"
             " has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL, params TEXT);");
        exec(h.db,
             "CREATE TABLE cadastre_parcels ("
             " entity_id INTEGER PRIMARY KEY,"
             " section TEXT, numero TEXT, contenance TEXT, commune TEXT,"
             " proprietaire TEXT, nature TEXT,"
             " FOREIGN KEY(entity_id) REFERENCES entities(id));");

        exec(h.db, "BEGIN TRANSACTION;");

        {
            const char* sql = "INSERT INTO layers VALUES (?,?,?,?,?,?,?,?);";
            StmtHandle st;
            sqlite3_prepare_v2(h.db, sql, -1, &st.stmt, nullptr);
            for (const auto& layer : doc.layerManager().layers()) {
                sqlite3_reset(st.stmt);
                sqlite3_bind_text(st.stmt, 1, layer.name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(st.stmt, 2, layer.color.r);
                sqlite3_bind_double(st.stmt, 3, layer.color.g);
                sqlite3_bind_double(st.stmt, 4, layer.color.b);
                sqlite3_bind_double(st.stmt, 5, layer.lineWeight);
                sqlite3_bind_int(st.stmt, 6, layer.visible ? 1 : 0);
                sqlite3_bind_int(st.stmt, 7, layer.locked ? 1 : 0);
                sqlite3_bind_int(st.stmt, 8, static_cast<int>(layer.lineType));
                if (sqlite3_step(st.stmt) != SQLITE_DONE) throw std::runtime_error("insert layer failed");
            }
        }
        {
            const char* sql = "INSERT INTO entities VALUES (?,?,?,?,?,?,?,?);";
            StmtHandle st;
            sqlite3_prepare_v2(h.db, sql, -1, &st.stmt, nullptr);
            for (const auto& e : doc.entities()) {
                std::optional<int> legacyType = legacyTypeFor(*e);
                if (!legacyType) continue; // type externe sans representation legacy

                sqlite3_reset(st.stmt);
                sqlite3_bind_int(st.stmt, 1, e->id());
                sqlite3_bind_int(st.stmt, 2, *legacyType);
                sqlite3_bind_text(st.stmt, 3, e->layer().c_str(), -1, SQLITE_TRANSIENT);
                bool hasOverride = e->colorOverride().has_value();
                sqlite3_bind_int(st.stmt, 4, hasOverride ? 1 : 0);
                Color c = hasOverride ? *e->colorOverride() : Color{};
                sqlite3_bind_double(st.stmt, 5, c.r);
                sqlite3_bind_double(st.stmt, 6, c.g);
                sqlite3_bind_double(st.stmt, 7, c.b);
                std::string params = serializeParamsOrCadastre(*e);
                sqlite3_bind_text(st.stmt, 8, params.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(st.stmt) != SQLITE_DONE) throw std::runtime_error("insert entity failed");

// Si c'est une parcelle cadastre, inserer les donnees supplementaires
            if (isCadastreParcelLocal(e.get())) {
                    const auto& props = e->properties();
                    const char* sqlCad = "INSERT INTO cadastre_parcels VALUES (?,?,?,?,?,?,?);";
                    StmtHandle stCad;
                    sqlite3_prepare_v2(h.db, sqlCad, -1, &stCad.stmt, nullptr);
                    sqlite3_bind_int(stCad.stmt, 1, e->id());
                    sqlite3_bind_text(stCad.stmt, 2, props.getString("cadastre.section").c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stCad.stmt, 3, props.getString("cadastre.numero").c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stCad.stmt, 4, props.getString("cadastre.contenance").c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stCad.stmt, 5, props.getString("cadastre.commune").c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stCad.stmt, 6, props.getString("cadastre.proprietaire").c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stCad.stmt, 7, props.getString("cadastre.nature").c_str(), -1, SQLITE_TRANSIENT);
                    if (sqlite3_step(stCad.stmt) != SQLITE_DONE) throw std::runtime_error("insert cadastre parcel failed");
                }
            }
        }

        exec(h.db, "COMMIT;");
    } catch (const std::exception&) {
        exec(h.db, "ROLLBACK;");
        return false;
    }
    return true;
}

bool Database::load(const std::string& path, Document& outDoc) {
    SqliteHandle h;
    if (sqlite3_open_v2(path.c_str(), &h.db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        return false;
    }

    StmtHandle version;
    if (sqlite3_prepare_v2(h.db, "PRAGMA user_version;", -1, &version.stmt, nullptr) != SQLITE_OK ||
        sqlite3_step(version.stmt) != SQLITE_ROW ||
        sqlite3_column_int(version.stmt, 0) > kCurrentSchemaVersion) {
        return false;
    }

    outDoc.clear();
    outDoc.layerManager().reset();
    Document& doc = outDoc;

    {
        const char* sql = "SELECT name,color_r,color_g,color_b,line_weight,visible,locked,line_type FROM layers;";
        StmtHandle st;
        if (sqlite3_prepare_v2(h.db, sql, -1, &st.stmt, nullptr) != SQLITE_OK) return false;
        bool first = true;
        while (sqlite3_step(st.stmt) == SQLITE_ROW) {
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(st.stmt, 0));
            Color color{ static_cast<float>(sqlite3_column_double(st.stmt, 1)),
                         static_cast<float>(sqlite3_column_double(st.stmt, 2)),
                         static_cast<float>(sqlite3_column_double(st.stmt, 3)) };
            if (!first || name != "0") {
                doc.layerManager().createLayer(name, color);
            }
            layers::Layer* layer = doc.layerManager().find(name);
            if (layer) {
                layer->color = color;
                layer->lineWeight = sqlite3_column_double(st.stmt, 4);
                layer->visible = sqlite3_column_int(st.stmt, 5) != 0;
                layer->locked = sqlite3_column_int(st.stmt, 6) != 0;
                layer->lineType = static_cast<layers::Layer::LineType>(sqlite3_column_int(st.stmt, 7));
            }
            first = false;
        }
    }

    {
        const char* sql = "SELECT id,type,layer,has_color_override,color_r,color_g,color_b,params FROM entities;";
        StmtHandle st;
        if (sqlite3_prepare_v2(h.db, sql, -1, &st.stmt, nullptr) != SQLITE_OK) return false;
        int maxId = 0;
        std::vector<int> cadastreIds;
        while (sqlite3_step(st.stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(st.stmt, 0);
            int typeInt = sqlite3_column_int(st.stmt, 1);
            std::string layer = reinterpret_cast<const char*>(sqlite3_column_text(st.stmt, 2));
            bool hasOverride = sqlite3_column_int(st.stmt, 3) != 0;
            std::string params = reinterpret_cast<const char*>(sqlite3_column_text(st.stmt, 7));

            // Convert legacy EntityType int to TypeId
            TypeId typeId;
            switch (static_cast<EntityType>(typeInt)) {
                case EntityType::Point: typeId = TypeId_Point; break;
                case EntityType::Line: typeId = TypeId_Line; break;
                case EntityType::Circle: typeId = TypeId_Circle; break;
                case EntityType::Arc: typeId = TypeId_Arc; break;
                case EntityType::Polyline: typeId = TypeId_Polyline; break;
                case EntityType(5): typeId = TypeId{"cadastre.parcel"}; break;
                case EntityType::Text: typeId = TypeId_Text; break;
            }

            auto entity = deserializeEntity(typeId, params);
            if (!entity) continue;
            entity->setLayer(layer);
            if (hasOverride) {
                Color c{ static_cast<float>(sqlite3_column_double(st.stmt, 4)),
                         static_cast<float>(sqlite3_column_double(st.stmt, 5)),
                         static_cast<float>(sqlite3_column_double(st.stmt, 6)) };
                entity->setColorOverride(c);
            }
            doc.addEntity(std::move(entity));
            if (typeInt == 5 || isCadastreParcelLocal(doc.entities().back().get()))
                cadastreIds.push_back(id);
            maxId = std::max(maxId, id);
        }
        (void)maxId; // Document réattribue des identifiants séquentiels à l'insertion ; les identifiants d'origine ne sont pas conservés.

        // Charger les donnees cadastre pour les parcelles
        if (!cadastreIds.empty()) {
            std::string placeholders;
            for (size_t i = 0; i < cadastreIds.size(); ++i) {
                if (i > 0) placeholders += ",";
                placeholders += "?";
            }
            std::string sqlCad = "SELECT entity_id, section, numero, contenance, commune, proprietaire, nature FROM cadastre_parcels WHERE entity_id IN (" + placeholders + ");";
            StmtHandle stCad;
            if (sqlite3_prepare_v2(h.db, sqlCad.c_str(), -1, &stCad.stmt, nullptr) == SQLITE_OK) {
                for (size_t i = 0; i < cadastreIds.size(); ++i) {
                    sqlite3_bind_int(stCad.stmt, static_cast<int>(i) + 1, cadastreIds[i]);
                }
                while (sqlite3_step(stCad.stmt) == SQLITE_ROW) {
                    int entityId = sqlite3_column_int(stCad.stmt, 0);
                    std::string section = reinterpret_cast<const char*>(sqlite3_column_text(stCad.stmt, 1));
                    std::string numero = reinterpret_cast<const char*>(sqlite3_column_text(stCad.stmt, 2));
                    std::string contenance = reinterpret_cast<const char*>(sqlite3_column_text(stCad.stmt, 3));
                    std::string commune = reinterpret_cast<const char*>(sqlite3_column_text(stCad.stmt, 4));
                    std::string proprietaire = reinterpret_cast<const char*>(sqlite3_column_text(stCad.stmt, 5));
                    std::string nature = reinterpret_cast<const char*>(sqlite3_column_text(stCad.stmt, 6));

                    // Trouver l'entité correspondante dans le document
                    for (auto& e : doc.entities()) {
                        if (e->id() == entityId && isCadastreParcelLocal(e.get())) {
                            auto& props = e->properties();
                            props.setString("cadastre.section", section);
                            props.setString("cadastre.numero", numero);
                            props.setString("cadastre.contenance", contenance);
                            props.setString("cadastre.commune", commune);
                            props.setString("cadastre.proprietaire", proprietaire);
                            props.setString("cadastre.nature", nature);
                            break;
                        }
                    }
                }
            }
        }
    }

    return true;
}

} // namespace bcad::io
