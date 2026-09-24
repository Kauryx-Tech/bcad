#include "GeoPackageSerializer.h"
#include "../entities/ParcelEntity.h"
#include "bcad/geometry/Polyline.h"

#include <sqlite3.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace bcad::cadastre {

namespace {

class Database {
public:
    ~Database() { if (db_) sqlite3_close(db_); }
    bool open(const std::string& path, int flags) {
        return sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) == SQLITE_OK;
    }
    bool exec(const char* sql) {
        char* error = nullptr;
        const bool ok = sqlite3_exec(db_, sql, nullptr, nullptr, &error) == SQLITE_OK;
        sqlite3_free(error);
        return ok;
    }
    sqlite3* get() const { return db_; }
private:
    sqlite3* db_ = nullptr;
};

void append32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}

void append64(std::vector<std::uint8_t>& out, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
}

std::vector<std::uint8_t> polygonWkb(const geom::PolylineEntity& polygon) {
    // GeoPackageBinary header: magic, version, flags (little endian, no
    // envelope), and undefined SRS (-1).
    std::vector<std::uint8_t> wkb{'G', 'P', 0, 1};
    append32(wkb, static_cast<std::uint32_t>(-1));
    wkb.push_back(1);
    append32(wkb, 3);
    append32(wkb, 1);
    append32(wkb, static_cast<std::uint32_t>(polygon.vertices().size() + 1));
    for (const auto& point : polygon.vertices()) {
        append64(wkb, point.x_);
        append64(wkb, point.y_);
    }
    const auto& first = polygon.vertices().front();
    append64(wkb, first.x_);
    append64(wkb, first.y_);
    return wkb;
}

bool readDouble(const std::uint8_t*& cursor, const std::uint8_t* end, double& value) {
    if (end - cursor < 8) return false;
    std::uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) bits |= static_cast<std::uint64_t>(*cursor++) << (i * 8);
    std::memcpy(&value, &bits, sizeof value);
    return true;
}

bool read32(const std::uint8_t*& cursor, const std::uint8_t* end, std::uint32_t& value) {
    if (end - cursor < 4) return false;
    value = 0;
    for (int i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(*cursor++) << (i * 8);
    return true;
}

std::unique_ptr<ParcelEntity> parcelFromWkb(const void* data, int size) {
    if (!data || size < 21) return nullptr;
    const auto* cursor = static_cast<const std::uint8_t*>(data);
    const auto* end = cursor + size;
    if (cursor[0] != 'G' || cursor[1] != 'P' || cursor[2] != 0
        || (cursor[3] & 0x01) == 0) return nullptr;
    cursor += 8;
    if (*cursor++ != 1) return nullptr;
    std::uint32_t type = 0, rings = 0, count = 0;
    if (!read32(cursor, end, type) || type != 3 || !read32(cursor, end, rings)
        || rings != 1 || !read32(cursor, end, count) || count < 4) return nullptr;
    std::vector<geom::Point2> points;
    points.reserve(count - 1);
    for (std::uint32_t i = 0; i < count; ++i) {
        double x = 0, y = 0;
        if (!readDouble(cursor, end, x) || !readDouble(cursor, end, y)) return nullptr;
        if (i + 1 < count) points.emplace_back(x, y);
    }
    return std::make_unique<ParcelEntity>(std::move(points));
}

}

bool GeoPackageSerializer::write(const std::string& path, const core::Document& document) const {
    Database database;
    if (!database.open(path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)) return false;
    if (!database.exec("PRAGMA application_id=1196444487; PRAGMA user_version=1;"
                       "CREATE TABLE IF NOT EXISTS gpkg_contents ("
                       "table_name TEXT PRIMARY KEY,data_type TEXT,identifier TEXT,"
                       "description TEXT,last_change TEXT,min_x REAL,min_y REAL,max_x REAL,max_y REAL,"
                       "srs_id INTEGER);"
                       "CREATE TABLE IF NOT EXISTS gpkg_geometry_columns ("
                       "table_name TEXT,column_name TEXT,geometry_type_name TEXT,"
                       "srs_id INTEGER,z TINYINT,m TINYINT,PRIMARY KEY(table_name,column_name));"
                       "CREATE TABLE IF NOT EXISTS cadastre_parcels ("
                       "id INTEGER PRIMARY KEY,geom BLOB NOT NULL,section TEXT,numero TEXT,"
                       "contenance TEXT,commune TEXT,proprietaire TEXT,nature INTEGER);"
                       "DELETE FROM cadastre_parcels;")) return false;

    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT INTO cadastre_parcels "
        "(id,geom,section,numero,contenance,commune,proprietaire,nature) VALUES (?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(database.get(), sql, -1, &statement, nullptr) != SQLITE_OK) return false;
    for (const auto& entity : document.entities()) {
        const auto* parcel = dynamic_cast<const geom::PolylineEntity*>(entity.get());
        if (!parcel || !parcel->closed() || parcel->vertices().size() < 3) continue;
        const auto blob = polygonWkb(*parcel);
        sqlite3_bind_int(statement, 1, entity->id());
        sqlite3_bind_blob(statement, 2, blob.data(), static_cast<int>(blob.size()), SQLITE_TRANSIENT);
        const auto& props = entity->properties();
        sqlite3_bind_text(statement, 3, props.getString("cadastre.section").c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, props.getString("cadastre.numero").c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 5, props.getString("cadastre.contenance").c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 6, props.getString("cadastre.commune").c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 7, props.getString("cadastre.proprietaire").c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(statement, 8, props.getEnum("cadastre.nature"));
        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            return false;
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
    return database.exec("INSERT OR REPLACE INTO gpkg_contents "
                         "(table_name,data_type,identifier,description,srs_id) VALUES "
                         "('cadastre_parcels','features','cadastre_parcels','BCAD cadastral parcels',0);"
                         "INSERT OR REPLACE INTO gpkg_geometry_columns "
                         "(table_name,column_name,geometry_type_name,srs_id,z,m) VALUES "
                         "('cadastre_parcels','geom','POLYGON',0,0,0);");
}

bool GeoPackageSerializer::read(const std::string& path, core::Document& document) const {
    Database database;
    if (!database.open(path, SQLITE_OPEN_READONLY)) return false;
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database.get(),
            "SELECT geom,section,numero,contenance,commune,proprietaire,nature "
            "FROM cadastre_parcels ORDER BY id", -1, &statement, nullptr) != SQLITE_OK) return false;
    document.clear();
    while (sqlite3_step(statement) == SQLITE_ROW) {
        auto parcel = parcelFromWkb(sqlite3_column_blob(statement, 0),
                                    sqlite3_column_bytes(statement, 0));
        if (!parcel) {
            sqlite3_finalize(statement);
            return false;
        }
        auto& props = parcel->properties();
        props.setString("cadastre.section", reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)));
        props.setString("cadastre.numero", reinterpret_cast<const char*>(sqlite3_column_text(statement, 2)));
        props.setString("cadastre.contenance", reinterpret_cast<const char*>(sqlite3_column_text(statement, 3)));
        props.setString("cadastre.commune", reinterpret_cast<const char*>(sqlite3_column_text(statement, 4)));
        props.setString("cadastre.proprietaire", reinterpret_cast<const char*>(sqlite3_column_text(statement, 5)));
        props.setEnum("cadastre.nature", sqlite3_column_int(statement, 6));
        document.addEntity(std::move(parcel));
    }
    const bool ok = sqlite3_errcode(database.get()) == SQLITE_OK;
    sqlite3_finalize(statement);
    return ok;
}

} // namespace bcad::cadastre
