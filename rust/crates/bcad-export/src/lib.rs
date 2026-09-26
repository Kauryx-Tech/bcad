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

/// Export database to GeoJSON
pub fn export_geojson(db: &Database, path: impl AsRef<Path>) -> ExportResult<()> {
    let entities = db.entities()?;
    let mut features = Vec::new();

    for entity in entities {
        if let Some(feature) = entity_to_feature(&entity) {
            features.push(feature);
        }
    }

    let collection = GeoJsonFeatureCollection {
        feature_type: "FeatureCollection".to_string(),
        features,
    };

    let file = std::fs::File::create(path)?;
    let mut writer = std::io::BufWriter::new(file);
    serde_json::to_writer_pretty(&mut writer, &collection)?;
    writer.flush()?;
    Ok(())
}

fn entity_to_feature(entity: &bcad_db::EntityRecord) -> Option<GeoJsonFeature> {
    use bcad_format::PropertyValue;

    let geometry = match entity.properties.get("geometry") {
        Some(PropertyValue::String(s)) => {
            // If geometry is stored as JSON string
            serde_json::from_str(s).ok()?
        }
        _ => {
            // Try to reconstruct from entity type
            return None;
        }
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
    if let Some(handle) = &entity.handle {
        props.insert(
            "bcad_handle".to_string(),
            serde_json::Value::String(handle.clone()),
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
    let mut headers = vec!["id", "type_id", "layer", "handle"];
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
            entity.handle.clone().unwrap_or_default(),
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
            handle: Some("ABC".to_string()),
            color_override: None,
            properties: props,
        };

        // `entity_to_feature` reads geometry from a `geometry` property holding
        // a serialised `GeoJsonGeometry`. An entity without one yields `None`:
        // there is no reconstruction path from `type_id`.
        assert!(
            entity_to_feature(&entity).is_none(),
            "an entity carrying no geometry must not produce a feature"
        );

        let mut with_geometry = entity.clone();
        with_geometry.properties.insert(
            "geometry".to_string(),
            PropertyValue::String(r#"{"type":"Point","coordinates":[1.5,2.5]}"#.to_string()),
        );

        let feature = entity_to_feature(&with_geometry)
            .expect("a serialised GeoJsonGeometry must round-trip into a feature");
        assert!(matches!(
            feature.geometry,
            GeoJsonGeometry::Point([1.5, 2.5])
        ));
        assert_eq!(feature.properties["bcad_layer"], serde_json::json!("0"));
        assert_eq!(
            feature.properties["bcad_type"],
            serde_json::json!("test:type")
        );
        assert_eq!(feature.properties["bcad_handle"], serde_json::json!("ABC"));

        // A `geometry` property that is not valid GeoJSON is dropped, not
        // panicked on: `serde_json::from_str(..).ok()?` is the contract.
        let mut broken = entity.clone();
        broken.properties.insert(
            "geometry".to_string(),
            PropertyValue::String("{ not json".to_string()),
        );
        assert!(entity_to_feature(&broken).is_none());
    }
}
