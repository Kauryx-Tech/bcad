//! BCAD Database (.bcad) reader/writer
//!
//! Pure Rust SQLite reader/writer for BCAD v2 format.
//! No Qt, no C++ dependencies.

use bcad_format::*;
use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum DbError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),

    #[error("Invalid schema version: {0} (supported: 1-2)")]
    InvalidSchemaVersion(i32),

    #[error("Missing required table: {0}")]
    MissingTable(String),

    #[error("Invalid entity data: {0}")]
    InvalidEntityData(String),

    #[error("Property serialization error: {0}")]
    PropertySerialization(String),

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("JSON serialization error: {0}")]
    Json(#[from] serde_json::Error),
}

pub type DbResult<T> = Result<T, DbError>;

/// Open a .bcad file in read-only mode
pub fn open_readonly(path: impl AsRef<std::path::Path>) -> DbResult<Database> {
    let conn = Connection::open_with_flags(path, rusqlite::OpenFlags::SQLITE_OPEN_READ_ONLY)?;
    Database::from_connection(conn)
}

/// Open a .bcad file read-write
pub fn open_readwrite(path: impl AsRef<std::path::Path>) -> DbResult<Database> {
    let conn = Connection::open(path)?;
    Database::from_connection(conn)
}

/// Create a new .bcad file
pub fn create_new(path: impl AsRef<std::path::Path>) -> DbResult<Database> {
    let conn = Connection::open(path)?;
    let mut db = Database { conn };
    db.initialize_schema()?;
    Ok(db)
}

/// BCAD database wrapper
pub struct Database {
    conn: Connection,
}

impl Database {
    fn from_connection(conn: Connection) -> DbResult<Self> {
        let mut db = Self { conn };
        db.check_schema_version()?;
        Ok(db)
    }

    fn check_schema_version(&mut self) -> DbResult<()> {
        let version: i32 = self
            .conn
            .query_row("PRAGMA user_version", [], |row| row.get(0))?;

        if !(1..=2).contains(&version) {
            return Err(DbError::InvalidSchemaVersion(version));
        }
        Ok(())
    }

    fn initialize_schema(&mut self) -> DbResult<()> {
        self.conn.execute_batch(
            r#"
            PRAGMA user_version = 2;
            PRAGMA journal_mode = WAL;

            CREATE TABLE layers (
                name TEXT PRIMARY KEY,
                color_r REAL, color_g REAL, color_b REAL,
                line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER
            );

            CREATE TABLE entities (
                id INTEGER PRIMARY KEY,
                type_id TEXT NOT NULL,
                layer TEXT NOT NULL,
                handle TEXT,
                has_color_override INTEGER,
                color_r REAL, color_g REAL, color_b REAL,
                props_json TEXT
            );

            CREATE TABLE entity_properties (
                entity_id INTEGER NOT NULL,
                key TEXT NOT NULL,
                type TEXT NOT NULL,
                value TEXT NOT NULL,
                FOREIGN KEY(entity_id) REFERENCES entities(id)
            );

            CREATE INDEX idx_entity_properties_entity ON entity_properties(entity_id);
            "#,
        )?;
        Ok(())
    }

    /// Get schema version
    pub fn schema_version(&self) -> DbResult<i32> {
        self.conn
            .query_row("PRAGMA user_version", [], |row| row.get(0))
            .map_err(DbError::from)
    }

    /// Get all layers
    pub fn layers(&self) -> DbResult<Vec<Layer>> {
        let mut stmt = self.conn.prepare(
            "SELECT name, color_r, color_g, color_b, line_weight, visible, locked, line_type FROM layers"
        )?;
        let rows = stmt.query_map([], |row| {
            Ok(Layer {
                name: row.get(0)?,
                color: Color {
                    r: row.get(1)?,
                    g: row.get(2)?,
                    b: row.get(3)?,
                    a: 1.0,
                },
                line_weight: row.get(4)?,
                visible: row.get::<_, i32>(5)? != 0,
                locked: row.get::<_, i32>(6)? != 0,
                line_type: row.get(7)?,
            })
        })?;
        rows.map(|r| r.map_err(DbError::from)).collect()
    }

    /// Get all entities with properties
    pub fn entities(&self) -> DbResult<Vec<EntityRecord>> {
        let mut stmt = self.conn.prepare(
            "SELECT id, type_id, layer, handle, has_color_override, color_r, color_g, color_b, props_json FROM entities"
        )?;
        let rows = stmt.query_map([], |row| {
            let props_json: String = row.get(8)?;
            let properties: PropertyMap = serde_json::from_str(&props_json).map_err(|_| {
                rusqlite::Error::InvalidColumnType(
                    8,
                    "props_json".into(),
                    rusqlite::types::Type::Text,
                )
            })?;

            Ok(EntityRecord {
                id: row.get(0)?,
                type_id: row.get(1)?,
                layer: row.get(2)?,
                handle: row.get(3)?,
                color_override: if row.get::<_, i32>(4)? != 0 {
                    Some(Color {
                        r: row.get(5)?,
                        g: row.get(6)?,
                        b: row.get(7)?,
                        a: 1.0,
                    })
                } else {
                    None
                },
                properties,
            })
        })?;
        rows.map(|r| r.map_err(DbError::from)).collect()
    }

    /// Get entity properties as key-value rows
    pub fn entity_properties(&self, entity_id: i64) -> DbResult<Vec<PropertyRecord>> {
        let mut stmt = self
            .conn
            .prepare("SELECT key, type, value FROM entity_properties WHERE entity_id = ?")?;
        let rows = stmt.query_map(params![entity_id], |row| {
            Ok(PropertyRecord {
                key: row.get(0)?,
                prop_type: row.get(1)?,
                value: row.get(2)?,
            })
        })?;
        rows.map(|r| r.map_err(DbError::from)).collect()
    }

