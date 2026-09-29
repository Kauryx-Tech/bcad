//! BCAD Database (.bcad) reader/writer
//!
//! Lecteur/écrivain SQLite pur du format natif, sur le schéma réel de
//! `src/io/Database.cpp` — pas un schéma parallèle : ce que le C++ écrit,
//! ceci le lit, et inversement.
//!
//! - v1 : `entities.type` entier + `cadastre_parcels` (lecture de compatibilité) ;
//! - v2 : `entities` en `type_id` + `entity_properties` en `value_json` ;
//! - v3 : la v2 plus `document_properties`, `sheets`, `sheet_views`,
//!   `furniture`, `furniture_fields` ;
//! - au-delà : refusé, fichier et mémoire inchangés.
//!
//! Règle d'`UnknownEntity`, comme côté C++ : un `type_id` sans grammaire connue
//! est conservé (type réel + `params` octet pour octet) et signalé, jamais
//! abandonné. Un ouvrir/enregistrer aveugle ne détruit rien.
//!
//! No Qt, no C++ dependencies.

use bcad_format::legacy_v1;
use bcad_format::value_json;
use bcad_format::{Color, PropertyMap, PropertyValue};
use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum DbError {
    #[error("SQLite error: {0}")]
    Sqlite(#[from] rusqlite::Error),

    #[error("Invalid schema version: {0} (supported: 1-3)")]
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

/// Create a new .bcad file (real v3 schema, the only version written)
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

        if !(1..=3).contains(&version) {
            return Err(DbError::InvalidSchemaVersion(version));
        }
        Ok(())
    }

    fn initialize_schema(&mut self) -> DbResult<()> {
        self.conn.execute_batch(
            r#"
            PRAGMA user_version = 3;
            PRAGMA journal_mode = WAL;

            CREATE TABLE layers (
                name TEXT PRIMARY KEY,
                color_r REAL, color_g REAL, color_b REAL,
                line_weight REAL, visible INTEGER, locked INTEGER, line_type INTEGER
            );

            CREATE TABLE entities (
                id INTEGER PRIMARY KEY,
                type_id TEXT NOT NULL,
                layer TEXT,
                has_color_override INTEGER,
                color_r REAL, color_g REAL, color_b REAL,
                params TEXT
            );

            CREATE TABLE entity_properties (
                entity_id INTEGER NOT NULL,
                key TEXT NOT NULL,
                value_json TEXT NOT NULL,
                PRIMARY KEY (entity_id, key),
                FOREIGN KEY (entity_id) REFERENCES entities(id) ON DELETE CASCADE
            );

            CREATE TABLE document_properties (
                key TEXT PRIMARY KEY,
                value_json TEXT NOT NULL
            );

            CREATE TABLE sheets (
                id INTEGER PRIMARY KEY,
                title TEXT NOT NULL,
                format_token TEXT NOT NULL,
                orientation_token TEXT NOT NULL,
                margin_top REAL, margin_bottom REAL,
                margin_left REAL, margin_right REAL
            );

            CREATE TABLE sheet_views (
                sheet_id INTEGER NOT NULL,
                idx INTEGER NOT NULL,
                src_minx REAL, src_miny REAL, src_maxx REAL, src_maxy REAL,
                scale REAL,
                paper_x REAL, paper_y REAL, paper_w REAL, paper_h REAL,
                PRIMARY KEY (sheet_id, idx),
                FOREIGN KEY (sheet_id) REFERENCES sheets(id) ON DELETE CASCADE
            );

            CREATE TABLE furniture (
                id INTEGER PRIMARY KEY,
                sheet_id INTEGER NOT NULL,
                nature TEXT NOT NULL,
                template_id TEXT NOT NULL,
                zone_x REAL, zone_y REAL, zone_w REAL, zone_h REAL,
                FOREIGN KEY (sheet_id) REFERENCES sheets(id) ON DELETE CASCADE
            );

            CREATE TABLE furniture_fields (
                furniture_id INTEGER NOT NULL,
                slot INTEGER NOT NULL,
                role TEXT NOT NULL, label TEXT NOT NULL,
                key TEXT NOT NULL, format TEXT NOT NULL,
                value_json TEXT NOT NULL,
                PRIMARY KEY (furniture_id, slot),
                FOREIGN KEY (furniture_id) REFERENCES furniture(id) ON DELETE CASCADE
            );
            "#,
        )?;
        Ok(())
    }

    fn table_exists(&self, name: &str) -> DbResult<bool> {
        let found: Option<String> = self
            .conn
            .query_row(
                "SELECT name FROM sqlite_master WHERE type='table' AND name=?",
                params![name],
                |row| row.get(0),
            )
            .ok();
        Ok(found.is_some())
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

    fn read_property_rows(&self, table: &str) -> DbResult<HashMap<i64, Vec<(String, String)>>> {
        let mut out: HashMap<i64, Vec<(String, String)>> = HashMap::new();
        if !self.table_exists(table)? {
            return Ok(out);
        }
        let sql = format!("SELECT entity_id, key, value_json FROM {table}");
        let mut stmt = self.conn.prepare(&sql)?;
        let rows = stmt.query_map([], |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
            ))
        })?;
        for row in rows {
            let (id, key, encoded) = row.map_err(DbError::from)?;
            out.entry(id).or_default().push((key, encoded));
        }
        Ok(out)
    }

    /// Get all entities with properties. v1 files are read through the same
    /// compatibility path as the C++ loader (integer type + `cadastre_parcels`
    /// columns become generic properties).
    pub fn entities(&self) -> DbResult<Vec<EntityRecord>> {
        let version = self.schema_version()?;
        if version <= 1 {
            return self.legacy_entities();
        }

        let rows = self.read_property_rows("entity_properties")?;
        let mut stmt = self.conn.prepare(
            "SELECT id, type_id, layer, has_color_override, color_r, color_g, color_b, params FROM entities",
        )?;
        let entities = stmt.query_map([], |row| {
            let id: i64 = row.get(0)?;
            let type_id: String = row.get(1)?;
            let layer: Option<String> = row.get(2)?;
            let override_flag: i32 = row.get(3)?;
            let params: Option<String> = row.get(7)?;
            let mut properties = PropertyMap::new();
            if let Some(pairs) = rows.get(&id) {
                for (key, encoded) in pairs {
                    if let Some(decoded) = value_json::decode(encoded) {
                        properties.insert(key.clone(), decoded.value);
                    }
                }
            }
            Ok(EntityRecord {
                id,
                type_id,
                layer: layer.unwrap_or_default(),
                color_override: if override_flag != 0 {
                    Some(Color {
                        r: row.get(4)?,
                        g: row.get(5)?,
                        b: row.get(6)?,
                        a: 1.0,
                    })
                } else {
                    None
                },
                params: params.unwrap_or_default(),
                properties,
            })
        })?;
        entities.map(|r| r.map_err(DbError::from)).collect()
    }

    /// v1 compatibility read: integer `type` becomes a `TypeId` string (with
    /// the `|` parcel disambiguation), and the six `cadastre_parcels` columns
    /// become generic `cadastre.*` string properties. Empty columns were not
    /// values and do not become empty properties.
    fn legacy_entities(&self) -> DbResult<Vec<EntityRecord>> {
        let mut parcels: HashMap<i64, Vec<(String, String)>> = HashMap::new();
        if self.table_exists("cadastre_parcels")? {
            let mut cols = String::from("entity_id");
            for column in legacy_v1::COLUMNS {
                cols.push_str(", \"");
                cols.push_str(column);
                cols.push('"');
            }
            let sql = format!("SELECT {cols} FROM cadastre_parcels");
            let mut stmt = self.conn.prepare(&sql)?;
            let rows = stmt.query_map([], |row| {
                let id: i64 = row.get(0)?;
                let mut pairs = Vec::new();
                for (i, column) in legacy_v1::COLUMNS.iter().enumerate() {
                    let value: Option<String> = row.get(i + 1)?;
                    if let Some(text) = value {
                        if !text.is_empty() {
                            let key = format!("{}{}", legacy_v1::KEY_PREFIX, column);
                            let encoded = value_json::encode(&PropertyValue::String(text), &[]);
                            pairs.push((key, encoded));
                        }
                    }
                }
                Ok((id, pairs))
            })?;
            for row in rows {
                let (id, pairs) = row.map_err(DbError::from)?;
                parcels.insert(id, pairs);
            }
        }

        let mut stmt = self.conn.prepare(
            "SELECT id, type, layer, has_color_override, color_r, color_g, color_b, params FROM entities",
        )?;
        let entities = stmt.query_map([], |row| {
            let id: i64 = row.get(0)?;
            let type_int: i64 = row.get(1)?;
            let params: Option<String> = row.get(7)?;
            let params = params.unwrap_or_default();
            let type_id = legacy_v1::type_id_of(type_int, &params).unwrap_or_default();
            let mut properties = PropertyMap::new();
            if let Some(pairs) = parcels.get(&id) {
                for (key, encoded) in pairs {
                    if let Some(decoded) = value_json::decode(encoded) {
                        properties.insert(key.clone(), decoded.value);
                    }
                }
            }
            let layer: Option<String> = row.get(2)?;
            let override_flag: i32 = row.get(3)?;
            Ok(EntityRecord {
                id,
                type_id: type_id.to_owned(),
                layer: layer.unwrap_or_default(),
                color_override: if override_flag != 0 {
                    Some(Color {
                        r: row.get(4)?,
                        g: row.get(5)?,
                        b: row.get(6)?,
                        a: 1.0,
                    })
                } else {
                    None
                },
                params,
                properties,
            })
        })?;
        entities
            .map(|r| r.map_err(DbError::from))
            .filter(|record| {
                // An integer the v1 format itself could not name has no place
                // anywhere, not even in v3 — same rule as the C++ migrator.
                record
                    .as_ref()
                    .map(|r| !r.type_id.is_empty())
                    .unwrap_or(true)
            })
            .collect()
    }

    /// Document-level attributes (`document_properties`, v3). A v2 file has no
    /// such table: absence is silence, not an error.
    pub fn document_properties(&self) -> DbResult<PropertyMap> {
        let mut properties = PropertyMap::new();
        if !self.table_exists("document_properties")? {
            return Ok(properties);
        }
        let mut stmt = self
            .conn
            .prepare("SELECT key, value_json FROM document_properties")?;
        let rows = stmt.query_map([], |row| {
            Ok((row.get::<_, String>(0)?, row.get::<_, String>(1)?))
        })?;
        for row in rows {
            let (key, encoded) = row.map_err(DbError::from)?;
            if let Some(decoded) = value_json::decode(&encoded) {
                properties.insert(key, decoded.value);
            }
        }
        Ok(properties)
    }

    /// Paper space (`sheets` and below, v3). Furniture natures are kept as-is,
    /// with no painter to understand them — the file-level UnknownEntity rule.
    pub fn sheets(&self) -> DbResult<Vec<SheetRecord>> {
        let mut sheets = Vec::new();
        if !self.table_exists("sheets")? {
            return Ok(sheets);
        }
        let mut stmt = self.conn.prepare(
            "SELECT id, title, format_token, orientation_token, margin_top, margin_bottom, margin_left, margin_right FROM sheets ORDER BY id",
        )?;
        let rows = stmt.query_map([], |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, String>(3)?,
                row.get::<_, f64>(4)?,
                row.get::<_, f64>(5)?,
                row.get::<_, f64>(6)?,
                row.get::<_, f64>(7)?,
            ))
        })?;
        for row in rows {
            let (id, title, format_token, orientation_token, top, bottom, left, right) =
                row.map_err(DbError::from)?;
            sheets.push(SheetRecord {
                id,
                title,
                format_token,
                orientation_token,
                margin_top: top,
                margin_bottom: bottom,
                margin_left: left,
                margin_right: right,
                views: self.sheet_views(id)?,
                furniture: self.sheet_furniture(id)?,
            });
        }
        Ok(sheets)
    }

    fn sheet_views(&self, sheet_id: i64) -> DbResult<Vec<SheetView>> {
        let mut views = Vec::new();
        if !self.table_exists("sheet_views")? {
            return Ok(views);
        }
        let mut stmt = self.conn.prepare(
            "SELECT src_minx, src_miny, src_maxx, src_maxy, scale, paper_x, paper_y, paper_w, paper_h FROM sheet_views WHERE sheet_id=? ORDER BY idx",
        )?;
        let rows = stmt.query_map(params![sheet_id], |row| {
            Ok(SheetView {
                src_minx: row.get(0)?,
                src_miny: row.get(1)?,
                src_maxx: row.get(2)?,
                src_maxy: row.get(3)?,
                scale: row.get(4)?,
                paper_x: row.get(5)?,
                paper_y: row.get(6)?,
                paper_w: row.get(7)?,
                paper_h: row.get(8)?,
            })
        })?;
        for row in rows {
            views.push(row.map_err(DbError::from)?);
        }
        Ok(views)
    }

    fn sheet_furniture(&self, sheet_id: i64) -> DbResult<Vec<FurnitureRecord>> {
        let mut furniture = Vec::new();
        if !self.table_exists("furniture")? {
            return Ok(furniture);
        }
        let mut stmt = self.conn.prepare(
            "SELECT id, nature, template_id, zone_x, zone_y, zone_w, zone_h FROM furniture WHERE sheet_id=? ORDER BY id",
        )?;
        let rows = stmt.query_map(params![sheet_id], |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, f64>(3)?,
                row.get::<_, f64>(4)?,
                row.get::<_, f64>(5)?,
                row.get::<_, f64>(6)?,
            ))
        })?;
        for row in rows {
            let (id, nature, template_id, x, y, w, h) = row.map_err(DbError::from)?;
            furniture.push(FurnitureRecord {
                id,
                nature,
                template_id,
                zone_x: x,
                zone_y: y,
                zone_w: w,
                zone_h: h,
                fields: self.furniture_fields(id)?,
            });
        }
        Ok(furniture)
    }

    fn furniture_fields(&self, furniture_id: i64) -> DbResult<Vec<FurnitureField>> {
        let mut fields = Vec::new();
        if !self.table_exists("furniture_fields")? {
            return Ok(fields);
        }
        let mut stmt = self.conn.prepare(
            "SELECT slot, role, label, key, format, value_json FROM furniture_fields WHERE furniture_id=? ORDER BY slot",
        )?;
        let rows = stmt.query_map(params![furniture_id], |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
                row.get::<_, String>(3)?,
                row.get::<_, String>(4)?,
                row.get::<_, String>(5)?,
            ))
        })?;
        for row in rows {
            let (slot, role, label, key, format, encoded) = row.map_err(DbError::from)?;
            let decoded = value_json::decode(&encoded).ok_or_else(|| {
                DbError::InvalidEntityData(format!("bad value_json in furniture field {label}"))
            })?;
            fields.push(FurnitureField {
                slot,
                role,
                label,
                key,
                format,
                value: decoded.value,
                enum_values: decoded.enum_values,
            });
        }
        Ok(fields)
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

    /// Insert an entity (real columns: `type_id` string, opaque `params`)
    pub fn insert_entity(&self, entity: &EntityRecord) -> DbResult<i64> {
        self.conn.execute(
            "INSERT INTO entities (id, type_id, layer, has_color_override, color_r, color_g, color_b, params) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
            params![
                entity.id,
                entity.type_id,
                entity.layer,
                entity.color_override.is_some() as i32,
                entity.color_override.as_ref().map(|c| c.r).unwrap_or(1.0),
                entity.color_override.as_ref().map(|c| c.g).unwrap_or(1.0),
                entity.color_override.as_ref().map(|c| c.b).unwrap_or(1.0),
                entity.params,
            ],
        )?;
        Ok(self.conn.last_insert_rowid())
    }

    /// Insert one entity property row (`value_json` grammar, enum domain kept)
    pub fn insert_entity_property(
        &self,
        entity_id: i64,
        key: &str,
        value: &PropertyValue,
        enum_values: &[String],
    ) -> DbResult<()> {
        let encoded = value_json::encode(value, enum_values);
        self.conn.execute(
            "INSERT INTO entity_properties (entity_id, key, value_json) VALUES (?1, ?2, ?3)",
            params![entity_id, key, encoded],
        )?;
        Ok(())
    }

    /// Insert one document property row
    pub fn insert_document_property(
        &self,
        key: &str,
        value: &PropertyValue,
        enum_values: &[String],
    ) -> DbResult<()> {
        let encoded = value_json::encode(value, enum_values);
        self.conn.execute(
            "INSERT INTO document_properties (key, value_json) VALUES (?1, ?2)",
            params![key, encoded],
        )?;
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

/// Entity record: real type id, opaque params, typed properties.
/// An unknown `type_id` is kept as-is (type + params + properties) and must be
/// signaled by the reader, never dropped.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EntityRecord {
    pub id: i64,
    pub type_id: String,
    pub layer: String,
    pub color_override: Option<Color>,
    pub params: String,
    pub properties: PropertyMap,
}

