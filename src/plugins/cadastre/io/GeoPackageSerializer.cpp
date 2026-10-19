#include "GeoPackageSerializer.h"
#include "../commands/CrsCommand.h"
#include "../crs/SystemesCoordonnees.h"
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

bool insertSrs(Database& database, const char* name, int id, const char* organization,
               int organizationId, const std::string& definition, const char* description) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database.get(),
            "INSERT OR REPLACE INTO gpkg_spatial_ref_sys "
            "(srs_name,srs_id,organization,organization_coordsys_id,definition,description) "
            "VALUES (?,?,?,?,?,?)", -1, &statement, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, id);
    sqlite3_bind_text(statement, 3, organization, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, organizationId);
    sqlite3_bind_text(statement, 5, definition.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, description, -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return ok;
}

// WGS 84 geodetique : ligne obligatoire de tout GeoPackage (srs_id 4326).
constexpr const char* kWgs84Wkt =
    "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],"
    "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]";

void append32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}

void append64(std::vector<std::uint8_t>& out, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
}

std::vector<std::uint8_t> polygonWkb(const geom::PolylineEntity& polygon, int srsId) {
    // GeoPackageBinary header: magic, version, flags (little endian, no
    // envelope), and the SRS of the dossier (-1 quand il n'est pas fixe).
    std::vector<std::uint8_t> wkb{'G', 'P', 0, 1};
    append32(wkb, static_cast<std::uint32_t>(srsId));
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
                       "CREATE TABLE IF NOT EXISTS gpkg_spatial_ref_sys ("
                       "srs_name TEXT NOT NULL,srs_id INTEGER NOT NULL PRIMARY KEY,"
                       "organization TEXT NOT NULL,organization_coordsys_id INTEGER NOT NULL,"
                       "definition TEXT NOT NULL,description TEXT);"
                       "DELETE FROM gpkg_spatial_ref_sys;"
                       "CREATE TABLE IF NOT EXISTS cadastre_parcels ("
                       "id INTEGER PRIMARY KEY,geom BLOB NOT NULL,section TEXT,numero TEXT,"
                       "contenance TEXT,commune TEXT,proprietaire TEXT,nature INTEGER);"
                       "DELETE FROM cadastre_parcels;")) return false;

    // Les trois lignes qu'exige la specification, puis le systeme du dossier
    // quand l'operateur l'a fixe (K-06) : sans lui, les coordonnees projetees
    // seraient lues comme un repere local muet.
    if (!insertSrs(database, "Undefined Cartesian SRS", -1, "NONE", -1, "undefined",
                   "undefined cartesian coordinate reference system")
        || !insertSrs(database, "Undefined geographic SRS", 0, "NONE", 0, "undefined",
                      "undefined geographic coordinate reference system")
        || !insertSrs(database, "WGS 84 geodetic", 4326, "EPSG", 4326, kWgs84Wkt,
                      "longitude/latitude coordinates in decimal degrees on the WGS 84 spheroid"))
        return false;

    const int epsg = document.properties().getInt(kCleEpsgDossier);
    const std::string cartouche = document.properties().getString(kCleSystemeCoordonnees);
    const int srsId = epsg > 0 ? epsg : -1;
    if (epsg > 0) {
        const auto connus = systemesCoordonneesInstalles();
        const auto* systeme = trouverSysteme(epsg, connus);
        const std::string definition =
            systeme && !systeme->wkt.empty() ? systeme->wkt : "undefined";
        const std::string nom = systeme ? systeme->nom : cartouche;
        if (!insertSrs(database, nom.c_str(), epsg, "EPSG", epsg, definition,
                       cartouche.c_str())) return false;
    }

    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT INTO cadastre_parcels "
        "(id,geom,section,numero,contenance,commune,proprietaire,nature) VALUES (?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(database.get(), sql, -1, &statement, nullptr) != SQLITE_OK) return false;
    for (const auto& entity : document.entities()) {
        const auto* parcel = dynamic_cast<const geom::PolylineEntity*>(entity.get());
        if (!parcel || !parcel->closed() || parcel->vertices().size() < 3) continue;
        const auto blob = polygonWkb(*parcel, srsId);
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
    const std::string srs = std::to_string(srsId);
    return database.exec(("INSERT OR REPLACE INTO gpkg_contents "
                          "(table_name,data_type,identifier,description,srs_id) VALUES "
                          "('cadastre_parcels','features','cadastre_parcels','BCAD cadastral parcels',"
                          + srs + ");"
                          "INSERT OR REPLACE INTO gpkg_geometry_columns "
                          "(table_name,column_name,geometry_type_name,srs_id,z,m) VALUES "
                          "('cadastre_parcels','geom','POLYGON'," + srs + ",0,0);").c_str());
}

bool GeoPackageSerializer::read(const std::string& path, core::Document& document) const {
    Database database;
    if (!database.open(path, SQLITE_OPEN_READONLY)) return false;
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database.get(),
            "SELECT geom,section,numero,contenance,commune,proprietaire,nature "
            "FROM cadastre_parcels ORDER BY id", -1, &statement, nullptr) != SQLITE_OK) return false;
    document.clear();
    int step = sqlite3_step(statement);
    while (step == SQLITE_ROW) {
        auto parcel = parcelFromWkb(sqlite3_column_blob(statement, 0),
                                    sqlite3_column_bytes(statement, 0));
        if (!parcel) {
            sqlite3_finalize(statement);
            return false;
        }
        auto& props = parcel->properties();
        auto text = [&](int column) {
            const auto* value = sqlite3_column_text(statement, column);
            return value ? reinterpret_cast<const char*>(value) : std::string();
        };
        props.setString("cadastre.section", text(1));
        props.setString("cadastre.numero", text(2));
        props.setString("cadastre.contenance", text(3));
        props.setString("cadastre.commune", text(4));
        props.setString("cadastre.proprietaire", text(5));
        props.setEnum("cadastre.nature", sqlite3_column_int(statement, 6));
        document.addEntity(std::move(parcel));
        step = sqlite3_step(statement);
    }
    sqlite3_finalize(statement);
    // Le systeme du dossier revient avec les parcelles : un export puis un
    // import ne doit pas rendre le plan muet sur son repere (K-06).
    sqlite3_stmt* srsStatement = nullptr;
    if (sqlite3_prepare_v2(database.get(),
            "SELECT srs_id FROM gpkg_contents WHERE table_name='cadastre_parcels'",
            -1, &srsStatement, nullptr) == SQLITE_OK) {
        if (sqlite3_step(srsStatement) == SQLITE_ROW) {
            const int epsg = sqlite3_column_int(srsStatement, 0);
            if (epsg > 0) {
                auto& props = document.properties();
                if (!props.has(kCleEpsgDossier)) props.addInt(kCleEpsgDossier, 0);
                props.setInt(kCleEpsgDossier, epsg);
                if (!props.has(kCleSystemeCoordonnees)) props.addString(kCleSystemeCoordonnees, "");
                props.setString(kCleSystemeCoordonnees,
                                libelleSysteme(epsg, systemesCoordonneesInstalles()));
            }
        }
        sqlite3_finalize(srsStatement);
    }
    // sqlite3_errcode() n'est pas fiable apres SQLITE_DONE : c'est le code de
    // sortie de la boucle qui dit si le parcours est alle au bout.
    return step == SQLITE_DONE;
}

} // namespace bcad::cadastre