    /// Insert a layer
    pub fn insert_layer(&self, layer: &Layer) -> DbResult<()> {
        self.conn.execute(
            "INSERT OR REPLACE INTO layers (name, color_r, color_g, color_b, line_weight, visible, locked, line_type) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
            params![
                layer.name,
                layer.color.r, layer.color.g, layer.color.b,
                layer.line_weight,
                layer.visible as i32,
                layer.locked as i32,
                layer.line_type,
            ],
        )?;
        Ok(())
    }

    /// Insert an entity
    pub fn insert_entity(&self, entity: &EntityRecord) -> DbResult<i64> {
        let props_json = serde_json::to_string(&entity.properties)?;
        self.conn.execute(
            "INSERT INTO entities (type_id, layer, handle, has_color_override, color_r, color_g, color_b, props_json) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
            params![
                entity.type_id,
                entity.layer,
                entity.handle,
                entity.color_override.is_some() as i32,
                entity.color_override.as_ref().map(|c| c.r).unwrap_or(0.0),
                entity.color_override.as_ref().map(|c| c.g).unwrap_or(0.0),
                entity.color_override.as_ref().map(|c| c.b).unwrap_or(0.0),
                props_json,
            ],
        )?;
        Ok(self.conn.last_insert_rowid())
    }

    /// Insert entity properties
    pub fn insert_properties(&self, entity_id: i64, properties: &PropertyMap) -> DbResult<()> {
        for (key, value) in &properties.values {
            let type_str = match value {
                PropertyValue::Double(_) => "double",
                PropertyValue::Int(_) => "int",
                PropertyValue::String(_) => "string",
                PropertyValue::Bool(_) => "bool",
                PropertyValue::Color(_) => "color",
                PropertyValue::Enum(_) => "enum",
            };
            let value_str = serde_json::to_string(value)?;
            self.conn.execute(
                "INSERT INTO entity_properties (entity_id, key, type, value) VALUES (?1, ?2, ?3, ?4)",
                params![entity_id, key, type_str, value_str],
            )?;
        }
        Ok(())
    }

    /// Integrity check
    pub fn integrity_check(&self) -> DbResult<String> {
        self.conn
            .query_row("PRAGMA integrity_check", [], |row| row.get(0))
            .map_err(DbError::from)
    }

    /// Vacuum database
    pub fn vacuum(&self) -> DbResult<()> {
        self.conn.execute_batch("VACUUM")?;
        Ok(())
    }
}

/// Layer record
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Layer {
    pub name: String,
    pub color: Color,
    pub line_weight: f64,
    pub visible: bool,
    pub locked: bool,
    pub line_type: i32,
}

/// Entity record from database
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EntityRecord {
    pub id: i64,
    pub type_id: String,
    pub layer: String,
    pub handle: Option<String>,
    pub color_override: Option<Color>,
    pub properties: PropertyMap,
}

/// Property record from entity_properties table
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PropertyRecord {
    pub key: String,
    pub prop_type: String,
    pub value: String,
}

/// Migration from v1 to v2
pub fn migrate_v1_to_v2(conn: &mut Connection) -> DbResult<()> {
    // This would contain the migration logic from old cadastre_parcels table
    // to the new v2 schema with entity_properties table
    conn.execute_batch(
        r#"
        -- Migration logic here
        "#,
    )?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn test_create_and_open() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("test.bcad");
        let db = create_new(&path).unwrap();
        assert_eq!(db.schema_version().unwrap(), 2);
    }

    #[test]
    fn test_layer_roundtrip() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("test.bcad");
        let db = create_new(&path).unwrap();

        let layer = Layer {
            name: "TEST_LAYER".to_string(),
            color: Color::from_rgb255(255, 0, 0, 255),
            line_weight: 0.5,
            visible: true,
            locked: false,
            line_type: 0,
        };
        db.insert_layer(&layer).unwrap();

        let layers = db.layers().unwrap();
        assert_eq!(layers.len(), 1);
        assert_eq!(layers[0].name, "TEST_LAYER");
    }

    #[test]
    fn test_entity_roundtrip() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("test.bcad");
        let db = create_new(&path).unwrap();

        db.insert_layer(&Layer {
            name: "0".to_string(),
            color: Color::WHITE,
            line_weight: 0.0,
            visible: true,
            locked: false,
            line_type: 0,
        })
        .unwrap();

        let mut props = PropertyMap::new();
        props.insert("height", PropertyValue::Double(2.5));
        props.insert("material", PropertyValue::String("concrete".to_string()));

        let entity = EntityRecord {
            id: 0,
            type_id: "architecture:wall".to_string(),
            layer: "0".to_string(),
            handle: Some("1A2B".to_string()),
            color_override: None,
            properties: props,
        };

        let id = db.insert_entity(&entity).unwrap();
        db.insert_properties(id, &entity.properties).unwrap();

        let entities = db.entities().unwrap();
        assert_eq!(entities.len(), 1);
        assert_eq!(entities[0].type_id, "architecture:wall");
        assert_eq!(
            entities[0].properties.get("height"),
            Some(&PropertyValue::Double(2.5))
        );
    }

    #[test]
    fn test_integrity_check() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("test.bcad");
        let db = create_new(&path).unwrap();
        let result = db.integrity_check().unwrap();
        assert_eq!(result, "ok");
    }
}
