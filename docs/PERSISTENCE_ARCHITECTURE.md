# Architecture de persistence BCAD

> [!IMPORTANT]
>
> ## Statut : MIXTE — sérialisation implémentée, format cible
>
> Le **`SerializerRegistry`** (§4) est **implémenté** et utilisé par les plugins
> (preuve `examples/sdk_proof`, cycle de vie `PluginManager::unloadPlugin`). Les **§3, 5** décrivent
> le format réel (SQLite table `entities` + DXF). Le **§2 (schéma JSON)** et le **§6 (versioning)**
> restent **cibles**.

> Persistence extensible. Les plugins peuvent sérialiser leurs propres entités sans modifier le Core.

## 1. Problème actuel

Le format `.bcad` utilise des `switch(EntityType)` dans `Database.cpp` :

```cpp
switch (entity.type()) {
    case EntityType::Line:
        stmt << line.start.x << line.start.y << ...;
        break;
    case EntityType::Circle:
        stmt << circle.center.x << circle.center.y << circle.radius;
        break;
    // ...
}
```

Ajouter une nouvelle entité (Wall, Pipe, ...) nécessite de modifier Database.cpp.

## 2. Architecture cible

### 2.1 Interface du serializer

```cpp
namespace bcad::persistence {

class IEntitySerializer {
public:
    virtual ~IEntitySerializer() = default;

    // Identification
    virtual document::TypeId typeId() const = 0;

    // Sérialisation
    virtual std::string serialize(const document::Entity& entity) const = 0;
    virtual std::unique_ptr<document::Entity> deserialize(const std::string& data) const = 0;

    // Versioning
    virtual int schemaVersion() const { return 1; }
};

}
```

### 2.2 SerializerRegistry

```cpp
namespace bcad::registry {

class SerializerRegistry {
public:
    // Enregistrement
    void registerSerializer(std::unique_ptr<persistence::IEntitySerializer> serializer);

    // Accès
    const persistence::IEntitySerializer* find(document::TypeId typeId) const;
    std::optional<document::TypeId> findByName(const std::string& name) const;

    // Création (fabrique)
    std::unique_ptr<document::Entity> create(document::TypeId typeId) const;

    // Liste
    std::vector<document::TypeId> listTypes() const;

    // Persistance complète d'un document
    void serialize(const document::Document& doc, const std::string& path) const;
    void deserialize(document::Document& doc, const std::string& path) const;
};

}
```

### 2.3 Document serializer

```cpp
namespace bcad::persistence {

class IDocumentSerializer {
public:
    virtual ~IDocumentSerializer() = default;

    virtual std::string format() const = 0;       // "bcad", "dxf", "dwg"
    virtual std::string fileExtension() const = 0;  // ".bcad", ".dxf"
    virtual bool canRead(const std::string& path) const = 0;
    virtual bool canWrite(const std::string& path) const = 0;

    virtual void save(const document::Document& doc, const std::string& path,
                     const SerializerRegistry& registry) const = 0;
    virtual void load(document::Document& doc, const std::string& path,
                     const SerializerRegistry& registry) const = 0;
};

}
```

## 3. Format .bcad (SQLite)

Le format natif utilise SQLite pour stocker les entités :