/// A paper-space sheet (v3)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SheetRecord {
    pub id: i64,
    pub title: String,
    pub format_token: String,
    pub orientation_token: String,
    pub margin_top: f64,
    pub margin_bottom: f64,
    pub margin_left: f64,
    pub margin_right: f64,
    pub views: Vec<SheetView>,
    pub furniture: Vec<FurnitureRecord>,
}

/// A sheet view: world source, explicit scale, paper placement
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SheetView {
    pub src_minx: f64,
    pub src_miny: f64,
    pub src_maxx: f64,
    pub src_maxy: f64,
    pub scale: f64,
    pub paper_x: f64,
    pub paper_y: f64,
    pub paper_w: f64,
    pub paper_h: f64,
}

/// A furniture piece: nature kept as-is, no painter to understand it
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FurnitureRecord {
    pub id: i64,
    pub nature: String,
    pub template_id: String,
    pub zone_x: f64,
    pub zone_y: f64,
    pub zone_w: f64,
    pub zone_h: f64,
    pub fields: Vec<FurnitureField>,
}

/// A declarative field with its typed value (enum domain kept)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FurnitureField {
    pub slot: i64,
    pub role: String,
    pub label: String,
    pub key: String,
    pub format: String,
    pub value: PropertyValue,
    pub enum_values: Vec<String>,
}

