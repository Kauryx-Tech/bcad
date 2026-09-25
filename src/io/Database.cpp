#include "bcad/io/Database.h"

#include "bcad/geometry/Arc.h"
#include "bcad/geometry/Circle.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/PointEntity.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/UnknownEntity.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"
#include "bcad/serialization/Serializer.h"
#include "JsonText.h"

#include <cstdio>
#include <iterator>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bcad::io {

using namespace geom;
using namespace core;
using properties::EnumIndex;
using properties::Property;
using properties::PropertyMap;
using properties::PropertyType;
using properties::PropertyValue;

namespace {

// Format v2 : la géométrie et le type réel de chaque entité dans `entities`
// (`type_id` porte la chaîne de l'enregistreur de types, plus un entier
// d'enum), les propriétés dans `entity_properties`, une ligne par clé. L'hôte
// ne connaît le nom d'aucune clé : il range ce que le document porte, y compris
// celui d'un module qui n'est pas installé sur ce poste.
//
// Format v1 : `entities.type` était un entier de l'enum historique, et les
// seules propriétés conserves six colonnes d'une table `cadastre_parcels` creee
// en dur. Un type hors de cette enum n'avait pas de place : l'entite etait
// declaree puis abandonnee a la relecture, ce qui rendait un simple
// ouvrir/enregistrer destructeur.
constexpr int kCurrentSchemaVersion = 2;
constexpr int kLegacySchemaVersion = 1;

// Nom de colonne du format v1, dans l'ordre de ses six champs. Ce sont des
// noms d'un format depasse, pas le vocabulaire d'un module : le module, lui,
// n'a plus besoin que l'hote connaisse ses cles pour les sauvegarder.
constexpr std::string_view kLegacyColumns[] = {
    "section", "numero", "contenance", "commune", "proprietaire", "nature"
};

// Les deux seuls littéraux métier que src/io manipule : l'identifiant de type et
// le préfixe de clé qu'un fichier v1 portait, pour que la migration et la
// lecture de compatibilité puissent les reconnaître. Rien d'autre, dans ce
// module, ne nomme un domaine — ni à l'écriture, ni au chargement d'un v2.
constexpr std::string_view kLegacyParcelTypeId = "cadastre.parcel"; // NOLINT(arch-legacy-v1)
constexpr std::string_view kLegacyKeyPrefix = "cadastre.";          // NOLINT(arch-legacy-v1)

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

// Remplace chaque occurrence de `from` par `to` dans `text`.
void substitute(std::string& text, std::string_view from, std::string_view to) {
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

int schemaVersionOf(sqlite3* db) {    StmtHandle st;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &st.stmt, nullptr) != SQLITE_OK)
        throw std::runtime_error("user_version introuvable");
    if (sqlite3_step(st.stmt) != SQLITE_ROW)
        throw std::runtime_error("user_version illisible");
    return sqlite3_column_int(st.stmt, 0);
}

bool tableExists(sqlite3* db, std::string_view name) {
    StmtHandle st;
    const char* sql = "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &st.stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(st.stmt, 1, std::string(name).c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(st.stmt) == SQLITE_ROW;
}

// Une colonne TEXT peut être NULL (une entité sans serializer, par exemple) :
// une chaîne vide est la seule réponse honnête, pas un pointeur converti en
// std::string.
std::string columnText(sqlite3_stmt* stmt, int col) {
    const auto* text = sqlite3_column_text(stmt, col);
    return text ? std::string(reinterpret_cast<const char*>(text)) : std::string{};
}

std::string serializeParamsOf(const Entity& e) {
    if (const auto* serializer = serialization::SerializerRegistry::find(e.typeId()))
        return serializer->serialize(e);
    // Pas de serializer : la chaîne que l'entité sait déjà rendre. Pour une
    // UnknownEntity c'est le payload du fichier, octet pour octet.
    return e.serializeParams();
}

// ------------------------------------------------------------------ value_json

