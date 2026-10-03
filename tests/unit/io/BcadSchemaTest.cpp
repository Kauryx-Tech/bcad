// Le format natif .bcad est-il honnete avec ce qu'il ne comprend pas ?
//
// Trois questions, trois series d'assertions :
//   - une propriete typée d'un module inconnu doit revenir avec son type et sa
//     valeur (la table generale `entity_properties` n'interprete rien) ;
//   - un fichier de la version 1, ou le metier tenait dans une table
//     `cadastre_parcels` declaree par l'hote, doit rester lisible, et sa
//     migration doit donner exactement la meme chose que sa lecture ;
//   - et surtout : ouvrir un document qui appelle un module absent, puis
//     l'enregistrer, ne doit rien detruire. Le collegue sans le module ne doit
//     pas pouvoir abimer le travail du collegue equipe.
//
// Le registre des serializateurs est global au processus. Les tests du « module
// absent » tournent donc avant que celui du module Cadastre soit enregistre, et
// le test central enregistre lui-meme le module au milieu de son histoire.

#include "entities/ParcelEntity.h"
#include "entities/SerializerRegistration.h"
#include "bcad/core/Document.h"
#include "bcad/geometry/Line.h"
#include "bcad/geometry/Polyline.h"
#include "bcad/geometry/TextEntity.h"
#include "bcad/geometry/UnknownEntity.h"
#include "bcad/io/Database.h"
#include "bcad/layout/FieldResolution.h"
#include "bcad/layout/Furniture.h"
#include "bcad/layout/Sheet.h"
#include "bcad/layout/Viewport.h"
#include "bcad/plugin/PluginRegistry.h"
#include "bcad/properties/PropertyMap.h"
#include "bcad/properties/PropertyTypes.h"
#include "bcad/serialization/Serializer.h"

#include <sqlite3.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace bcad;

#ifndef BCAD_FIXTURES_DIR
#define BCAD_FIXTURES_DIR ""
#endif

