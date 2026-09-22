#include "bcad/io/Database.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
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

std::string serializeParams(const Entity& e) {
    const auto* serializer = serialization::SerializerRegistry::find(e.typeId());
    if (!serializer) return {};
    return serializer->serialize(e);
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
    return std::nullopt;
}

} // namespace

bool Database::save(const std::string& path, const Document& doc) {
    std::remove(path.c_str());

    SqliteHandle h;
    if (sqlite3_open(path.c_str(), &h.db) != SQLITE_OK) return false;

    try {
        exec(h.db, "PRAGMA journal_mode=WAL;");
        exec(h.db,
             "CREATE TABLE layers ("
             " name TEXT PRIMARY KEY, color_r REAL, color_g REAL, color_b REAL,"
             " line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER);");
        exec(h.db,
             "CREATE TABLE entities ("
             " id INTEGER PRIMARY KEY, type INTEGER, layer TEXT,"
             " has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL, params TEXT);");

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
                std::optional<int> legacyType = legacyTypeInt(e->typeId().value);
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
                std::string params = serializeParams(*e);
                sqlite3_bind_text(st.stmt, 8, params.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(st.stmt) != SQLITE_DONE) throw std::runtime_error("insert entity failed");
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
            maxId = std::max(maxId, id);
        }
        (void)maxId; // Document réattribue des identifiants séquentiels à l'insertion ; les identifiants d'origine ne sont pas conservés.
    }

    return true;
}

} // namespace bcad::io