> **État actuel (`src/io/Database.cpp`) :** le fichier porte `PRAGMA user_version = 3` ; les versions 1
> et 2 restent lisibles (la 2 sans attributs du dossier ni feuilles : elle n'en portait pas, la
> migration ne peut rien inventer), une version supérieure est refusée. Le schéma écrit est :
>
> ```sql
> CREATE TABLE entities (
>     id INTEGER PRIMARY KEY,
>     type_id TEXT NOT NULL,       -- la chaîne de l'enregistreur de types : "bcad.Line", "cadastre.parcel"
>     layer TEXT,
>     has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL,
>     params TEXT                  -- la chaîne du serializer pour ce type (CSV compact, pas de JSON)
> );
>
> CREATE TABLE entity_properties (
>     entity_id INTEGER NOT NULL,
>     key TEXT NOT NULL,
>     value_json TEXT NOT NULL,    -- {"type":"double","value":1250.42} — le type voyage avec la valeur
>     PRIMARY KEY (entity_id, key),
>     FOREIGN KEY (entity_id) REFERENCES entities(id) ON DELETE CASCADE
> );
>
> CREATE TABLE layers (name TEXT PRIMARY KEY, color_r REAL, color_g REAL, color_b REAL,
>                      line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER);
>
> CREATE TABLE document_properties (
>     key TEXT PRIMARY KEY,
>     value_json TEXT NOT NULL     -- attributs du dossier, même grammaire que les propriétés
> );
>
> CREATE TABLE sheets (            -- l'espace papier : feuilles, vues, meubles, champs
>     id INTEGER PRIMARY KEY, title TEXT NOT NULL,
>     format_token TEXT NOT NULL, orientation_token TEXT NOT NULL,  -- gardés tels quels, jamais
>     margin_top REAL, margin_bottom REAL, margin_left REAL, margin_right REAL);  -- retombés
>
> CREATE TABLE sheet_views (
>     sheet_id INTEGER NOT NULL, idx INTEGER NOT NULL,  -- source monde, échelle explicite,
>     src_minx REAL, src_miny REAL, src_maxx REAL, src_maxy REAL, scale REAL,  -- place papier :
>     paper_x REAL, paper_y REAL, paper_w REAL, paper_h REAL,  -- trois données distinctes
>     PRIMARY KEY (sheet_id, idx),
>     FOREIGN KEY (sheet_id) REFERENCES sheets(id) ON DELETE CASCADE);
>
> CREATE TABLE furniture (          -- nature rangée telle quelle, comme un `type_id` :
>     id INTEGER PRIMARY KEY, sheet_id INTEGER NOT NULL,  -- un meuble de module absent se
>     nature TEXT NOT NULL, template_id TEXT NOT NULL,  -- relit sans peintre et repart
>     zone_x REAL, zone_y REAL, zone_w REAL, zone_h REAL,  -- octet pour octet
>     FOREIGN KEY (sheet_id) REFERENCES sheets(id) ON DELETE CASCADE);
>
> CREATE TABLE furniture_fields (
>     furniture_id INTEGER NOT NULL, slot INTEGER NOT NULL,
>     role TEXT NOT NULL, label TEXT NOT NULL, key TEXT NOT NULL, format TEXT NOT NULL,
>     value_json TEXT NOT NULL,
>     PRIMARY KEY (furniture_id, slot),
>     FOREIGN KEY (furniture_id) REFERENCES furniture(id) ON DELETE CASCADE);
> ```
>
> `type_id` a remplacé l'entier d'enum historique, et `entity_properties` a remplacé la table
> `cadastre_parcels` que l'hôte déclarait en dur : l'écrivain ne connaît le nom d'aucune clé, il
> range ce que le document porte. C'est ce qui rend possible l'ouverture, sans le module concerné,
> d'un dessin qui en appelle un — voir §6.2.
>
> Le schéma ci-dessous est la **cible d'origine** ; elle n'a pas été adoptée telle quelle : la
> géométrie reste une chaîne de paramètres compacte produite par le serializer du type, seule la
> valeur d'une propriété est du JSON.

```sql
-- Entités
CREATE TABLE entities (
    id INTEGER PRIMARY KEY,
    type_id TEXT NOT NULL,      -- "core:line", "architecture:wall"
    data BLOB NOT NULL,          -- JSON sérialisé
    layer TEXT,
    created_at INTEGER,
    modified_at INTEGER
);

-- Calques
CREATE TABLE layers (
    name TEXT PRIMARY KEY,
    color INTEGER,
    line_weight REAL,
    visible INTEGER,
    locked INTEGER
);

-- Métadonnées
CREATE TABLE metadata (
    key TEXT PRIMARY KEY,
    value TEXT
);
```

Le JSON dans `data` est généré par le serializer :

```json
{
  "schemaVersion": 1,
  "typeId": "core:line",
  "data": {
    "start": { "x": 0, "y": 0 },
    "end": { "x": 100, "y": 50 }
  }
}
```

## 4. Enregistrement par un plugin

```cpp
// architecture plugin
class WallSerializer : public persistence::IEntitySerializer {
public:
    document::TypeId typeId() const override {
        return {"architecture", "wall", kGuidWall};
    }

    std::string serialize(const document::Entity& entity) const override {
        const auto* wall = static_cast<const WallEntity*>(&entity);
        // sérialiser en JSON
    }

    std::unique_ptr<document::Entity> deserialize(const std::string& data) const override {
        // désérialiser depuis JSON
    }
};

extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    reg.registerSerializer(std::make_unique<WallSerializer>());
    return true;
}
```

## 5. DXF import/export

```cpp
namespace bcad::io {

class DxfSerializer : public persistence::IDocumentSerializer {
public:
    std::string format() const override { return "dxf"; }
    std::string fileExtension() const override { return ".dxf"; }

    void save(const document::Document& doc, const std::string& path,
              const SerializerRegistry& registry) const override;
    void load(document::Document& doc, const std::string& path,
              const SerializerRegistry& registry) const override;
};

}
```

Le DXF exporte la géométrie que chaque entité sait écrire (`Entity::writeDxf`) plus
toutes les propriétés du `PropertyMap` en XDATA générique `BCAD_PROPS` — donc les
clés d'un module absent survivent à un aller-retour. Le bloc/proxy (INSERT) n'est
pas implémenté ; voir `IO_ARCHITECTURE.md` §4.

## 6. Versioning

### 6.1 Version du fichier