// Le type est une donnée de la ligne, pas une supposition du lecteur : un réel
// de 1250.42 doit revenir en Double et non en chaîne ni en entier. Le domaine
// d'une énumération voyage avec elle (`values`) : c'est la seule façon de
// reinjecter un index sans le mettre hors domaine.
std::string encodeValue(const Property& prop) {
    std::ostringstream ss;
    ss.precision(17);
    ss << "{\"type\":\"";
    switch (prop.type()) {
        case PropertyType::Double:
            return (ss << "double\",\"value\":" << prop.asDouble() << "}", ss.str());
        case PropertyType::Int:
            return (ss << "int\",\"value\":" << prop.asInt() << "}", ss.str());
        case PropertyType::Bool:
            return (ss << "bool\",\"value\":" << (prop.asBool() ? "true" : "false") << "}", ss.str());
        case PropertyType::Color: {
            const auto c = prop.asColor();
            return (ss << "color\",\"value\":{\"r\":" << c.r << ",\"g\":" << c.g
                       << ",\"b\":" << c.b << ",\"a\":" << c.a << "}}", ss.str());
        }
        case PropertyType::Enum: {
            ss << "enum\",\"value\":" << prop.asEnum() << ",\"values\":[";
            const auto& values = prop.enumValues();
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i) ss << ',';
                ss << jsonString(values[i]);
            }
            return (ss << "]}", ss.str());
        }
        case PropertyType::String:
            return (ss << "string\",\"value\":" << jsonString(prop.asString()) << "}", ss.str());
    }
    return {};
}

struct DecodedValue {
    PropertyType type = PropertyType::String;
    PropertyValue value;
    std::vector<std::string> enumValues;
    int enumIndex = 0;
};

bool consume(std::string_view& text, std::string_view prefix) {
    if (text.size() < prefix.size() || text.substr(0, prefix.size()) != prefix) return false;
    text = text.substr(prefix.size());
    return true;
}

// Nombre jusqu'au délimiteur donné (':' n'existe pas ici, on cherche '}' ou ',').
std::optional<double> numberUntil(std::string_view text, char stop) {
    const std::size_t end = text.find(stop);
    if (end == std::string_view::npos) return std::nullopt;
    double value = 0.0;
    if (!parseJsonNumber(text.substr(0, end), value)) return std::nullopt;
    return value;
}

// Liste des libellés d'un enum : « "a","b" » ou vide.
std::vector<std::string> labelsOf(std::string_view text) {
    std::vector<std::string> out;
    while (!text.empty()) {
        if (!consume(text, "\"")) return out;
        const std::size_t quote = text.find('"');
        if (quote == std::string_view::npos) return out;
        out.push_back(jsonUnescape(std::string(text.substr(0, quote))));
        text = quote + 1 < text.size() ? text.substr(quote + 1) : std::string_view{};
        if (!consume(text, ",")) break;
    }
    return out;
}