/// Bring a v1 or v2 file to v3 in place, atomically (all or nothing).
/// Idempotent on v3, refused on a future version.
/// A v2 carries no dossier attributes and no sheets: the v3 tables arrive
/// empty, the migration cannot invent them. A v1 goes through the same
/// integer-to-`type_id` translation and six-column folding as the reader.
pub fn migrate_to_v3(conn: &mut Connection) -> DbResult<()> {
    let version: i32 = conn.query_row("PRAGMA user_version", [], |row| row.get(0))?;
    if !(1..=3).contains(&version) {
        return Err(DbError::InvalidSchemaVersion(version));
    }
    if version >= 3 {
        return Ok(());
    }

    let tx = conn.transaction()?;
    if version <= 1 {
        tx.execute_batch(
            r#"
            CREATE TABLE entities_new (
                id INTEGER PRIMARY KEY, type_id TEXT, layer TEXT,
                has_color_override INTEGER, color_r REAL, color_g REAL, color_b REAL, params TEXT
            );
            "#,
        )?;
        tx.execute_batch(
            "INSERT INTO entities_new SELECT id, CASE type \
             WHEN 0 THEN 'bcad.Point' WHEN 1 THEN 'bcad.Line' WHEN 2 THEN 'bcad.Circle' \
             WHEN 3 THEN 'bcad.Arc' WHEN 4 THEN 'bcad.Polyline' WHEN 6 THEN 'bcad.Text' \
             WHEN 5 THEN CASE WHEN params IS NULL OR params NOT LIKE '%|%' THEN 'bcad.Polyline' \
             ELSE 'cadastre.parcel' END ELSE NULL END, \
             layer, has_color_override, color_r, color_g, color_b, params FROM entities;",
        )?;
        tx.execute_batch("DELETE FROM entities_new WHERE type_id IS NULL;")?;
        tx.execute_batch(
            r#"
            CREATE TABLE entity_properties_new (
                entity_id INTEGER NOT NULL, key TEXT NOT NULL, value_json TEXT NOT NULL,
                PRIMARY KEY (entity_id, key)
            );
            "#,
        )?;
        for column in legacy_v1::COLUMNS {
            let key = format!("{}{}", legacy_v1::KEY_PREFIX, column);
            let sql = format!(
                "INSERT INTO entity_properties_new SELECT entity_id, '{key}', \
                 '{{\"type\":\"string\",\"value\":\"' || replace(replace(replace(replace(replace(\"{column}\", \
                 '\\', '\\\\'), '\"', '\\\"'), char(10), '\\n'), char(13), '\\r'), char(9), '\\t') \
                 || '\"}}' FROM cadastre_parcels WHERE \"{column}\" IS NOT NULL AND \"{column}\" <> '';"
            );
            tx.execute_batch(&sql)?;
        }
        tx.execute_batch(
            r#"
            DROP TABLE IF EXISTS entity_properties;
            DROP TABLE entities;
            DROP TABLE IF EXISTS cadastre_parcels;
            ALTER TABLE entities_new RENAME TO entities;
            ALTER TABLE entity_properties_new RENAME TO entity_properties;
            CREATE TABLE entity_properties_final (
                entity_id INTEGER NOT NULL, key TEXT NOT NULL, value_json TEXT NOT NULL,
                PRIMARY KEY (entity_id, key),
                FOREIGN KEY (entity_id) REFERENCES entities(id) ON DELETE CASCADE
            );
            INSERT INTO entity_properties_final SELECT * FROM entity_properties;
            DROP TABLE entity_properties;
            ALTER TABLE entity_properties_final RENAME TO entity_properties;
            "#,
        )?;
    }
    tx.execute_batch(
        r#"
        CREATE TABLE IF NOT EXISTS document_properties (
            key TEXT PRIMARY KEY, value_json TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS sheets (
            id INTEGER PRIMARY KEY, title TEXT NOT NULL,
            format_token TEXT NOT NULL, orientation_token TEXT NOT NULL,
            margin_top REAL, margin_bottom REAL, margin_left REAL, margin_right REAL
        );
        CREATE TABLE IF NOT EXISTS sheet_views (
            sheet_id INTEGER NOT NULL, idx INTEGER NOT NULL,
            src_minx REAL, src_miny REAL, src_maxx REAL, src_maxy REAL, scale REAL,
            paper_x REAL, paper_y REAL, paper_w REAL, paper_h REAL,
            PRIMARY KEY (sheet_id, idx),
            FOREIGN KEY (sheet_id) REFERENCES sheets(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS furniture (
            id INTEGER PRIMARY KEY, sheet_id INTEGER NOT NULL,
            nature TEXT NOT NULL, template_id TEXT NOT NULL,
            zone_x REAL, zone_y REAL, zone_w REAL, zone_h REAL,
            FOREIGN KEY (sheet_id) REFERENCES sheets(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS furniture_fields (
            furniture_id INTEGER NOT NULL, slot INTEGER NOT NULL,
            role TEXT NOT NULL, label TEXT NOT NULL, key TEXT NOT NULL, format TEXT NOT NULL,
            value_json TEXT NOT NULL,
            PRIMARY KEY (furniture_id, slot),
            FOREIGN KEY (furniture_id) REFERENCES furniture(id) ON DELETE CASCADE
        );
        PRAGMA user_version = 3;
        "#,
    )?;
    tx.commit()?;
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
        assert_eq!(db.schema_version().unwrap(), 3);
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

        let entity = EntityRecord {
            id: 1,
            type_id: "bcad.Line".to_string(),
            layer: "0".to_string(),
            color_override: None,
            params: "0,0,10,5".to_string(),
            properties: PropertyMap::new(),
        };
        db.insert_entity(&entity).unwrap();
        db.insert_entity_property(1, "releve.longueur", &PropertyValue::Double(11.18), &[])
            .unwrap();
        db.insert_entity_property(
            1,
            "releve.statut",
            &PropertyValue::Enum(bcad_format::EnumValue {
                index: 1,
                label: "verifie".to_string(),
                values: vec!["pose".to_string(), "verifie".to_string()],
            }),
            &["pose".to_string(), "verifie".to_string()],
        )
        .unwrap();

        // Unknown type: kept with type, params and properties, never dropped.
        let unknown = EntityRecord {
            id: 2,
            type_id: "network.pipe".to_string(),
            layer: "0".to_string(),
            color_override: None,
            params: "DN200;PEHD|z=1.2".to_string(),
            properties: PropertyMap::new(),
        };
        db.insert_entity(&unknown).unwrap();
        db.insert_entity_property(
            2,
            "network.material",
            &PropertyValue::String("PEHD".to_string()),
            &[],
        )
        .unwrap();

        let entities = db.entities().unwrap();
        assert_eq!(entities.len(), 2);
        assert_eq!(entities[0].type_id, "bcad.Line");
        assert_eq!(entities[0].params, "0,0,10,5");
        assert_eq!(
            entities[0].properties.get("releve.longueur"),
            Some(&PropertyValue::Double(11.18))
        );
        assert_eq!(
            entities[1].type_id, "network.pipe",
            "unknown type kept, not dropped"
        );
        assert_eq!(entities[1].params, "DN200;PEHD|z=1.2");
        assert_eq!(
            entities[1].properties.get("network.material"),
            Some(&PropertyValue::String("PEHD".to_string()))
        );
    }

    #[test]
    fn test_dossier_and_sheets_roundtrip() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("test.bcad");
        let db = create_new(&path).unwrap();

        db.insert_document_property(
            "dossier.projet",
            &PropertyValue::String("Ecole Primaire Lome".to_string()),
            &[],
        )
        .unwrap();
        db.insert_document_property("dossier.feuillet", &PropertyValue::Int(3), &[])
            .unwrap();

        db.conn
            .execute(
                "INSERT INTO sheets VALUES (1,'A3 Paysage','A3','Paysage',12,12,10,10)",
                [],
            )
            .unwrap();
        db.conn
            .execute(
                "INSERT INTO sheet_views VALUES (1,0,0,0,300,200,500,20,20,380,250)",
                [],
            )
            .unwrap();
        db.conn
            .execute(
                "INSERT INTO furniture VALUES (1,1,'reseau.legende','reseau.legende.decret_2024',20,300,100,60)",
                [],
            )
            .unwrap();
        db.conn
            .execute(
                "INSERT INTO furniture_fields VALUES (1,0,'attribut','Exploitant','reseau.exploitant','',?)",
                params![value_json::encode(&PropertyValue::String("TDE".to_string()), &[])],
            )
            .unwrap();

        let dossier = db.document_properties().unwrap();
        assert_eq!(
            dossier.get("dossier.projet"),
            Some(&PropertyValue::String("Ecole Primaire Lome".to_string()))
        );
        assert_eq!(
            dossier.get("dossier.feuillet"),
            Some(&PropertyValue::Int(3))
        );

        let sheets = db.sheets().unwrap();
        assert_eq!(sheets.len(), 1);
        assert_eq!(sheets[0].title, "A3 Paysage");
        assert_eq!(sheets[0].format_token, "A3");
        assert_eq!(sheets[0].views.len(), 1);
        assert_eq!(sheets[0].views[0].scale, 500.0);
        assert_eq!(sheets[0].furniture.len(), 1);
        assert_eq!(sheets[0].furniture[0].nature, "reseau.legende");
        assert_eq!(sheets[0].furniture[0].fields.len(), 1);
        assert_eq!(
            sheets[0].furniture[0].fields[0].value,
            PropertyValue::String("TDE".to_string())
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

    // Preuves anti-dérive : les fixtures que le C++ écrit sont celles que
    // Rust lit. Si le schéma C++ bouge sans le Rust, ces tests cassent ici —
    // pas chez un opérateur avec un dossier réel.
    const FIXTURES: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/../../../tests/fixtures/bcad");

    #[test]
    fn test_reads_real_v2_fixture() {
        let db = open_readonly(format!("{FIXTURES}/reference_v2.bcad")).unwrap();
        assert_eq!(db.schema_version().unwrap(), 2);
        let entities = db.entities().unwrap();
        assert_eq!(entities.len(), 3);
        assert_eq!(entities[0].type_id, "bcad.Line");
        assert_eq!(entities[0].params, "0,0,10,5");
        assert_eq!(
            entities[0].properties.get("releve.longueur"),
            Some(&PropertyValue::Double(11.18))
        );
        // L'inconnue traverse : type, payload et propriétés.
        assert_eq!(entities[2].type_id, "network.pipe");
        assert_eq!(entities[2].params, "DN200;PEHD|z=1.2");
        assert_eq!(
            entities[2].properties.get("network.material"),
            Some(&PropertyValue::String("PEHD".to_string()))
        );
        // Un v2 ne porte ni dossier ni feuilles : absence silencieuse.
        assert!(db.document_properties().unwrap().is_empty());
        assert!(db.sheets().unwrap().is_empty());
    }

    #[test]
    fn test_reads_real_v1_fixture() {
        let db = open_readonly(format!("{FIXTURES}/legacy_v1.bcad")).unwrap();
        assert_eq!(db.schema_version().unwrap(), 1);
        let entities = db.entities().unwrap();
        assert_eq!(entities.len(), 5);
        // 5 sans '|' resté la polyligne qu'il a toujours étée.
        assert_eq!(entities[0].type_id, "bcad.Polyline");
        assert_eq!(
            entities[0].properties.get("cadastre.section"),
            Some(&PropertyValue::String("AB".to_string()))
        );
        // La parcelle garde son type réel et ses clés.
        let parcels: Vec<_> = entities
            .iter()
            .filter(|e| e.type_id == "cadastre.parcel")
            .collect();
        assert_eq!(parcels.len(), 1);
        assert!(parcels[0].params.contains('|'));
    }

    #[test]
    fn test_reads_real_v3_fixture() {
        let db = open_readonly(format!("{FIXTURES}/reference_v3.bcad")).unwrap();
        assert_eq!(db.schema_version().unwrap(), 3);
        assert_eq!(db.entities().unwrap().len(), 2);
        let dossier = db.document_properties().unwrap();
        assert_eq!(
            dossier.get("dossier.projet"),
            Some(&PropertyValue::String("Ecole Primaire Lome".to_string()))
        );
        let sheets = db.sheets().unwrap();
        assert_eq!(sheets.len(), 1);
        assert_eq!(sheets[0].title, "A3 Paysage");
        assert_eq!(sheets[0].views.len(), 1);
        assert_eq!(sheets[0].views[0].scale, 500.0);
        assert_eq!(sheets[0].furniture.len(), 1);
        assert_eq!(sheets[0].furniture[0].nature, "reseau.legende");
        assert_eq!(
            sheets[0].furniture[0].fields[0].value,
            PropertyValue::String("TDE".to_string())
        );
    }

    #[test]
    fn test_migrates_real_v2_to_v3() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("migrated.bcad");
        std::fs::write(
            &path,
            include_bytes!(concat!(
                env!("CARGO_MANIFEST_DIR"),
                "/../../../tests/fixtures/bcad/reference_v2.bcad"
            )),
        )
        .unwrap();
        let mut conn = Connection::open(&path).unwrap();
        migrate_to_v3(&mut conn).unwrap();
        migrate_to_v3(&mut conn).unwrap(); // idempotente
        drop(conn);
        let db = open_readonly(&path).unwrap();
        assert_eq!(db.schema_version().unwrap(), 3);
        let entities = db.entities().unwrap();
        assert_eq!(entities.len(), 3);
        assert_eq!(entities[2].type_id, "network.pipe");
    }
}