La version de schéma SQLite est stockée dans `PRAGMA user_version`. La version écrite est `3`. Les
versions `1` (et les anciennes sans pragma, version `0`) et `2` sont lues avec un chemin de
compatibilité, une version supérieure est refusée plutôt que chargée à un schéma que l'hôte ignore.
Toute évolution nécessitant une migration doit incrémenter cette valeur et ajouter la migration
explicite **avant** d'augmenter la constante du lecteur.

Matrice de compatibilité, telle que la vérifie `tests/unit/io/BcadSchemaTest.cpp` (fixtures
`tests/fixtures/bcad/legacy_v1.bcad`, `reference_v2.bcad` et `future_v4.bcad`) :

| Fichier | Comportement |
|---------|--------------|
| v1 (`user_version` 0 ou 1) | lu : `entities.type` entier traduit en `type_id`, `cadastre_parcels` relu en lignes de `entity_properties` ; migré en v3 avec tables de mise en page vides |
| v2 | lu (sans dossier ni feuilles) et migré en v3 (tables vides) ; n'est plus écrit |
| v3 | lu et écrit (entités, propriétés, dossier, feuilles, vues, meubles, champs) |
| v4 et au-delà | refusé, fichier et document en mémoire inchangés |
| clé absente du document | créée avec le type et la valeur du fichier |
| clé déclarée par le module, même type | la valeur du fichier gagne |
| clé déclarée par le module, type différent | le schéma du module gagne (un fichier n'impose pas une chaîne à une propriété relue comme un `Enum`) |
| colonne v1 vide | n'était pas une valeur : ne devient pas une propriété vide |

`Database::migrateSchema(path)` monte un fichier de v1 ou v2 à v3 **sans** le repasser par un document :
`PRAGMA foreign_keys=OFF`, `BEGIN IMMEDIATE`, traduction des types (palier v1→v2, conditionnel),
transformation des six colonnes en lignes JSON échappées à la main (l'extension JSON1 n'est pas
garantie sur un poste hors ligne), reconstruction de la table de liens pour que sa clé étrangère
cible la table portée, puis pose des tables de mise en page vides (`IF NOT EXISTS` : un v2 ne
portait ni dossier ni feuilles, la migration ne peut rien inventer), `user_version = 3`, `COMMIT`.
Tout ou rien : un échec laisse le fichier à sa version d'entrée, sans table résiduelle. Elle est
idempotente sur du v3 et refuse une version future. Elle produit exactement le même document que la
lecture du v1 — la migration n'est pas une deuxième interprétation du format.

### 6.2 Un module absent n'est pas une entité perdue

`SerializerRegistry` ne connaît que les types des modules chargés. Une ligne dont `type_id` n'a pas
de serializer n'est donc **pas** abandonnée : `geom::UnknownEntity` (`include/bcad/geometry/UnknownEntity.h`)
conserve le type réel, la chaîne de paramètres octet pour octet et les propriétés typées. Elle ne
contribue aucune géométrie à l'écran (emprise vide, aucune tessellation, incrochable), mais elle
reste comptée, listée, et réécrite telle quelle. L'ouvrir, le sauvegarder, puis réinstaller le module
retrouve l'entité réelle : la reconversion a lieu à la relecture, quand le serializer du module
retrouve son type.

Cette règle est ce qu'un poste sans le module ne puisse pas détruire le travail d'un poste équipé ;
le test `testEquippedWriteBlindWriteEquippedRead` en est la preuve aller-retour.

### 6.3 Version de schéma par serializer (cible, non implémentée)

`IEntitySerializer` ne déclare pas encore de version : le versionnement est celui du fichier, pas
celui du type. La cible resterait :

```cpp
class WallSerializer : public IEntitySerializer {
public:
    int schemaVersion() const override { return 2; }  // +1 si changement de format
};
```

À la lecture, si `schemaVersion < current`, un migrateur est appliqué :

```cpp
void WallSerializer::migrate(std::string& data, int fromVersion) {
    while (fromVersion < schemaVersion()) {
        switch (fromVersion) {
            case 1: migrateV1toV2(data); break;
        }
        ++fromVersion;
    }
}
```

## 7. Règles

1. Chaque type d'entité a un serializer
2. Les plugins enregistrent leurs serializers
3. Le format natif .bcad est SQLite : `type_id` + la chaîne de paramètres du serializer, et une table
   générale `entity_properties` où le type de chaque valeur est une donnée de la ligne (JSON)
4. Le versionnement est celui du fichier (`user_version`) ; le format d'un type est la affaire de son
   serializer, qui ne déclare pas encore de version (§6.3)
5. Le DXF est un serializer parmi d'autres
6. Document ne connaît pas le format
7. Une entité que l'hôte ne sait pas interpréter est conservée et réécrite, jamais ignorée : un
   ouvrir/enregistrer ne doit rien détruire (§6.2)
8. L'hôte ne nomme aucun domaine : `scripts/check_arch.sh` garde `src/io/` et `include/bcad/io/`
   libres d'en-tête et d'identifiant métier, hors les lignes marquées `NOLINT(arch-legacy-v1)`
   (la lecture et la migration du format v1)