// Lecture stricte : la grammaire est exactement celle d'encodeValue. Les
// chaînes sont prises par bornes (première quote ouvrante, dernière quote
// fermante), jamais par mot-clé, pour qu'une valeur contenant « "value": » ne
// devienne pas une structure. Un texte qui ne colle pas est une donnée invalide,
// pas un cas à deviner.
std::optional<DecodedValue> decodeValue(std::string_view text) {
    if (!consume(text, "{\"type\":\"")) return std::nullopt;
    const std::size_t tagEnd = text.find('"');
    if (tagEnd == std::string_view::npos) return std::nullopt;
    const std::string tag(text.substr(0, tagEnd));
    text = tagEnd + 1 < text.size() ? text.substr(tagEnd + 1) : std::string_view{};
    if (!consume(text, ",\"value\":")) return std::nullopt;

    DecodedValue out;
    if (tag == "double") {
        const auto value = numberUntil(text, '}');
        if (!value) return std::nullopt;
        out.type = PropertyType::Double;
        out.value = *value;
        return out;
    }
    if (tag == "int") {
        const auto value = numberUntil(text, '}');
        if (!value) return std::nullopt;
        out.type = PropertyType::Int;
        out.value = static_cast<int>(*value);
        return out;
    }
    if (tag == "bool") {
        if (consume(text, "true}")) out.value = true;
        else if (consume(text, "false}")) out.value = false;
        else return std::nullopt;
        out.type = PropertyType::Bool;
        return out;
    }
    if (tag == "string") {
        if (!consume(text, "\"")) return std::nullopt;
        const std::size_t close = text.rfind('"');
        if (close == std::string_view::npos || close + 1 >= text.size()) return std::nullopt;
        if (text.substr(close + 1) != "}") return std::nullopt;
        out.type = PropertyType::String;
        out.value = jsonUnescape(std::string(text.substr(0, close)));
        return out;
    }
    if (tag == "color") {
        if (!consume(text, "{\"r\":")) return std::nullopt;
        std::optional<double> r = numberUntil(text, ',');
        text = text.substr(text.find(',') + 1);
        if (!consume(text, "\"g\":")) return std::nullopt;
        std::optional<double> g = numberUntil(text, ',');
        text = text.substr(text.find(',') + 1);
        if (!consume(text, "\"b\":")) return std::nullopt;
        std::optional<double> b = numberUntil(text, ',');
        text = text.substr(text.find(',') + 1);
        if (!consume(text, "\"a\":")) return std::nullopt;
        std::optional<double> a = numberUntil(text, '}');
        if (!r || !g || !b || !a) return std::nullopt;
        out.type = PropertyType::Color;
        out.value = geom::Color{ static_cast<float>(*r), static_cast<float>(*g),
                                 static_cast<float>(*b), static_cast<float>(*a) };
        return out;
    }
    if (tag == "enum") {
        const auto index = numberUntil(text, ',');
        if (!index) return std::nullopt;
        text = text.substr(text.find(',') + 1);
        if (!consume(text, "\"values\":[")) return std::nullopt;
        const std::size_t close = text.rfind("]}");
        if (close == std::string_view::npos) return std::nullopt;
        if (text.substr(close) != "]}" && text.substr(close + 1) != "}") return std::nullopt;
        out.type = PropertyType::Enum;
        out.enumIndex = static_cast<int>(*index);
        out.value = EnumIndex(out.enumIndex);
        out.enumValues = labelsOf(text.substr(0, close));
        return out;
    }
    return std::nullopt;
}

// Règle de précédence, la même qu'en v1 où la table gagnait sur les paramètres,
// et la même qu'en DXF où la XDATA gagnait sur la géométrie :
//
//   - clé absente            : créée avec le type et la valeur du fichier ;
//   - clé déclarée, type concordant : la valeur du fichier gagne ;
//   - clé déclarée, type différent  : le schéma du module gagne. Un fichier ne
//     peut pas imposer une chaîne à une propriété que le module relit comme un
//     Enum (getEnum lèverait bad_variant_access) ; le module corrige le fichier
//     en réécrivant la propriété.
//
// Une propriété déclarée en lecture seule n'est pas touchée : sa valeur est
// recalculée par le module, pas archivée.
void applyStoredValue(PropertyMap& props, const std::string& key, const DecodedValue& stored) {
    if (const Property* existing = props.find(key)) {
        if (existing->type() != stored.type || existing->isReadOnly()) return;
    }

    switch (stored.type) {
        case PropertyType::Double: props.setDouble(key, std::get<double>(stored.value)); break;
        case PropertyType::Int: props.setInt(key, std::get<int>(stored.value)); break;
        case PropertyType::String: props.setString(key, std::get<std::string>(stored.value)); break;
        case PropertyType::Bool: props.setBool(key, std::get<bool>(stored.value)); break;
        case PropertyType::Color: props.setColor(key, std::get<geom::Color>(stored.value)); break;
        case PropertyType::Enum: {
            Property* prop = props.get(key);
            if (!prop || prop->type() != PropertyType::Enum) {
                prop = props.addEnum(key, 0, stored.enumValues);
            }
            try {
                prop->setFromEnum(stored.enumIndex);
            } catch (const std::out_of_range&) {
                // Index reçu d'un domaine plus long que celui déclaré ici : on
                // garde la valeur de départ, jamais un index hors domaine.
            }
            break;
        }
    }
}

// ------------------------------------------------------------- lignes de propriétés

using PropertyRows = std::unordered_map<int, std::vector<std::pair<std::string, std::string>>>;

