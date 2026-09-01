# Architecture de persistence BCAD

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

extern "C" void bcad_plugin_init(PluginRegistry& reg) {
    reg.serializerRegistry().registerSerializer(
        std::make_unique<WallSerializer>());
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

Le DXF exporte les types qu'il connaît. Les types plugin sont ignorés ou exportés en tant que bloc/proxy.

## 6. Versioning

Chaque serializer déclare une version de schéma :

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
3. Le format natif .bcad est SQLite + JSON
4. Les serializers gèrent le versioning
5. Le DXF est un serializer parmi d'autres
6. Document ne connaît pas le format