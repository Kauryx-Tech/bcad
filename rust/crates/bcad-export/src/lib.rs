//! BCAD Export
//!
//! Exporters for GeoJSON, CSV, and other formats.

use bcad_db::Database;
use bcad_format::*;
use serde::{Deserialize, Serialize};
use std::io::Write;
use std::path::Path;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum ExportError {
    #[error("Database error: {0}")]
    Database(#[from] bcad_db::DbError),

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("JSON serialization error: {0}")]
    Json(#[from] serde_json::Error),

    #[error("CSV error: {0}")]
    Csv(#[from] csv::Error),

    #[error("Invalid geometry: {0}")]
    InvalidGeometry(String),

    #[error("DXF write error: {0}")]
    DxfWrite(String),
}

pub type ExportResult<T> = Result<T, ExportError>;

/// GeoJSON Feature
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeoJsonFeature {
    #[serde(rename = "type")]
    pub feature_type: String,
    pub geometry: GeoJsonGeometry,
    pub properties: serde_json::Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", content = "coordinates")]
pub enum GeoJsonGeometry {
    Point([f64; 2]),
    LineString(Vec<[f64; 2]>),
    Polygon(Vec<Vec<[f64; 2]>>),
    MultiLineString(Vec<Vec<[f64; 2]>>),
    MultiPolygon(Vec<Vec<Vec<[f64; 2]>>>),
}

/// GeoJSON FeatureCollection
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeoJsonFeatureCollection {
    #[serde(rename = "type")]
    pub feature_type: String,
    pub features: Vec<GeoJsonFeature>,
}

/// Export database to GeoJSON.
///
/// Geometry comes from the native `params` grammars
/// (`bcad_format::native_params`): a native entity whose `params` cannot be
/// decoded, and any non-native type, is skipped — counted by the caller via
/// the returned count, never invented. Returns the number of features written.
pub fn export_geojson(db: &Database, path: impl AsRef<Path>) -> ExportResult<usize> {
    let entities = db.entities()?;
    let mut features = Vec::new();

    for entity in entities {
        if let Some(feature) = entity_to_feature(&entity) {
            features.push(feature);
        }
    }
    let count = features.len();

    let collection = GeoJsonFeatureCollection {
        feature_type: "FeatureCollection".to_string(),
        features,
    };

    let file = std::fs::File::create(path)?;
    let mut writer = std::io::BufWriter::new(file);
    serde_json::to_writer_pretty(&mut writer, &collection)?;
    writer.flush()?;
    Ok(count)
}

fn entity_to_feature(entity: &bcad_db::EntityRecord) -> Option<GeoJsonFeature> {
    use bcad_format::native_params::{decode, NativeGeometry};

    let native = decode(&entity.type_id, &entity.params)?;
    let geometry = match &native {
        NativeGeometry::Point { position } => GeoJsonGeometry::Point([position[0], position[1]]),
        NativeGeometry::Line { start, end } => {
            GeoJsonGeometry::LineString(vec![[start[0], start[1]], [end[0], end[1]]])
        }
        NativeGeometry::Circle { center, .. } => GeoJsonGeometry::Point([center[0], center[1]]),
        NativeGeometry::Arc {
            center,
            radius,
            start_angle,
            end_angle,
        } => {
            // An arc is not a GeoJSON primitive: sample it. 64 segments max,
            // fewer when the sweep is small — a full circle still closes.
            let sweep = (end_angle - start_angle).abs().max(0.05);
            let steps = ((sweep / (2.0 * std::f64::consts::PI) * 64.0).ceil() as usize).max(2);
            let points: Vec<[f64; 2]> = (0..=steps)
                .map(|i| {
                    let a = start_angle + sweep * i as f64 / steps as f64;
                    [center[0] + radius * a.cos(), center[1] + radius * a.sin()]
                })
                .collect();
            GeoJsonGeometry::LineString(points)
        }
        NativeGeometry::Polyline { closed, vertices } => {
            let ring: Vec<[f64; 2]> = vertices.to_vec();
            if *closed {
                let mut closed_ring = ring;
                if let Some(first) = closed_ring.first().copied() {
                    closed_ring.push(first);
                }
                GeoJsonGeometry::Polygon(vec![closed_ring])
            } else {
                GeoJsonGeometry::LineString(ring)
            }
        }
        NativeGeometry::Text { position, .. } => GeoJsonGeometry::Point([position[0], position[1]]),
    };

    let mut props = serde_json::Map::new();
    props.insert(
        "bcad_id".to_string(),
        serde_json::Value::Number(entity.id.into()),
    );
    props.insert(
        "bcad_type".to_string(),
        serde_json::Value::String(entity.type_id.clone()),
    );
    props.insert(
        "bcad_layer".to_string(),
        serde_json::Value::String(entity.layer.clone()),
    );
    // GeoJSON has no circle primitive: the point above is the center, and the
    // radius travels as a property so the export does not lie about geometry.
    if let NativeGeometry::Circle { radius, .. } = native {
        props.insert(
            "bcad_radius".to_string(),
            serde_json::Value::Number(
                serde_json::Number::from_f64(radius).unwrap_or(serde_json::Number::from(0)),
            ),
        );
    }

    // Add all properties
    for (key, value) in &entity.properties.values {
        props.insert(key.clone(), property_value_to_json(value));
    }

    Some(GeoJsonFeature {
        feature_type: "Feature".to_string(),
        geometry,
        properties: serde_json::Value::Object(props),
    })
}

fn property_value_to_json(value: &PropertyValue) -> serde_json::Value {
    match value {
        PropertyValue::Double(v) => serde_json::Value::Number(
            serde_json::Number::from_f64(*v).unwrap_or(serde_json::Number::from(0)),
        ),
        PropertyValue::Int(v) => serde_json::Value::Number((*v).into()),
        PropertyValue::String(v) => serde_json::Value::String(v.clone()),
        PropertyValue::Bool(v) => serde_json::Value::Bool(*v),
        PropertyValue::Color(c) => serde_json::json!({
            "r": c.r, "g": c.g, "b": c.b, "a": c.a
        }),
        PropertyValue::Enum(e) => serde_json::json!({
            "index": e.index,
            "label": e.label
        }),
    }
}

/// Export entities to CSV
pub fn export_csv(
    db: &Database,
    path: impl AsRef<Path>,
    include_geometry: bool,
) -> ExportResult<()> {
    let entities = db.entities()?;
    let mut writer = csv::Writer::from_path(path)?;

    // Write header
    let mut headers = vec!["id", "type_id", "layer"];
    if include_geometry {
        headers.push("geometry");
    }
    // Add property keys from first entity
    let mut prop_keys: Vec<String> = Vec::new();
    if let Some(first) = entities.first() {
        for key in first.properties.keys() {
            prop_keys.push(key.clone());
        }
    }
    headers.extend(prop_keys.iter().map(|s| s.as_str()));

    writer.write_record(&headers)?;

    for entity in entities {
        let mut record = vec![
            entity.id.to_string(),
            entity.type_id.clone(),
            entity.layer.clone(),
        ];
        if include_geometry {
            record.push("".to_string()); // Placeholder
        }
        for key in &prop_keys {
            if let Some(value) = entity.properties.get(key) {
                record.push(property_value_to_csv_string(value));
            } else {
                record.push("".to_string());
            }
        }
        writer.write_record(&record)?;
    }
    writer.flush()?;
    Ok(())
}

fn property_value_to_csv_string(value: &PropertyValue) -> String {
    match value {
        PropertyValue::Double(v) => v.to_string(),
        PropertyValue::Int(v) => v.to_string(),
        PropertyValue::String(v) => v.clone(),
        PropertyValue::Bool(v) => v.to_string(),
        PropertyValue::Color(c) => format!("rgba({},{},{},{})", c.r, c.g, c.b, c.a),
        PropertyValue::Enum(e) => format!("{} ({})", e.label, e.index),
    }
}

/// Export entity properties to simple key-value CSV
pub fn export_properties_csv(db: &Database, path: impl AsRef<Path>) -> ExportResult<()> {
    let entities = db.entities()?;
    let mut writer = csv::Writer::from_path(path)?;

    writer.write_record([
        "entity_id",
        "type_id",
        "layer",
        "property_key",
        "property_type",
        "property_value",
    ])?;

    for entity in entities {
        for (key, value) in &entity.properties.values {
            let (type_str, value_str) = match value {
                PropertyValue::Double(v) => ("double".to_string(), v.to_string()),
                PropertyValue::Int(v) => ("int".to_string(), v.to_string()),
                PropertyValue::String(v) => ("string".to_string(), v.clone()),
                PropertyValue::Bool(v) => ("bool".to_string(), v.to_string()),
                PropertyValue::Color(c) => (
                    "color".to_string(),
                    format!("rgba({},{},{},{})", c.r, c.g, c.b, c.a),
                ),
                PropertyValue::Enum(e) => {
                    ("enum".to_string(), format!("{} ({})", e.label, e.index))
                }
            };
            writer.write_record(&[
                entity.id.to_string(),
                entity.type_id.clone(),
                entity.layer.clone(),
                key.clone(),
                type_str,
                value_str,
            ])?;
        }
    }
    writer.flush()?;
    Ok(())
}

/// DXF export module
pub mod dxf {
    use super::*;
    use bcad_format::{ParsedDocument, ParsedEntity, ParsedEntityType, ParsedLayer, PropertyValue};
    use std::fs::File;
    use std::io::{BufWriter, Write};

    const PROPS_APP_ID: &str = "BCAD_PROPS";

    /// Writes a DXF file from a ParsedDocument
    pub fn write_dxf(doc: &ParsedDocument, path: impl AsRef<Path>) -> ExportResult<()> {
        let file = File::create(path)?;
        let mut writer = BufWriter::new(file);
        write_dxf_internal(&mut writer, doc)?;
        writer.flush()?;
        Ok(())
    }

    /// Writes a DXF to a writer
    fn write_dxf_internal<W: Write>(w: &mut W, doc: &ParsedDocument) -> ExportResult<()> {
        // HEADER section
        writeln!(w, "0")?;
        writeln!(w, "SECTION")?;
        writeln!(w, "2")?;
        writeln!(w, "HEADER")?;
        writeln!(w, "9")?;
        writeln!(w, "$ACADVER")?;
        writeln!(w, "1")?;
        writeln!(w, "AC1015")?;
        writeln!(w, "0")?;
        writeln!(w, "ENDSEC")?;

        // TABLES section (LAYER + APPID)
        writeln!(w, "0")?;
        writeln!(w, "SECTION")?;
        writeln!(w, "2")?;
        writeln!(w, "TABLES")?;

        // LAYER table
        writeln!(w, "0")?;
        writeln!(w, "TABLE")?;
        writeln!(w, "2")?;
        writeln!(w, "LAYER")?;
        writeln!(w, "70")?;
        writeln!(w, "{}", doc.layers.len())?;

        for layer in &doc.layers {
            write_layer(w, layer)?;
        }
        writeln!(w, "0")?;
        writeln!(w, "ENDTAB")?;

        // APPID table
        writeln!(w, "0")?;
        writeln!(w, "TABLE")?;
        writeln!(w, "2")?;
        writeln!(w, "APPID")?;
        writeln!(w, "70")?;
        writeln!(w, "1")?;
        writeln!(w, "0")?;
        writeln!(w, "APPID")?;
        writeln!(w, "2")?;
        writeln!(w, "{}", PROPS_APP_ID)?;
        writeln!(w, "70")?;
        writeln!(w, "0")?;
        writeln!(w, "0")?;
        writeln!(w, "ENDTAB")?;

        writeln!(w, "0")?;
        writeln!(w, "ENDSEC")?;

        // ENTITIES section
        writeln!(w, "0")?;
        writeln!(w, "SECTION")?;
        writeln!(w, "2")?;
        writeln!(w, "ENTITIES")?;

        for entity in &doc.entities {
            write_entity(w, entity)?;
        }

        writeln!(w, "0")?;
        writeln!(w, "ENDSEC")?;

        // EOF
        writeln!(w, "0")?;
        writeln!(w, "EOF")?;

        Ok(())
    }

    fn write_layer<W: Write>(w: &mut W, layer: &ParsedLayer) -> ExportResult<()> {
        writeln!(w, "0")?;
        writeln!(w, "LAYER")?;
        writeln!(w, "2")?;
        writeln!(w, "{}", layer.name)?;

        // 70: layer flags (1 = frozen, 4 = locked)
        let mut flags = 0;
        if layer.frozen {
            flags |= 1;
        }
        if layer.locked {
            flags |= 4;
        }
        writeln!(w, "70")?;
        writeln!(w, "{}", flags)?;

        // 62: color (ACI, 1-255, negative = off)
        let aci = color_to_aci(layer.color);
        let aci_signed = if layer.visible { aci } else { -aci };
        writeln!(w, "62")?;
        writeln!(w, "{}", aci_signed)?;

        // 6: line type name
        writeln!(w, "6")?;
        writeln!(w, "{}", layer.line_type)?;

        Ok(())
    }

    fn color_to_aci(color: i32) -> i32 {
        // color is an ACI index (1-255), 0 = ByBlock, 256 = ByLayer
        // Just clamp to valid range
        color.clamp(1, 255)
    }

    fn write_entity<W: Write>(w: &mut W, entity: &ParsedEntity) -> ExportResult<()> {
        match &entity.entity_type {
            ParsedEntityType::Point { position } => {
                writeln!(w, "0")?;
                writeln!(w, "POINT")?;
                writeln!(w, "8")?;
                writeln!(w, "{}", entity.layer)?;
                writeln!(w, "10")?;
                writeln!(w, "{}", position[0])?;
                writeln!(w, "20")?;
                writeln!(w, "{}", position[1])?;
                writeln!(w, "30")?;
                writeln!(w, "{}", position[2])?;
            }
            ParsedEntityType::Line { start, end } => {
                writeln!(w, "0")?;
                writeln!(w, "LINE")?;
                writeln!(w, "8")?;
                writeln!(w, "{}", entity.layer)?;
                writeln!(w, "10")?;
                writeln!(w, "{}", start[0])?;
                writeln!(w, "20")?;
                writeln!(w, "{}", start[1])?;
                writeln!(w, "30")?;
                writeln!(w, "{}", start[2])?;
                writeln!(w, "11")?;
                writeln!(w, "{}", end[0])?;
                writeln!(w, "21")?;
                writeln!(w, "{}", end[1])?;
                writeln!(w, "31")?;
                writeln!(w, "{}", end[2])?;
            }
            ParsedEntityType::Circle { center, radius } => {
                writeln!(w, "0")?;
                writeln!(w, "CIRCLE")?;
                writeln!(w, "8")?;
                writeln!(w, "{}", entity.layer)?;
                writeln!(w, "10")?;
                writeln!(w, "{}", center[0])?;
                writeln!(w, "20")?;
                writeln!(w, "{}", center[1])?;
                writeln!(w, "30")?;
                writeln!(w, "{}", center[2])?;
                writeln!(w, "40")?;
                writeln!(w, "{}", radius)?;
            }
            ParsedEntityType::Arc {
                center,
                radius,
                start_angle_deg,
                end_angle_deg,
            } => {
                writeln!(w, "0")?;
                writeln!(w, "ARC")?;
                writeln!(w, "8")?;
                writeln!(w, "{}", entity.layer)?;
                writeln!(w, "10")?;
                writeln!(w, "{}", center[0])?;
                writeln!(w, "20")?;
                writeln!(w, "{}", center[1])?;
                writeln!(w, "30")?;
                writeln!(w, "{}", center[2])?;
                writeln!(w, "40")?;
                writeln!(w, "{}", radius)?;
                writeln!(w, "50")?;
                writeln!(w, "{}", start_angle_deg)?;
                writeln!(w, "51")?;
                writeln!(w, "{}", end_angle_deg)?;
            }
            ParsedEntityType::Polyline {
                vertices,
                closed,
                elevation,
            } => {
                // Use LWPOLYLINE for 2D/3D polylines
                writeln!(w, "0")?;
                writeln!(w, "LWPOLYLINE")?;
                writeln!(w, "8")?;
                writeln!(w, "{}", entity.layer)?;
                writeln!(w, "70")?;
                writeln!(w, "{}", if *closed { 1 } else { 0 })?;
                writeln!(w, "90")?;
                writeln!(w, "{}", vertices.len())?;

                // Elevation (38) - only if non-zero
                if *elevation != 0.0 {
                    writeln!(w, "38")?;
                    writeln!(w, "{}", elevation)?;
                }

                for v in vertices {
                    writeln!(w, "10")?;
                    writeln!(w, "{}", v[0])?;
                    writeln!(w, "20")?;
                    writeln!(w, "{}", v[1])?;
                    // 30 is per-vertex Z, but LWPOLYLINE uses elevation + 38 for Z
                    // So we don't write 30 here
                }
            }
            ParsedEntityType::Text {
                position,
                text,
                height,
                rotation_deg,
            } => {
                writeln!(w, "0")?;
                writeln!(w, "TEXT")?;
                writeln!(w, "8")?;
                writeln!(w, "{}", entity.layer)?;
                writeln!(w, "10")?;
                writeln!(w, "{}", position[0])?;
                writeln!(w, "20")?;
                writeln!(w, "{}", position[1])?;
                writeln!(w, "30")?;
                writeln!(w, "{}", position[2])?;
                writeln!(w, "40")?;
                writeln!(w, "{}", height)?;
                writeln!(w, "1")?;
                writeln!(w, "{}", text)?;
                writeln!(w, "50")?;
                writeln!(w, "{}", rotation_deg)?;
            }
            ParsedEntityType::Unknown {
                type_name,
                raw_groups,
            } => {
                // Write unknown entity verbatim from raw_groups
                // Find the entity type name from raw groups
                for g in raw_groups {
                    writeln!(w, "{}", g.code)?;
                    writeln!(w, "{}", g.value)?;
                }
                // If no raw groups, fall back to type_name
                if raw_groups.is_empty() {
                    writeln!(w, "0")?;
                    writeln!(w, "{}", type_name)?;
                    writeln!(w, "8")?;
                    writeln!(w, "{}", entity.layer)?;
                }
            }
        }

        // Write properties as XDATA (BCAD_PROPS)
        write_properties_xdata(w, entity)?;

        Ok(())
    }

    fn write_properties_xdata<W: Write>(w: &mut W, entity: &ParsedEntity) -> ExportResult<()> {
        let props = &entity.properties;
        let names = props.keys().collect::<Vec<_>>();
        if names.is_empty() {
            return Ok(());
        }

        writeln!(w, "1001")?;
        writeln!(w, "{}", PROPS_APP_ID)?;
        writeln!(w, "1002")?;
        writeln!(w, "{{")?;

        for name in names {
            if let Some(value) = props.get(name) {
                writeln!(w, "1000")?;
                writeln!(w, "{}", escape_xdata_string(name))?;

                let type_str = property_type_to_str(value);
                writeln!(w, "1000")?;
                writeln!(w, "{}", type_str)?;

                let value_str = property_value_to_str(value);
                writeln!(w, "1000")?;
                writeln!(w, "{}", escape_xdata_string(&value_str))?;
            }
        }

        writeln!(w, "1002")?;
        writeln!(w, "}}")?;

        Ok(())
    }

    fn escape_xdata_string(s: &str) -> String {
        s.replace(['\n', '\r'], " ")
    }

    fn property_type_to_str(value: &PropertyValue) -> String {
        match value {
            PropertyValue::Double(_) => "double",
            PropertyValue::Int(_) => "int",
            PropertyValue::String(_) => "string",
            PropertyValue::Bool(_) => "bool",
            PropertyValue::Color(_) => "color",
            PropertyValue::Enum(_) => "string", // enums as label
        }
        .to_string()
    }

    fn property_value_to_str(value: &PropertyValue) -> String {
        match value {
            PropertyValue::Double(v) => format!("{:.17}", v),
            PropertyValue::Int(v) => v.to_string(),
            PropertyValue::String(v) => v.clone(),
            PropertyValue::Bool(v) => if *v { "true" } else { "false" }.to_string(),
            PropertyValue::Color(c) => format!("{} {} {} {}", c.r, c.g, c.b, c.a),
            PropertyValue::Enum(e) => e.label.clone(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_entity_to_feature() {
        use bcad_db::Layer;
        use bcad_format::{Color, PropertyMap, PropertyValue};

        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("test.bcad");
        let db = bcad_db::create_new(&path).unwrap();

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
        props.insert("test_prop", PropertyValue::String("test".to_string()));

        let entity = bcad_db::EntityRecord {
            id: 1,
            type_id: "test:type".to_string(),
            layer: "0".to_string(),
            color_override: None,
            params: String::new(),
            properties: props,
        };

        // Geometry comes from native `params`, not from a `geometry` property:
        // an unknown type yields `None` and is skipped, never invented.
        assert!(
            entity_to_feature(&entity).is_none(),
            "an entity without native geometry must not produce a feature"
        );

        let mut native = entity.clone();
        native.type_id = "bcad.Point".to_string();
        native.params = "1.5,2.5".to_string();

        let feature = entity_to_feature(&native).expect("native params must decode");
        assert!(matches!(
            feature.geometry,
            GeoJsonGeometry::Point([1.5, 2.5])
        ));
        assert_eq!(feature.properties["bcad_layer"], serde_json::json!("0"));
        assert_eq!(
            feature.properties["bcad_type"],
            serde_json::json!("bcad.Point")
        );

        // Unreadable `params` are dropped, not panicked on.
        let mut broken = entity.clone();
        broken.type_id = "bcad.Line".to_string();
        broken.params = "0,0".to_string();
        assert!(entity_to_feature(&broken).is_none());
    }
}