PropertyRows readPropertyRows(sqlite3* db, const char* table) {
    PropertyRows rows;
    if (!tableExists(db, table)) return rows;
    StmtHandle st;
    const std::string sql = std::string("SELECT entity_id, key, value_json FROM ") + table + ";";
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st.stmt, nullptr) != SQLITE_OK) return rows;
    while (sqlite3_step(st.stmt) == SQLITE_ROW) {
        const int entityId = sqlite3_column_int(st.stmt, 0);
        rows[entityId].emplace_back(columnText(st.stmt, 1), columnText(st.stmt, 2));
    }
    return rows;
}

// Les six colonnes de la table v1 deviennent des lignes de la table générique.
// Une colonne vide n'était pas une valeur : elle ne devient pas une propriété
// vide (même règle que la lecture de la XDATA cadastral héritée, côté DXF).
PropertyRows readLegacyPropertyRows(sqlite3* db) {
    PropertyRows rows;
    if (!tableExists(db, "cadastre_parcels")) return rows;

    std::string sql = "SELECT entity_id";
    for (const auto& column : kLegacyColumns) sql += ", \"" + std::string(column) + "\"";
    sql += " FROM cadastre_parcels;";

    StmtHandle st;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st.stmt, nullptr) != SQLITE_OK) return rows;
    while (sqlite3_step(st.stmt) == SQLITE_ROW) {
        const int entityId = sqlite3_column_int(st.stmt, 0);
        for (std::size_t i = 0; i < std::size(kLegacyColumns); ++i) {
            std::string value = columnText(st.stmt, static_cast<int>(i) + 1);
            if (value.empty()) continue;
            std::string key(kLegacyKeyPrefix);
            key += kLegacyColumns[i];
            rows[entityId].emplace_back(
                std::move(key),
                std::string("{\"type\":\"string\",\"value\":") + jsonString(value) + "}");
        }
    }
    return rows;
}

// ------------------------------------------------------------------ v1 lecture

// L'entier de la colonne `type` de v1 et le TypeId qu'il désignait. 5 était
// réservé au module cadastral : c'est le seul sens métier qu'un fichier v1
// portait sans le nommer.
std::optional<std::string> legacyTypeId(int typeInt) {
    switch (typeInt) {
        case 0: return std::string(TypeId_Point.value);
        case 1: return std::string(TypeId_Line.value);
        case 2: return std::string(TypeId_Circle.value);
        case 3: return std::string(TypeId_Arc.value);
        case 4: return std::string(TypeId_Polyline.value);
        case 5: return std::string(kLegacyParcelTypeId);
        case 6: return std::string(TypeId_Text.value);
        default: return std::nullopt;
    }
}

// v1 écrivait `type=5` aussi bien pour une parcelle que pour une polyligne
// enrichie jamais promue : le marqueur '|' du format parcelle les distingue, et
// sans lui la ligne est la polyligne qu'elle a toujours été.
std::string legacyTypeIdOf(int typeInt, const std::string& params) {
    const std::optional<std::string> mapped = legacyTypeId(typeInt);
    if (!mapped) return {};
    if (*mapped == kLegacyParcelTypeId && params.find('|') == std::string::npos)
        return std::string(TypeId_Polyline.value);
    return *mapped;
}