namespace {

std::string fixturePath(const std::string& name) {
    return std::string(BCAD_FIXTURES_DIR) + "/bcad/" + name;
}

std::string tempPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void copyFile(const std::string& from, const std::string& to) {
    std::filesystem::remove(to);
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
}

// ------------------------------------------------------------------ SQLite brut
//
// Une partie des assertions porte sur la forme du fichier : ce que l'hote a
// compris en memoire ne prouve pas qu'il a ecrit la bonne table.

struct Db {
    sqlite3* h = nullptr;
    explicit Db(const std::string& p) {
        if (sqlite3_open_v2(p.c_str(), &h, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) h = nullptr;
    }
    ~Db() {
        if (h) sqlite3_close(h);
    }
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;
};

std::vector<std::string> column(sqlite3* h, const std::string& sql) {
    std::vector<std::string> out;
    if (!h) return out;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(h, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        sqlite3_finalize(st);
        return out;
    }
    while (sqlite3_step(st) == SQLITE_ROW) {
        const auto* text = sqlite3_column_text(st, 0);
        out.push_back(text ? std::string(reinterpret_cast<const char*>(text)) : std::string());
    }
    sqlite3_finalize(st);
    return out;
}

std::vector<std::string> tableNames(const Db& db) {
    return column(db.h, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");
}

int userVersion(const Db& db) {
    const auto rows = column(db.h, "PRAGMA user_version;");
    return rows.empty() ? -1 : std::stoi(rows.front());
}

std::string columnsOf(const Db& db, const std::string& table) {
    const auto rows =
        column(db.h, "SELECT group_concat(name, ',') FROM pragma_table_info('" + table + "');");
    return rows.empty() ? std::string{} : rows.front();
}

size_t rowCount(const Db& db, const std::string& sql) {
    const auto rows = column(db.h, sql);
    return rows.empty() ? 0 : static_cast<size_t>(std::stoul(rows.front()));
}

std::vector<std::string> propertyRows(const Db& db, int entityId) {
    return column(db.h, "SELECT key || '=' || value_json FROM entity_properties WHERE entity_id=" +
                            std::to_string(entityId) + " ORDER BY 1;");
}

bool hasPrefix(const std::vector<std::string>& rows, const std::string& prefix) {
    return std::any_of(rows.begin(), rows.end(),
                       [&](const std::string& r) { return r.rfind(prefix, 0) == 0; });
}

bool hasValue(const std::vector<std::string>& rows, const std::string& value) {
    return std::find(rows.begin(), rows.end(), value) != rows.end();
}

// ------------------------------------------------------------------ description

// Une propriete resumee par son type et sa valeur, sans passer par un accesseur
// qui supposerait le type (asString() sur un Enum leve bad_variant_access).
std::string describe(const properties::Property& prop) {
    std::ostringstream ss;
    ss << prop.name() << ':';
    switch (prop.type()) {
        case properties::PropertyType::Double: ss.precision(17); ss << "double=" << prop.asDouble(); break;
        case properties::PropertyType::Int: ss << "int=" << prop.asInt(); break;
        case properties::PropertyType::Bool: ss << "bool=" << (prop.asBool() ? "1" : "0"); break;
        case properties::PropertyType::Color: {
            const auto c = prop.asColor();
            ss << "color=" << c.r << ',' << c.g << ',' << c.b << ',' << c.a;
            break;
        }
        case properties::PropertyType::Enum: {
            ss << "enum=" << prop.asEnum() << "/";
            const auto& values = prop.enumValues();
            for (std::size_t i = 0; i < values.size(); ++i) ss << (i ? "|" : "") << values[i];
            break;
        }
        case properties::PropertyType::String: ss << "string=" << prop.asString(); break;
    }
    return ss.str();
}

// Etat d'un document, en lignes triees : l'ordre d'un PropertyMap n'est pas une
// stabilite de test, et un desaccord entre deux fichiers ne doit pas en dependre.
std::vector<std::string> describeDocument(const core::Document& doc) {
    std::vector<std::string> out;
    for (const auto& e : doc.entities()) {
        std::ostringstream ss;
        ss << e->typeId().value << '|' << e->layer() << '|' << e->serializeParams();
        if (e->colorOverride()) ss << "|couleur";
        std::vector<std::string> props;
        for (const auto& name : e->properties().listNames()) {
            if (const auto* prop = e->properties().find(name)) props.push_back(describe(*prop));
        }
        std::sort(props.begin(), props.end());
        for (const auto& p : props) ss << "\n  " << p;
        out.push_back(ss.str());
    }
    return out;
}

size_t countProperties(const core::Document& doc) {
    size_t total = 0;
    for (const auto& e : doc.entities()) total += e->properties().listNames().size();
    return total;
}

const char* const kParcelParams = "1,0,0,30,0,30,15,0,15|A|012|999 m2|Dakar|TRAORE Awa|2";
const char* const kEscapedProprietaire = "KOUASSI \"K.\" \\ Kofi";

// ------------------------------------------------------------- v2 : typage

// Les six types du PropertyMap traversent la table generale sans que l'hote
// connaisse ni les cles, ni leur sens.
void testTypedValuesRoundTrip() {
    core::Document doc;
    auto line = std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(5, 5));
    auto& props = line->properties();
    props.setDouble("network.diameter", 1250.42);
    props.setInt("network.joints", 7);
    props.setString("network.material", "PVC");
    props.setBool("network.pressureTest", true);
    props.setColor("network.markColor", geom::Color{0.25f, 0.5f, 0.75f, 1.0f});
    props.addEnum("network.status", 2, {"pose", "verifie", "abandonne"});
    doc.addEntity(std::move(line));

    const std::string path = tempPath("bcad_schema_typed.bcad");
    assert(io::Database::save(path, doc));

    {
        const Db db(path);
        assert(userVersion(db) == 3);
        assert(columnsOf(db, "entities") ==
               "id,type_id,layer,has_color_override,color_r,color_g,color_b,params");
        const auto names = tableNames(db);
        assert(std::find(names.begin(), names.end(), "cadastre_parcels") == names.end());
        assert(std::find(names.begin(), names.end(), "entity_properties") != names.end());

        // Le type est une donnee de la ligne, pas une supposition du lecteur.
        const auto rows = propertyRows(db, doc.entities().front()->id());
        assert(rows.size() == 6);
        assert(hasPrefix(rows, "network.diameter={\"type\":\"double\",\"value\":1250."));
        assert(hasValue(rows, "network.joints={\"type\":\"int\",\"value\":7}"));
        assert(hasValue(rows, "network.material={\"type\":\"string\",\"value\":\"PVC\"}"));
        assert(hasValue(rows, "network.pressureTest={\"type\":\"bool\",\"value\":true}"));
        assert(hasValue(rows, "network.markColor={\"type\":\"color\",\"value\":{\"r\":0.25,\"g\":0.5,"
                              "\"b\":0.75,\"a\":1}}"));
        assert(hasValue(rows, "network.status={\"type\":\"enum\",\"value\":2,\"values\":"
                              "[\"pose\",\"verifie\",\"abandonne\"]}"));
    }

    core::Document back;
    assert(io::Database::load(path, back));
    std::filesystem::remove(path);

    assert(back.entities().size() == 1);
    const auto& restored = back.entities().front()->properties();
    assert(restored.find("network.diameter")->type() == properties::PropertyType::Double);
    assert(restored.getDouble("network.diameter") == 1250.42);
    assert(restored.find("network.joints")->type() == properties::PropertyType::Int);
    assert(restored.getInt("network.joints") == 7);
    assert(restored.getString("network.material") == "PVC");
    assert(restored.getBool("network.pressureTest", false));
    assert(restored.getColor("network.markColor") == geom::Color(0.25f, 0.5f, 0.75f, 1.0f));
    assert(restored.getEnum("network.status") == 2);
    assert(restored.find("network.status")->enumValues().size() == 3);
    assert(restored.find("network.status")->enumValues()[1] == "verifie");
}

// Une entite dont aucun serializer ne veut reste dans le fichier, avec son type
// reel et sa chaine de parametres octet pour octet.
void testOpaqueEntitySurvivesOpenAndSave() {
    core::Document doc;
    // Document::addEntity rejette une entite sur un calque qu'il ne connait
    // pas : le calque est declare d'abord, sinon c'est la regle du document qui
    // parlerait, pas celle de la persistance.
    doc.layerManager().createLayer("RESEAUX", geom::Color{});
    auto unknown = std::make_unique<geom::UnknownEntity>(geom::TypeId{"network.pipe"},
                                                         "opaque;payload,with|bars");
    unknown->setLayer("RESEAUX");
    unknown->properties().setString("network.material", "PEHD");
    unknown->properties().setDouble("network.depth", 1.25);
    doc.addEntity(std::move(unknown));

    const std::string path = tempPath("bcad_schema_opaque.bcad");
    assert(io::Database::save(path, doc));

    {
        const Db db(path);
        const auto types = column(db.h, "SELECT type_id FROM entities;");
        assert(types.size() == 1 && types.front() == "network.pipe");
        const auto payloads = column(db.h, "SELECT params FROM entities;");
        assert(payloads.size() == 1 && payloads.front() == "opaque;payload,with|bars");
    }

    core::Document back;
    assert(io::Database::load(path, back));
    std::filesystem::remove(path);

    assert(back.entities().size() == 1);
    const auto* kept = dynamic_cast<const geom::UnknownEntity*>(back.entities().front().get());
    assert(kept);
    assert(kept->typeId() == "network.pipe");
    assert(kept->payload() == "opaque;payload,with|bars");
    assert(kept->layer() == "RESEAUX");
    assert(kept->properties().getString("network.material") == "PEHD");
    assert(kept->properties().getDouble("network.depth") == 1.25);
}

// Porte 3 au niveau fichier : un type inconnu génère un diagnostic nommant le
// type_id. Prouve que Database::load() signale sans abandonner l'entité.
void testUnknownEntityGeneratesDiagnostic() {
    core::Document doc;
    doc.layerManager().createLayer("TEST", geom::Color{});
    auto unk = std::make_unique<geom::UnknownEntity>(geom::TypeId{"future.widget"}, "v=1");
    unk->setLayer("TEST");
    doc.addEntity(std::move(unk));

    const std::string path = tempPath("bcad_schema_diag.bcad");
    assert(io::Database::save(path, doc));

    core::Document back;
    std::vector<std::string> diag;
    assert(io::Database::load(path, back, &diag));
    std::filesystem::remove(path);

    // L'entité est conservée
    assert(back.entities().size() == 1);
    assert(dynamic_cast<const geom::UnknownEntity*>(back.entities().front().get()));

    // Un diagnostic nommant le type_id a été émis
    assert(diag.size() == 1);
    assert(diag[0].find("future.widget") != std::string::npos);
}

// --------------------------------------------------------------- v1 : lecture

// Le fixture est la reproduction du v1 : six lignes d'entites, dont un type que
// le format v1 lui-meme ne savait pas nommer.
void testLegacyV1LoadsWithoutPlugin() {
    core::Document doc;
    assert(io::Database::load(fixturePath("legacy_v1.bcad"), doc));
    assert(doc.entities().size() == 5);

    // Deux calques, dont celui des parcelles avec sa couleur.
    assert(doc.layerManager().find("CADASTRE") != nullptr);
    assert(doc.layerManager().find("CADASTRE")->color == geom::Color(0.8f, 0.2f, 0.2f, 1.0f));

    // 1. type=5 sans '|' : une polyligne enrichie que v1 n'a jamais promue, et
    //    qui reste la polyligne qu'elle a toujours ete.
    const auto* enriched = dynamic_cast<const geom::PolylineEntity*>(doc.entities()[0].get());
    assert(enriched);
    assert(enriched->typeId() == "bcad.Polyline");
    assert(enriched->closed());
    assert(enriched->vertices().size() == 4);
    const auto& first = doc.entities()[0]->properties();
    assert(first.getString("cadastre.section") == "AB");
    assert(first.getString("cadastre.numero") == "0072");
    assert(first.getString("cadastre.contenance") == "1234 m2");
    assert(first.getString("cadastre.commune") == "Abidjan");
    // Guillemet et barre oblique inverse dans la valeur : l'evasion JSON doit
    // rendre la chaine recue, pas une moitie de chaine.
    assert(first.getString("cadastre.proprietaire") == kEscapedProprietaire);
    // Une colonne vide de la table v1 n'etait pas une valeur : elle ne devient
    // pas une propriete vide.
    assert(!first.has("cadastre.nature"));
    assert(first.find("cadastre.section")->type() == properties::PropertyType::String);

    // 2. type=5 avec '|' : le format parcelle, que rien ne sait lire ici.
    const auto* parcel = dynamic_cast<const geom::UnknownEntity*>(doc.entities()[1].get());
    assert(parcel);
    assert(parcel->typeId() == "cadastre.parcel");
    assert(parcel->payload() == kParcelParams);
    assert(doc.entities()[1]->properties().listNames().size() == 4); // nature et proprietaire vides
    assert(doc.entities()[1]->properties().getString("cadastre.contenance") == "1250 m2");

    // 3. une ligne et sa couleur explicite.
    const auto* line = dynamic_cast<const geom::LineEntity*>(doc.entities()[2].get());
    assert(line);
    assert(line->colorOverride().has_value());
    assert(*line->colorOverride() == geom::Color(0.2f, 0.4f, 0.6f, 1.0f));

    // 4. un texte.
    const auto* text = dynamic_cast<const geom::TextEntity*>(doc.entities()[3].get());
    assert(text);
    assert(text->text() == "BONJOUR");

    // 5. une polyligne de type 5 sans ligne dans la table : geometrie seule.
    const auto* bare = dynamic_cast<const geom::PolylineEntity*>(doc.entities()[4].get());
    assert(bare);
    assert(bare->properties().listNames().empty());
}

// La migration n'est pas une seconde interpretation du v1 : elle doit donner le
// meme document que la lecture du v1.
void testMigrateV1MatchesLoadingV1() {
    const std::string path = tempPath("bcad_schema_migrate.bcad");
    copyFile(fixturePath("legacy_v1.bcad"), path);

    assert(io::Database::schemaVersion(path) == 1);
    assert(io::Database::migrateSchema(path));
    assert(io::Database::schemaVersion(path) == 3);
    assert(io::Database::migrateSchema(path)); // idempotente, et toujours v3
    assert(io::Database::schemaVersion(path) == 3);

    const Db db(path);
    const auto names = tableNames(db);
    assert(std::find(names.begin(), names.end(), "cadastre_parcels") == names.end());
    assert(std::find(names.begin(), names.end(), "entity_properties") != names.end());
    assert(columnsOf(db, "entities") ==
           "id,type_id,layer,has_color_override,color_r,color_g,color_b,params");

    // Les six colonnes sont devenues des lignes typées chaîne, sur l'identifiant
    // d'origine, et l'evasion est celle que le lecteur produit lui-meme.
    const auto rows = propertyRows(db, 1);
    assert(rows.size() == 5);
    assert(hasValue(rows, std::string("cadastre.contenance={\"type\":\"string\",\"value\":\"1234 m2\"}")));
    assert(hasValue(rows, std::string("cadastre.proprietaire={\"type\":\"string\",\"value\":"
                                      "\"KOUASSI \\\"K.\\\" \\\\ Kofi\"}")));

    // Un entier de type que v1 ne savait pas nommer n'avait aucune place en v2.
    assert(rowCount(db, "SELECT count(*) FROM entities WHERE id=6;") == 0);
    assert(rowCount(db, "SELECT count(*) FROM entities;") == 5);

    core::Document loaded, migrated;
    assert(io::Database::load(fixturePath("legacy_v1.bcad"), loaded));
    assert(io::Database::load(path, migrated));
    std::filesystem::remove(path);

    assert(describeDocument(loaded) == describeDocument(migrated));
}

// Tout ou rien : une migration qui echoue laisse le fichier dans son etat
// precedent, sans table residuelle.
void testFailedMigrationLeavesFileIntact() {
    const std::string path = tempPath("bcad_schema_notabcaf.bcad");
    std::filesystem::remove(path);
    sqlite3* h = nullptr;
    assert(sqlite3_open(path.c_str(), &h) == SQLITE_OK);
    char* err = nullptr;
    assert(sqlite3_exec(h, "CREATE TABLE something_else (x INTEGER);", nullptr, nullptr, &err) == SQLITE_OK);
    sqlite3_free(err);
    sqlite3_close(h);

    const std::string before = readFile(path);
    assert(!io::Database::migrateSchema(path)); // aucune table `entities` a traduire
    assert(readFile(path) == before);
    assert(io::Database::schemaVersion(path) == 0);

    const Db db(path);
    const auto names = tableNames(db);
    assert(std::find(names.begin(), names.end(), "entities_new") == names.end());
    assert(std::find(names.begin(), names.end(), "entity_properties") == names.end());
    std::filesystem::remove(path);
}

// Un fichier plus recent que le chargeur n'est pas lu avec un schema ignore, et
// n'est pas retouche.
void testFutureVersionIsRefusedAndLeftIntact() {
    const std::string path = tempPath("bcad_schema_future.bcad");
    copyFile(fixturePath("future_v4.bcad"), path);
    const std::string before = readFile(path);

    assert(io::Database::schemaVersion(path) == 4);
    assert(!io::Database::migrateSchema(path));
    assert(io::Database::schemaVersion(path) == 4);

    core::Document doc;
    assert(io::Database::load(fixturePath("legacy_v1.bcad"), doc));
    const auto state = describeDocument(doc);
    assert(!io::Database::load(path, doc));
    // Un refus ne vide pas le document ouvert : l'utilisateur peut continuer.
    assert(describeDocument(doc) == state);

    assert(readFile(path) == before);
    std::filesystem::remove(path);
}

// ------------------------------------------------------------- le test central

// Ouvrir sans le module, enregistrer, installer le module, rouvrir : le fichier
// doit traverser la session aveugle sans rien perdre, y compris ce que l'hote ne
// sait pas lire. C'est la regle qui commande toute cette table generale.
void testBlindSessionDestroysNothing() {
    assert(!serialization::SerializerRegistry::contains(cadastre::TypeId_Parcel));

    const std::string path = tempPath("bcad_schema_blind.bcad");
    std::filesystem::remove(path);

    core::Document blind;
    assert(io::Database::load(fixturePath("legacy_v1.bcad"), blind));
    assert(io::Database::save(path, blind));

    {
        const Db db(path);
        assert(userVersion(db) == 3);
        // Le type reel est ecrit, pas un type de repli.
        const auto types = column(db.h, "SELECT type_id FROM entities ORDER BY id;");
        assert(types.size() == 5);
        assert(std::find(types.begin(), types.end(), "cadastre.parcel") != types.end());
        // La chaine de parametres qu'aucun serializer enregistre ne savait lire
        // est revenue telle quelle.
        const auto payloads = column(db.h, "SELECT params FROM entities WHERE type_id='cadastre.parcel';");
        assert(payloads.size() == 1 && payloads.front() == kParcelParams);
        assert(rowCount(db, "SELECT count(*) FROM entity_properties;") == countProperties(blind));
    }

    core::Document stillBlind;
    assert(io::Database::load(path, stillBlind));
    assert(describeDocument(stillBlind) == describeDocument(blind));

    // Le module est installe sur le poste.
    plugin::PluginRegistry registry;
    assert(cadastre::registerParcelSerializer(registry));

    core::Document sighted;
    assert(io::Database::load(path, sighted));
    std::filesystem::remove(path);

    assert(sighted.entities().size() == blind.entities().size());
    const auto* parcel = dynamic_cast<const cadastre::ParcelEntity*>(sighted.entities()[1].get());
    assert(parcel);
    assert(parcel->vertices().size() == 4);
    const auto& props = sighted.entities()[1]->properties();
    assert(props.getString("cadastre.section") == "A");
    assert(props.getString("cadastre.numero") == "012");
    assert(props.getString("cadastre.contenance") == "1250 m2");
    assert(props.getString("cadastre.commune") == "Dakar");
    assert(props.getEnum("cadastre.nature") == 2);

    // Et la polyligne enrichie du haut du fichier a traverse elle aussi.
    const auto* enriched = dynamic_cast<const geom::PolylineEntity*>(sighted.entities()[0].get());
    assert(enriched);
    assert(enriched->properties().getString("cadastre.proprietaire") == kEscapedProprietaire);
}

// ------------------------------------------- module present : precedences

// Avec le module charge, la parcelle redevient une parcelle et la regle de
// precedence s'observe : la table corrige les parametres, les parametres
// remplissent ce que la table laissait.
void testLegacyV1WithParcelSerializer() {
    core::Document doc;
    assert(io::Database::load(fixturePath("legacy_v1.bcad"), doc));

    const auto* parcel = dynamic_cast<const cadastre::ParcelEntity*>(doc.entities()[1].get());
    assert(parcel);
    assert(parcel->closed());
    assert(parcel->vertices().size() == 4);
    const auto& props = doc.entities()[1]->properties();
    // params disait « 999 m2 », la table disait « 1250 m2 » : la table gagne.
    assert(props.getString("cadastre.contenance") == "1250 m2");
    assert(props.getString("cadastre.commune") == "Dakar");
    // La table laissait nature vide : c'est l'index porte par params qui tient,
    // et une chaine du fichier ne peut pas imposer son type a une propriete que
    // le module relit comme un Enum.
    assert(props.getEnum("cadastre.nature") == 2);
    assert(props.find("cadastre.nature")->type() == properties::PropertyType::Enum);

    // L'enrichie jamais promue reste une polyligne, module charge ou non.
    assert(dynamic_cast<const cadastre::ParcelEntity*>(doc.entities()[0].get()) == nullptr);
    assert(doc.entities()[0]->typeId() == "bcad.Polyline");
}

// Relire du v1 puis enregistrer produit du v3, sans rien inventer ni rien perdre.
void testSavingALegacyFileWritesOnlyV2() {
    core::Document doc;
    assert(io::Database::load(fixturePath("legacy_v1.bcad"), doc));

    const std::string path = tempPath("bcad_schema_resave.bcad");
    assert(io::Database::save(path, doc));

    const Db db(path);
    assert(userVersion(db) == 3);
    const auto names = tableNames(db);
    assert(std::find(names.begin(), names.end(), "cadastre_parcels") == names.end());
    // Une propriete du document = une ligne ; ni de plus, ni de moins.
    assert(rowCount(db, "SELECT count(*) FROM entity_properties;") == countProperties(doc));
    assert(rowCount(db, "SELECT count(*) FROM entities;") == doc.entities().size());

    core::Document back;
    assert(io::Database::load(path, back));
    std::filesystem::remove(path);
    assert(describeDocument(doc) == describeDocument(back));
}

// Le collegue equipe enregistre ; le collegue non equipe rouvre le meme
// document, y travaille, et enregistre ; l'equipe retrouve tout. Le cycle complet
// suppose de retirer le serializer du module, ce que PluginManager fait deja au
// dlclose d'un plugin.
void testEquippedWriteBlindWriteEquippedRead() {
    assert(serialization::SerializerRegistry::contains(cadastre::TypeId_Parcel));

    core::Document written;
    written.layerManager().createLayer("CADASTRE", geom::Color{});
    auto parcel = std::make_unique<cadastre::ParcelEntity>(
        std::vector<geom::Point2>{{0, 0}, {40, 0}, {40, 25}, {0, 25}});
    parcel->setLayer("CADASTRE");
    auto& props = parcel->properties();
    props.setString("cadastre.section", "B");
    props.setString("cadastre.numero", "1024");
    props.setString("cadastre.contenance", "1250 m2");
    props.setString("cadastre.commune", "Dakar");
    props.setString("cadastre.proprietaire", "Ndiaye");
    props.setEnum("cadastre.nature", 2);
    // Deux cles que le module lui-meme ne declare pas : posees par l'utilisateur
    // ou par un autre module, l'hote n'en sait rien, et c'est le cas normal.
    props.setString("cadastre.field_note", "cote 12,50 m du bornage");
    props.setDouble("survey.offset", 12.5);
    written.addEntity(std::move(parcel));

    // Ce que le format du module ecrit dans `params` : la chaine des sept champs,
    // pas la forme courte de la classe.
    const std::string expectedParams =
        serialization::SerializerRegistry::find(cadastre::TypeId_Parcel)
            ->serialize(*written.entities().front());

    const std::string path = tempPath("bcad_schema_cycle.bcad");
    assert(io::Database::save(path, written));

    // Le module est desinstalle du poste.
    serialization::SerializerRegistry::remove(cadastre::TypeId_Parcel);
    assert(!serialization::SerializerRegistry::contains(cadastre::TypeId_Parcel));

    core::Document blind;
    assert(io::Database::load(path, blind));
    assert(blind.entities().size() == 1);
    const auto* kept = dynamic_cast<const geom::UnknownEntity*>(blind.entities().front().get());
    assert(kept);
    assert(kept->typeId() == "cadastre.parcel");
    assert(kept->payload() == expectedParams);
    assert(kept->layer() == "CADASTRE");
    const auto& keptProps = blind.entities().front()->properties();
    assert(keptProps.listNames().size() == 8);
    assert(keptProps.getString("cadastre.contenance") == "1250 m2");
    assert(keptProps.getString("cadastre.field_note") == "cote 12,50 m du bornage");
    assert(keptProps.getDouble("survey.offset") == 12.5);
    // Un enum survit avec son domaine : sans lui, l'index 2 ne voudrait rien dire.
    assert(keptProps.getEnum("cadastre.nature") == 2);
    assert(keptProps.find("cadastre.nature")->enumValues().size() == 4);

    const std::string rewritten = tempPath("bcad_schema_cycle2.bcad");
    assert(io::Database::save(rewritten, blind));
    std::filesystem::remove(path);

    plugin::PluginRegistry registry;
    assert(cadastre::registerParcelSerializer(registry));

    core::Document restored;
    assert(io::Database::load(rewritten, restored));
    std::filesystem::remove(rewritten);

    const auto* parcelAgain = dynamic_cast<const cadastre::ParcelEntity*>(restored.entities().front().get());
    assert(parcelAgain);
    assert(parcelAgain->closed());
    assert(parcelAgain->vertices().size() == 4);
    assert(restored.entities().front()->layer() == "CADASTRE");
    const auto& back = restored.entities().front()->properties();
    assert(back.getString("cadastre.section") == "B");
    assert(back.getString("cadastre.contenance") == "1250 m2");
    assert(back.getString("cadastre.field_note") == "cote 12,50 m du bornage");
    assert(back.getDouble("survey.offset") == 12.5);
    assert(back.getEnum("cadastre.nature") == 2);
}

// ------------------------------------------------------------- v3 : ADR-017, tranche 2

// Porte 5 : le jeu v2 réaliste — entités, propriétés typées, entité de module
// absent — monte en v3 sans que le dessin bouge d'une ligne. La migration ne
// peut pas inventer les attributs du dossier ni les feuilles que le v2 ne
// portait pas : les tables arrivent vides, le reste est identique.
void testReferenceV2MigratesToV3Identical() {
    core::Document before;
    assert(io::Database::load(fixturePath("reference_v2.bcad"), before));
    assert(before.entities().size() == 3);
    const auto state = describeDocument(before);

    const std::string path = tempPath("bcad_schema_refv2.bcad");
    copyFile(fixturePath("reference_v2.bcad"), path);
    assert(io::Database::schemaVersion(path) == 2);
    assert(io::Database::migrateSchema(path));
    assert(io::Database::schemaVersion(path) == 3);
    assert(io::Database::migrateSchema(path)); // idempotente
    assert(io::Database::schemaVersion(path) == 3);

    {
        const Db db(path);
        const auto names = tableNames(db);
        for (const char* table : {"document_properties", "sheets", "sheet_views",
                                  "furniture", "furniture_fields"})
            assert(std::find(names.begin(), names.end(), table) != names.end());
        assert(rowCount(db, "SELECT count(*) FROM document_properties;") == 0);
        assert(rowCount(db, "SELECT count(*) FROM sheets;") == 0);
    }

    core::Document after;
    assert(io::Database::load(path, after));
    std::filesystem::remove(path);
    assert(describeDocument(after) == state);
    // L'inconnue a traversé la migration avec son type, son payload et ses clés.
    const auto* kept =
        dynamic_cast<const geom::UnknownEntity*>(after.entities()[2].get());
    assert(kept);
    assert(kept->typeId() == "network.pipe");
    assert(kept->payload() == "DN200;PEHD|z=1.2");
    assert(kept->properties().getString("network.material") == "PEHD");
    assert(kept->properties().getDouble("network.depth") == 1.2);
}

// Les attributs du dossier et l'espace papier traversent le fichier : clés
// inconnues de l'hôte comprises, nature de meuble inconnue comprise, format de
// champ inconnu compris. C'est la fin du « la feuille se perd en fermant ».
void testDossierAndSheetsRoundTrip() {
    core::Document doc;
    doc.layerManager().createLayer("BATIMENTS", geom::Color{});
    auto line = std::make_unique<geom::LineEntity>(geom::Point2(0, 0), geom::Point2(10, 5));
    line->setLayer("BATIMENTS");
    doc.addEntity(std::move(line));

    doc.properties().setString("dossier.projet", "Ecole Primaire Lome");
    doc.properties().setDouble("dossier.surface", 1250.42);
    doc.properties().setInt("dossier.feuillet", 3);
    doc.properties().setBool("dossier.valide", true);
    doc.properties().addEnum("dossier.statut", 1, {"brouillon", "valide", "archive"});

    layout::Sheet* sheet = doc.addSheet("A3 Paysage");
    assert(sheet);
    sheet->setFormatToken("A3");
    sheet->setOrientationToken("Paysage");
    layout::Margins m{12.0, 12.0, 10.0, 10.0};
    sheet->setMargins(m);

    layout::Viewport view;
    geom::BoundingBox src;
    src.minX = 0;
    src.minY = 0;
    src.maxX = 300;
    src.maxY = 200;
    view.setSource(src);
    view.setScale(500);
    view.setPaper({20.0, 20.0, 380.0, 250.0});
    sheet->views().push_back(view);

    // Meuble d'un module que ce poste ne connaît pas : conservé, restitué.
    layout::Furniture meuble("reseau.legende");
    meuble.setTemplateId("reseau.legende.decret_2024");
    meuble.setZone({20.0, 300.0, 100.0, 60.0});
    layout::Field inconnu;
    inconnu.role = "attribut";
    inconnu.label = "Exploitant";
    inconnu.key = "reseau.exploitant";
    inconnu.slot = 0;
    inconnu.value.type = properties::PropertyType::String;
    inconnu.value.value = std::string("TDE");
    meuble.addField(inconnu);
    layout::Field formatInconnu;
    formatInconnu.role = "attribut";
    formatInconnu.label = "Pression";
    formatInconnu.key = "reseau.pression";
    formatInconnu.format = "pression-bar"; // indice que l'hôte ne connaît pas
    formatInconnu.slot = 1;
    formatInconnu.value.type = properties::PropertyType::Double;
    formatInconnu.value.value = 4.5;
    meuble.addField(formatInconnu);
    layout::Field statut;
    statut.role = "attribut";
    statut.label = "Statut";
    statut.key = "reseau.statut";
    statut.slot = 2;
    statut.value.type = properties::PropertyType::Enum;
    statut.value.value = properties::EnumIndex{1};
    statut.value.enumValues = {"projet", "existant", "abandonne"};
    meuble.addField(statut);
    sheet->furniture().push_back(std::move(meuble));

    const std::string path = tempPath("bcad_schema_v3.bcad");
    assert(io::Database::save(path, doc));

    {
        const Db db(path);
        assert(userVersion(db) == 3);
        assert(rowCount(db, "SELECT count(*) FROM document_properties;") == 5);
        assert(rowCount(db, "SELECT count(*) FROM sheets;") == 1);
        assert(rowCount(db, "SELECT count(*) FROM sheet_views;") == 1);
        assert(rowCount(db, "SELECT count(*) FROM furniture;") == 1);
        assert(rowCount(db, "SELECT count(*) FROM furniture_fields;") == 3);
    }

    core::Document back;
    assert(io::Database::load(path, back));
    std::filesystem::remove(path);

    assert(back.properties().getString("dossier.projet") == "Ecole Primaire Lome");
    assert(back.properties().getDouble("dossier.surface") == 1250.42);
    assert(back.properties().getInt("dossier.feuillet") == 3);
    assert(back.properties().getBool("dossier.valide", false));
    assert(back.properties().getEnum("dossier.statut") == 1);

    assert(back.sheets().size() == 1);
    const layout::Sheet& feuille = *back.sheets().front();
    assert(feuille.title() == "A3 Paysage");
    assert(feuille.formatToken() == "A3");
    assert(feuille.orientationToken() == "Paysage");
    assert(feuille.margins().top == 12.0);
    assert(feuille.views().size() == 1);
    const layout::Viewport& vue = feuille.views().front();
    assert(vue.source().minX == 0 && vue.source().maxX == 300);
    assert(vue.source().minY == 0 && vue.source().maxY == 200);
    assert(vue.scale() == 500);
    assert(vue.paper().x == 20.0 && vue.paper().w == 380.0);

    assert(feuille.furniture().size() == 1);
    const layout::Furniture& relu = feuille.furniture().front();
    assert(relu.nature() == "reseau.legende");
    assert(relu.templateId() == "reseau.legende.decret_2024");
    assert(relu.zone().x == 20.0 && relu.zone().h == 60.0);
    assert(relu.fields().size() == 3);
    assert(relu.fields()[0].label == "Exploitant");
    assert(std::get<std::string>(relu.fields()[0].value.value) == "TDE");
    assert(relu.fields()[1].format == "pression-bar"); // rendu tel quel, gardé tel quel
    assert(std::get<double>(relu.fields()[1].value.value) == 4.5);
    assert(relu.fields()[2].value.type == properties::PropertyType::Enum);
    assert(std::get<properties::EnumIndex>(relu.fields()[2].value.value).value == 1);
    assert(relu.fields()[2].value.enumValues.size() == 3);
}

// Reste de la porte 3, au niveau du fichier : une clé que le gabarit ne nomme
// pas est conservée ET signalée ; une clé que le gabarit attend et que le
// dossier ne porte pas est un diagnostic qui nomme la clé, jamais une
// disparition silencieuse.
void testUnknownKeyRoundTripAndMissingValueDiagnostic() {
    layout::FurnitureTemplate gabarit;
    gabarit.id = "cadastre.cartouche.profil_national";
    gabarit.nature = "cadastre.cartouche";
    layout::TemplateField attendu;
    attendu.role = "attribut";
    attendu.label = "Commune";
    attendu.key = "dossier.commune";
    attendu.slot = 0;
    gabarit.fields.push_back(attendu);

    layout::Furniture meuble("cadastre.cartouche");
    layout::Field extra;
    extra.role = "attribut";
    extra.label = "Visa du chef";
    extra.key = "dossier.visa"; // le gabarit ne le nomme pas
    extra.slot = 7;
    extra.value.type = properties::PropertyType::String;
    extra.value.value = std::string("vu");
    meuble.addField(extra);

    properties::PropertyMap dossier; // ne porte pas dossier.commune
    const layout::FieldScope scope{&dossier, {}};

    std::vector<validation::Diagnostic> diagnostics;
    const std::vector<layout::Field> resolus =
        layout::resolveFields(meuble, gabarit, scope, diagnostics);

    // Le champ attendu sans valeur : un constat, pas une absence.
    assert(resolus.size() == 2);
    assert(resolus[0].key == "dossier.commune");
    assert(!resolus[0].hasValue());
    // L'inconnu conservé, et dit.
    assert(resolus[1].key == "dossier.visa");
    assert(std::get<std::string>(resolus[1].value.value) == "vu");

    bool manqueSignale = false, conserveSignale = false;
    for (const auto& diag : diagnostics) {
        if (diag.message.find("dossier.commune") != std::string::npos) manqueSignale = true;
        if (diag.message.find("dossier.visa") != std::string::npos) conserveSignale = true;
    }
    assert(manqueSignale);
    assert(conserveSignale);
}

} // namespace

int main() {
    serialization::SerializerRegistry::initializeNativeSerializers();

    // Phase « module absent » : seuls les types natifs ont un serializer.
    testTypedValuesRoundTrip();
    testOpaqueEntitySurvivesOpenAndSave();
    testUnknownEntityGeneratesDiagnostic();
    testLegacyV1LoadsWithoutPlugin();
    testMigrateV1MatchesLoadingV1();
    testFailedMigrationLeavesFileIntact();
    testFutureVersionIsRefusedAndLeftIntact();

    // Le test central enregistre lui-meme le module au milieu de son histoire.
    testBlindSessionDestroysNothing();

    // Phase « module present ».
    testLegacyV1WithParcelSerializer();
    testSavingALegacyFileWritesOnlyV2();
    testEquippedWriteBlindWriteEquippedRead();

    // Tranche 2 (ADR-017) : le format v3.
    testReferenceV2MigratesToV3Identical();
    testDossierAndSheetsRoundTrip();
    testUnknownKeyRoundTripAndMissingValueDiagnostic();
    return 0;
}