void loadLayers(sqlite3* db, Document& doc) {
    const char* sql = "SELECT name,color_r,color_g,color_b,line_weight,visible,locked,line_type FROM layers;";
    StmtHandle st;
    if (sqlite3_prepare_v2(db, sql, -1, &st.stmt, nullptr) != SQLITE_OK) return;
    bool first = true;
    while (sqlite3_step(st.stmt) == SQLITE_ROW) {
        std::string name = columnText(st.stmt, 0);
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

std::unique_ptr<Entity> buildEntity(std::string_view typeIdText, const std::string& params,
                                    const std::string& layerName, bool hasOverride,
                                    const Color& overrideColor,
                                    const std::vector<std::pair<std::string, std::string>>& rows) {
    if (typeIdText.empty()) return nullptr;
    const TypeId typeId{std::string(typeIdText)};

    std::unique_ptr<Entity> entity;
    if (const auto* serializer = serialization::SerializerRegistry::find(typeId))
        entity = serializer->deserialize(params);
    if (!entity) {
        // Aucun serializer ne connaît ce type : le module est absent, ou le
        // fichier vient d'une version du module. Le contenu est conservé, pas
        // abandonné — c'est ce qui rend un ouvrir/enregistrer inoffensif.
        entity = std::make_unique<UnknownEntity>(typeId, params);
    }

    entity->setLayer(layerName);
    if (hasOverride) entity->setColorOverride(overrideColor);
    for (const auto& [key, encoded] : rows) {
        if (const auto decoded = decodeValue(encoded))
            applyStoredValue(entity->properties(), key, *decoded);
    }
    return entity;
}

const char* kCreateLayers =
    "CREATE TABLE layers ("
    " name TEXT PRIMARY KEY, color_r REAL, color_g REAL, color_b REAL,"
    " line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER);";

const char* kCreateEntitiesV2 =
    "CREATE TABLE entities ("
    " id INTEGER PRIMARY KEY, type_id TEXT NOT NULL, layer TEXT,"
    " has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL, params TEXT);";

const char* kCreatePropertiesV2 =
    "CREATE TABLE entity_properties ("
    " entity_id INTEGER NOT NULL, key TEXT NOT NULL, value_json TEXT NOT NULL,"
    " PRIMARY KEY (entity_id, key),"
    " FOREIGN KEY (entity_id) REFERENCES entities(id) ON DELETE CASCADE);";

} // namespace

bool Database::save(const std::string& path, const Document& doc) {
    std::remove(path.c_str());

    SqliteHandle h;
    if (sqlite3_open(path.c_str(), &h.db) != SQLITE_OK) return false;

    try {
        exec(h.db, "PRAGMA journal_mode=WAL;");
        exec(h.db, "PRAGMA foreign_keys=ON;");
        exec(h.db, "PRAGMA user_version = 2;");
        exec(h.db, kCreateLayers);
        exec(h.db, kCreateEntitiesV2);
        exec(h.db, kCreatePropertiesV2);

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
            const char* sqlEntity = "INSERT INTO entities VALUES (?,?,?,?,?,?,?,?);";
            const char* sqlProp = "INSERT INTO entity_properties VALUES (?,?,?);";
            StmtHandle stEntity, stProp;
            sqlite3_prepare_v2(h.db, sqlEntity, -1, &stEntity.stmt, nullptr);
            sqlite3_prepare_v2(h.db, sqlProp, -1, &stProp.stmt, nullptr);

            for (const auto& e : doc.entities()) {
                // Aucun filtre sur le type : v2 ne manipule que des chaînes
                // d'identifiants. Une entité de module absent s'écrit avec son
                // type réel et son payload intact.
                const std::string typeId = e->typeId().value;
                if (typeId.empty()) continue;

                sqlite3_reset(stEntity.stmt);
                sqlite3_bind_int(stEntity.stmt, 1, e->id());
                sqlite3_bind_text(stEntity.stmt, 2, typeId.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stEntity.stmt, 3, e->layer().c_str(), -1, SQLITE_TRANSIENT);
                const bool hasOverride = e->colorOverride().has_value();
                sqlite3_bind_int(stEntity.stmt, 4, hasOverride ? 1 : 0);
                const Color c = hasOverride ? *e->colorOverride() : Color{};
                sqlite3_bind_double(stEntity.stmt, 5, c.r);
                sqlite3_bind_double(stEntity.stmt, 6, c.g);
                sqlite3_bind_double(stEntity.stmt, 7, c.b);
                const std::string params = serializeParamsOf(*e);
                sqlite3_bind_text(stEntity.stmt, 8, params.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stEntity.stmt) != SQLITE_DONE)
                    throw std::runtime_error("insert entity failed");

                for (const auto& name : e->properties().listNames()) {
                    const Property* prop = e->properties().get(name);
                    if (!prop) continue;
                    const std::string encoded = encodeValue(*prop);
                    sqlite3_reset(stProp.stmt);
                    sqlite3_bind_int(stProp.stmt, 1, e->id());
                    sqlite3_bind_text(stProp.stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stProp.stmt, 3, encoded.c_str(), -1, SQLITE_TRANSIENT);
                    if (sqlite3_step(stProp.stmt) != SQLITE_DONE)
                        throw std::runtime_error("insert property failed");
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
    if (sqlite3_open_v2(path.c_str(), &h.db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
        return false;

    int version = 0;
    try {
        version = schemaVersionOf(h.db);
    } catch (const std::exception&) {
        return false;
    }
    // Une version future n'est pas chargée à l'aveugle avec un schéma qu'elle
    // ne connaît pas : elle est refusée, et le fichier reste intact.
    if (version > kCurrentSchemaVersion) return false;

    outDoc.clear();
    outDoc.layerManager().reset();
    Document& doc = outDoc;

    try {
        loadLayers(h.db, doc);

        const bool legacy = version <= kLegacySchemaVersion;
        const PropertyRows rows = legacy ? readLegacyPropertyRows(h.db)
                                         : readPropertyRows(h.db, "entity_properties");

        StmtHandle st;
        const char* sql = legacy
            ? "SELECT id,type,layer,has_color_override,color_r,color_g,color_b,params FROM entities;"
            : "SELECT id,type_id,layer,has_color_override,color_r,color_g,color_b,params FROM entities;";
        if (sqlite3_prepare_v2(h.db, sql, -1, &st.stmt, nullptr) != SQLITE_OK) return false;

        while (sqlite3_step(st.stmt) == SQLITE_ROW) {
            const int storedId = sqlite3_column_int(st.stmt, 0);
            const std::string params = columnText(st.stmt, 7);
            std::string typeIdText;
            if (legacy) {
                typeIdText = legacyTypeIdOf(sqlite3_column_int(st.stmt, 1), params);
                if (typeIdText.empty()) continue; // entier hors de l'enum v1 : type inconnu du format lui-même
            } else {
                typeIdText = columnText(st.stmt, 1);
            }

            const std::string layerName = columnText(st.stmt, 2);
            const bool hasOverride = sqlite3_column_int(st.stmt, 3) != 0;
            const Color overrideColor{ static_cast<float>(sqlite3_column_double(st.stmt, 4)),
                                       static_cast<float>(sqlite3_column_double(st.stmt, 5)),
                                       static_cast<float>(sqlite3_column_double(st.stmt, 6)) };

            const auto found = rows.find(storedId);
            auto entity = buildEntity(typeIdText, params, layerName, hasOverride, overrideColor,
                                      found != rows.end() ? found->second
                                                         : std::vector<std::pair<std::string, std::string>>{});
            if (!entity) continue;
            doc.addEntity(std::move(entity));
        }
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

int Database::schemaVersion(const std::string& path) {
    SqliteHandle h;
    if (sqlite3_open_v2(path.c_str(), &h.db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
        return -1;
    try {
        return schemaVersionOf(h.db);
    } catch (const std::exception&) {
        return -1;
    }
}

bool Database::migrateSchema(const std::string& path) {
    SqliteHandle h;
    // READWRITE : la migration est le seul chemin qui monte un fichier sans le
    // réécrire depuis un document. Un ouvrir/enregistrer produit déjà du v2.
    if (sqlite3_open_v2(path.c_str(), &h.db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
        return false;

    try {
        const int version = schemaVersionOf(h.db);
        if (version > kCurrentSchemaVersion) return false;
        if (version >= kCurrentSchemaVersion) return true; // déjà montée, idempotente

        exec(h.db, "PRAGMA foreign_keys=OFF;");
        // BEGIN IMMEDIATE : tout ou rien. Un échec laisse le fichier en v1,
        // jamais à moitié converti.
        exec(h.db, "BEGIN IMMEDIATE;");

        exec(h.db, "CREATE TABLE entities_new ("
                   " id INTEGER PRIMARY KEY, type_id TEXT, layer TEXT,"
                   " has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL, params TEXT);");

        // Même traduction que legacyTypeIdOf() côté chargeur : l'entier devient
        // la chaîne du TypeId, et un 5 sans marqueur '|' était une polyligne.
        std::string typeCase =
            "CASE type"
            " WHEN 0 THEN 'bcad.Point' WHEN 1 THEN 'bcad.Line' WHEN 2 THEN 'bcad.Circle'"
            " WHEN 3 THEN 'bcad.Arc' WHEN 4 THEN 'bcad.Polyline' WHEN 6 THEN 'bcad.Text'"
            " WHEN 5 THEN CASE WHEN params IS NULL OR params NOT LIKE '%|%' THEN '";
        typeCase += TypeId_Polyline.value;
        typeCase += "' ELSE '";
        typeCase += kLegacyParcelTypeId;
        typeCase += "' END ELSE NULL END";

        exec(h.db, ("INSERT INTO entities_new SELECT id, " + typeCase +
                    ", layer, has_color_override, color_r, color_g, color_b, params FROM entities;")
                       .c_str());
        // Un entier que la table ci-dessus ne sait pas nommer a atterri dans
        // type_id NULL : cette ligne n'est d'aucun type connu du format, elle
        // n'avait déjà aucune place en v2.
        exec(h.db, "DELETE FROM entities_new WHERE type_id IS NULL;");

        exec(h.db, "CREATE TABLE entity_properties_new ("
                   " entity_id INTEGER NOT NULL, key TEXT NOT NULL, value_json TEXT NOT NULL,"
                   " PRIMARY KEY (entity_id, key));");

        // Les six colonnes deviennent des lignes de type chaîne, échappées à la
        // main : json_quote() demanderait l'extension JSON1 de SQLite, que ce
        // poste hors ligne n'a pas forcément. Les mêmes échappements qu'à la
        // lecture, sinon un saut de ligne nu casserait une valeur que
        // readLegacyPropertyRows() aurait su protéger.
        static constexpr std::string_view kColumnInsert = R"SQL(INSERT INTO entity_properties_new
            SELECT entity_id, '%1',
                   '{"type":"string","value":"'
                       || replace(replace(replace(replace(replace("%2",
                                        '\', '\\'), '"', '\"'),
                                        char(10), '\n'),
                                        char(13), '\r'),
                                        char(9), '\t')
                       || '"}'
              FROM cadastre_parcels
             WHERE "%2" IS NOT NULL AND "%2" <> '';)SQL";
        if (tableExists(h.db, "cadastre_parcels")) {
            for (const auto& column : kLegacyColumns) {
                std::string sql(kColumnInsert);
                substitute(sql, "%2", std::string(column));
                substitute(sql, "%1", std::string(kLegacyKeyPrefix) + std::string(column));
                exec(h.db, sql.c_str());
            }
        }

        exec(h.db, "DROP TABLE IF EXISTS entity_properties;");
        exec(h.db, "DROP TABLE entities;");
        exec(h.db, "DROP TABLE IF EXISTS cadastre_parcels;");
        exec(h.db, "ALTER TABLE entities_new RENAME TO entities;");
        exec(h.db, "ALTER TABLE entity_properties_new RENAME TO entity_properties;");
        // La contrainte de clé étrangère est ajoutée par la référence sur la
        // table portante, ce que RENAME ne réécrit pas : on recrée la table de
        // liens une fois les noms rétablis.
        exec(h.db, "CREATE TABLE entity_properties_final ("
                   " entity_id INTEGER NOT NULL, key TEXT NOT NULL, value_json TEXT NOT NULL,"
                   " PRIMARY KEY (entity_id, key),"
                   " FOREIGN KEY (entity_id) REFERENCES entities(id) ON DELETE CASCADE);");
        exec(h.db, "INSERT INTO entity_properties_final SELECT * FROM entity_properties;");
        exec(h.db, "DROP TABLE entity_properties;");
        exec(h.db, "ALTER TABLE entity_properties_final RENAME TO entity_properties;");
        exec(h.db, "PRAGMA user_version = 2;");
        exec(h.db, "COMMIT;");
    } catch (const std::exception&) {
        exec(h.db, "ROLLBACK;");
        return false;
    }
    return true;
}

} // namespace bcad::io
