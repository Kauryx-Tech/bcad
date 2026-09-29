//! BCAD format-neutral types
//!
//! This crate contains the core data structures used across BCAD Rust components:
//! - Diagnostic types for structured error reporting
//! - Format versioning
//! - Identifier types
//! - Safe numeric conversions
//! - Property value variants (equivalent to C++ `PropertyValue`)

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use thiserror::Error;

/// Severity level for diagnostics
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Severity {
    Info,
    Warning,
    Error,
}

/// Structured diagnostic entry (matches C++ `PropertyChanged` / validation events)
#[derive(PartialEq, Debug, Clone, Serialize, Deserialize)]
pub struct Diagnostic {
    pub severity: Severity,
    pub code: String,
    pub message: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub file: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub entity_id: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub geometry: Option<GeometryRef>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub suggestion: Option<String>,
    #[serde(skip_serializing_if = "HashMap::is_empty", default)]
    pub context: HashMap<String, serde_json::Value>,
}

impl Diagnostic {
    pub fn error(code: impl Into<String>, message: impl Into<String>) -> Self {
        Self {
            severity: Severity::Error,
            code: code.into(),
            message: message.into(),
            file: None,
            entity_id: None,
            geometry: None,
            suggestion: None,
            context: HashMap::new(),
        }
    }

    pub fn warning(code: impl Into<String>, message: impl Into<String>) -> Self {
        Self {
            severity: Severity::Warning,
            code: code.into(),
            message: message.into(),
            file: None,
            entity_id: None,
            geometry: None,
            suggestion: None,
            context: HashMap::new(),
        }
    }

    pub fn info(code: impl Into<String>, message: impl Into<String>) -> Self {
        Self {
            severity: Severity::Info,
            code: code.into(),
            message: message.into(),
            file: None,
            entity_id: None,
            geometry: None,
            suggestion: None,
            context: HashMap::new(),
        }
    }

    #[must_use]
    pub fn with_file(mut self, file: impl Into<String>) -> Self {
        self.file = Some(file.into());
        self
    }

    #[must_use]
    pub const fn with_entity(mut self, entity_id: u64) -> Self {
        self.entity_id = Some(entity_id);
        self
    }

    #[must_use]
    pub fn with_geometry(mut self, geometry: GeometryRef) -> Self {
        self.geometry = Some(geometry);
        self
    }

    #[must_use]
    pub fn with_suggestion(mut self, suggestion: impl Into<String>) -> Self {
        self.suggestion = Some(suggestion.into());
        self
    }

    #[must_use]
    pub fn with_context(
        mut self,
        key: impl Into<String>,
        value: impl Into<serde_json::Value>,
    ) -> Self {
        self.context.insert(key.into(), value.into());
        self
    }
}

/// Reference to geometry for diagnostic context
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum GeometryRef {
    Point {
        x: f64,
        y: f64,
    },
    Line {
        x1: f64,
        y1: f64,
        x2: f64,
        y2: f64,
    },
    Polyline {
        points: Vec<[f64; 2]>,
        closed: bool,
    },
    Circle {
        cx: f64,
        cy: f64,
        r: f64,
    },
    Arc {
        cx: f64,
        cy: f64,
        r: f64,
        start: f64,
        end: f64,
    },
    BoundingBox {
        min_x: f64,
        min_y: f64,
        max_x: f64,
        max_y: f64,
    },
}

/// Format version (matches C++ schema versions)
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u32)]
pub enum FormatVersion {
    V1 = 1,
    V2 = 2,
    V3 = 3,
}

impl FormatVersion {
    pub const CURRENT: Self = Self::V3;
    pub const MIN_SUPPORTED: Self = Self::V1;
    pub const MAX_SUPPORTED: Self = Self::V3;

    #[must_use]
    pub const fn from_u32(v: u32) -> Option<Self> {
        match v {
            1 => Some(Self::V1),
            2 => Some(Self::V2),
            3 => Some(Self::V3),
            _ => None,
        }
    }

    #[must_use]
    pub const fn is_supported(self) -> bool {
        matches!(self, Self::V1 | Self::V2 | Self::V3)
    }
}

/// Stable entity type identifier (matches C++ `TypeId`)
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct TypeId(pub String);

impl TypeId {
    pub fn new(s: impl Into<String>) -> Self {
        Self(s.into())
    }

    #[must_use]
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

/// Native BCAD type IDs
pub mod native_types {
    use super::TypeId;
    #[must_use]
    pub fn point() -> TypeId {
        TypeId::new("bcad.Point")
    }
    #[must_use]
    pub fn line() -> TypeId {
        TypeId::new("bcad.Line")
    }
    #[must_use]
    pub fn circle() -> TypeId {
        TypeId::new("bcad.Circle")
    }
    #[must_use]
    pub fn arc() -> TypeId {
        TypeId::new("bcad.Arc")
    }
    #[must_use]
    pub fn polyline() -> TypeId {
        TypeId::new("bcad.Polyline")
    }
    #[must_use]
    pub fn text() -> TypeId {
        TypeId::new("bcad.Text")
    }
}

/// Property value variant (equivalent to C++ `PropertyValue`)
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type", content = "value")]
pub enum PropertyValue {
    Double(f64),
    Int(i64),
    String(String),
    Bool(bool),
    Color(Color),
    Enum(EnumValue),
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize, Deserialize)]
pub struct Color {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}

impl Color {
    #[must_use]
    pub fn from_rgb255(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self {
            r: f32::from(r) / 255.0,
            g: f32::from(g) / 255.0,
            b: f32::from(b) / 255.0,
            a: f32::from(a) / 255.0,
        }
    }

    pub const WHITE: Self = Self {
        r: 1.0,
        g: 1.0,
        b: 1.0,
        a: 1.0,
    };
    pub const BLACK: Self = Self {
        r: 0.0,
        g: 0.0,
        b: 0.0,
        a: 1.0,
    };
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct EnumValue {
    pub index: i32,
    pub label: String,
    /// Le domaine de l'enum, qui voyage avec la valeur (comme C++ `TypedValue`)
    /// : sans lui, un index relu d'un module absent serait hors domaine et
    /// donc perdu à la réécriture.
    #[serde(default)]
    pub values: Vec<String>,
}

/// Property type discriminant
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase")]
pub enum PropertyType {
    Double,
    Int,
    String,
    Bool,
    Color,
    Enum,
}

/// Property metadata (for UI generation)
#[derive(PartialEq, Debug, Clone, Serialize, Deserialize)]
pub struct PropertyMeta {
    pub name: String,
    pub prop_type: PropertyType,
    pub unit: Option<String>,
    pub description: Option<String>,
    pub read_only: bool,
    pub min: Option<f64>,
    pub max: Option<f64>,
    pub enum_values: Vec<String>,
}

/// Entity property map (serializable subset)
#[derive(PartialEq, Debug, Clone, Default, Serialize, Deserialize)]
pub struct PropertyMap {
    #[serde(flatten)]
    pub values: HashMap<String, PropertyValue>,
    #[serde(skip_serializing_if = "HashMap::is_empty", default)]
    pub meta: HashMap<String, PropertyMeta>,
}

impl PropertyMap {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn insert(&mut self, name: impl Into<String>, value: PropertyValue) {
        self.values.insert(name.into(), value);
    }

    #[must_use]
    pub fn get(&self, name: &str) -> Option<&PropertyValue> {
        self.values.get(name)
    }

    pub fn get_mut(&mut self, name: &str) -> Option<&mut PropertyValue> {
        self.values.get_mut(name)
    }

    pub fn remove(&mut self, name: &str) -> Option<PropertyValue> {
        self.values.remove(name)
    }

    pub fn set_meta(&mut self, name: impl Into<String>, meta: PropertyMeta) {
        self.meta.insert(name.into(), meta);
    }

    #[must_use]
    pub fn get_meta(&self, name: &str) -> Option<&PropertyMeta> {
        self.meta.get(name)
    }

    pub fn keys(&self) -> impl Iterator<Item = &String> {
        self.values.keys()
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.values.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.values.is_empty()
    }
}

/// Parsed layer information
///
/// Quatre drapeaux enthusiasts (on, locked, frozen, plot) pour une seule entite
/// sont un signal de regroupement. Un bitmask les remplacerait, mais cela
/// changerait la forme publique de `ParsedLayer` et donc le code de tous ses
/// consommateurs, `bcad-dxf` compris. Le regroupement est donc une decision
/// distincte, a traiter avec les crates aval et non dans un coup defrenforcement
/// de lint.
#[allow(clippy::struct_excessive_bools)]
#[derive(PartialEq, Debug, Clone, Serialize, Deserialize)]
pub struct ParsedLayer {
    pub name: String,
    /// ACI color index: 0 = `ByBlock`, 256 = `ByLayer`, 1..=255 = explicit index
    pub color: i32,
    /// Line weight in millimetres; negative means "not specified"
    pub line_weight: f64,
    pub visible: bool,
    pub locked: bool,
    pub frozen: bool,
    pub plot: bool,
    /// Line type name as written in the source table (DXF group code 6)
    pub line_type: String,
}

impl ParsedLayer {
    /// ACI index 7 (white) is the `AutoCAD` default when the source omits group 62.
    pub const DEFAULT_COLOR: i32 = 7;
    /// `AutoCAD` sentinel for "line weight not specified".
    pub const DEFAULT_LINE_WEIGHT: f64 = -1.0;
}

/// Parsed APPID entry
#[derive(PartialEq, Eq, Debug, Clone, Serialize, Deserialize)]
pub struct ParsedAppId {
    pub name: String,
}

/// Raw group as read from the source, retained verbatim for entities that this
/// crate does not interpret.
///
/// `line` is the 1-based source line of the group *code*, so diagnostics can
/// point back at the exact place the value came from.
#[derive(PartialEq, Eq, Debug, Clone, Serialize, Deserialize)]
pub struct RawGroup {
    pub code: i32,
    pub value: String,
    pub line: usize,
}

/// One XDATA item, attributed to its owning entity.
///
/// `line` is the 1-based source line of the group code, so a malformed XDATA
/// block can be reported at the exact location.
#[derive(PartialEq, Eq, Debug, Clone, Serialize, Deserialize)]
pub struct XDataRecord {
    pub code: i32,
    pub value: String,
    pub line: usize,
}

/// Format-agnostic parsed entity (for DXF, `GeoJSON`, etc.)
#[derive(PartialEq, Debug, Clone, Serialize, Deserialize)]
pub struct ParsedEntity {
    pub handle: Option<String>,
    pub layer: String,
    pub entity_type: ParsedEntityType,
    pub properties: PropertyMap,
    #[serde(default)]
    pub xdata: std::collections::BTreeMap<String, Vec<XDataRecord>>,
}

/// Canonical parsed geometry, expressed in 3D.
///
/// This is the single neutral geometry representation shared by every BCAD
/// format reader. It deliberately uses one spelling per concept and lifts no
/// source-format vocabulary:
///
/// - point coordinates are `[x, y, z]` triples, never separate scalars;
/// - `radius`, not `r`; `start_angle_deg` / `end_angle_deg`, not `start` / `end`;
/// - `vertices`, not `points`; one `Polyline` variant serves both classic
///   `POLYLINE` (VERTEX/SEQEND block) and `LWPOLYLINE` (inline vertices).
///
/// A 2D source entity is a 3D entity with `z = 0.0`; no separate 2D variant
/// exists, and no format-specific entity variant is introduced.
#[derive(PartialEq, Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", content = "data")]
pub enum ParsedEntityType {
    Point {
        position: [f64; 3],
    },
    Line {
        start: [f64; 3],
        end: [f64; 3],
    },
    Polyline {
        /// Vertex positions in 3D. The elevation of the source entity is
        /// already folded into each `z` component.
        vertices: Vec<[f64; 3]>,
        closed: bool,
        /// Retained for round-tripping; `vertices` already include it.
        elevation: f64,
    },
    Circle {
        center: [f64; 3],
        radius: f64,
    },
    Arc {
        center: [f64; 3],
        radius: f64,
        start_angle_deg: f64,
        end_angle_deg: f64,
    },
    Text {
        position: [f64; 3],
        text: String,
        height: f64,
        rotation_deg: f64,
    },
    /// A recognised-but-not-interpreted entity (MTEXT, INSERT, DIMENSION,
    /// HATCH, ...) or a completely unknown one. `raw_groups` keeps every group
    /// verbatim so a later pass can interpret it without re-reading the file.
    Unknown {
        type_name: String,
        raw_groups: Vec<RawGroup>,
    },
}

/// Complete parsed document (format-agnostic)
#[derive(PartialEq, Debug, Clone, Serialize, Deserialize)]
pub struct ParsedDocument {
    pub format_version: FormatVersion,
    pub layers: Vec<ParsedLayer>,
    pub entities: Vec<ParsedEntity>,
    pub app_ids: Vec<ParsedAppId>,
    #[serde(default)]
    pub diagnostics: Vec<Diagnostic>,
}

impl ParsedDocument {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            format_version: FormatVersion::CURRENT,
            layers: Vec::new(),
            entities: Vec::new(),
            app_ids: Vec::new(),
            diagnostics: Vec::new(),
        }
    }

    pub fn add_diagnostic(&mut self, diag: Diagnostic) {
        self.diagnostics.push(diag);
    }

    #[must_use]
    pub fn has_errors(&self) -> bool {
        self.diagnostics
            .iter()
            .any(|d| d.severity == Severity::Error)
    }

    #[must_use]
    pub fn errors(&self) -> Vec<&Diagnostic> {
        self.diagnostics
            .iter()
            .filter(|d| d.severity == Severity::Error)
            .collect()
    }

    #[must_use]
    pub fn warnings(&self) -> Vec<&Diagnostic> {
        self.diagnostics
            .iter()
            .filter(|d| d.severity == Severity::Warning)
            .collect()
    }
}

impl Default for ParsedDocument {
    fn default() -> Self {
        Self::new()
    }
}

/// Result type for parsing operations
pub type ParseResult<T> = Result<T, ParseError>;

#[derive(Debug, Error)]
pub enum ParseError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),

    #[error("Invalid format: {0}")]
    InvalidFormat(String),

    #[error("Unsupported format version: {0}")]
    UnsupportedVersion(u32),

    #[error("Missing required field: {0}")]
    MissingField(String),

    #[error("Invalid value: {0}")]
    InvalidValue(String),

    #[error("Encoding error: {0}")]
    Encoding(String),

    #[error("Resource limit exceeded: {0}")]
    ResourceLimit(String),

    #[error("Geometric validation failed: {0}")]
    GeometryValidation(String),

    #[error("Unknown entity type: {0}")]
    UnknownEntityType(String),

    #[error("Color conversion error: {0}")]
    ColorConversion(String),
}

/// Safe numeric conversion utilities
pub mod convert {
    use rust_decimal::Decimal;

    #[must_use]
    pub fn f64_to_decimal(v: f64) -> Option<Decimal> {
        Decimal::try_from(v).ok()
    }

    /// Exact `Decimal` to `f64` conversion.
    ///
    /// Returns `None` when the value is not representable. This never
    /// substitutes `NaN`: a caller that cannot represent the value must
    /// surface an error rather than propagate a poison float.
    #[must_use]
    pub fn decimal_to_f64(d: Decimal) -> Option<f64> {
        let v: f64 = d.try_into().ok()?;
        v.is_finite().then_some(v)
    }

    /// Widens an `i64` to `f64`.
    ///
    /// Lossy above `2^53`: `f64` carries a 52-bit mantissa, so integers beyond
    /// that cannot be represented exactly. This is inherent to the type pair and
    /// cannot be fixed here, only stated. DXF coordinates are `f64` end to end,
    /// so the conversion is lossless for every value the format can carry
    /// (32-bit integers), and the loss only appears for synthetic input.
    #[must_use]
    #[allow(clippy::cast_precision_loss)]
    pub const fn i64_to_f64(v: i64) -> f64 {
        v as f64
    }

    /// `f64` to `i64` conversion, rejecting non-finite and out-of-range values
    /// instead of saturating or truncating them silently.
    ///
    /// The bounds are `2^63` exclusive, not `i64::MAX`. `i64::MAX as f64` rounds
    /// *up* to `2^63`, so a guard written as `v <= i64::MAX as f64` lets `2^63`
    /// through and `v as i64` then saturates to `i64::MAX` without any error.
    /// Every `f64` in `[-2^53, 2^53)` is exactly representable; above that the
    /// conversion truncates toward zero, which is the documented behaviour of
    /// `as` and is what a coordinate read needs.
    ///
    /// # Errors
    ///
    /// Returns an error string when `v` is not finite or lies outside
    /// `[-2^63, 2^63)`.
    #[allow(clippy::cast_possible_truncation)]
    pub fn f64_to_i64(v: f64) -> Result<i64, String> {
        // 2^63, the first `f64` that no longer fits an `i64`. Spelled as a power
        // of two so it is exact, unlike `i64::MAX as f64` which is not.
        const I64_MIN_EXCLUSIVE: f64 = -9_223_372_036_854_775_808.0;
        const I64_MAX_EXCLUSIVE: f64 = 9_223_372_036_854_775_808.0;
        if v.is_finite() && (I64_MIN_EXCLUSIVE..I64_MAX_EXCLUSIVE).contains(&v) {
            #[allow(clippy::cast_possible_truncation)]
            Ok(v as i64)
        } else {
            Err("f64 value cannot be converted to i64".to_string())
        }
    }

    /// Widens a `u32` to `usize`, lossless on every platform Rust supports
    /// (`usize` is at least 16 bits, and a 16-bit target cannot hold a `u32`).
    #[must_use]
    pub const fn u32_to_usize(v: u32) -> usize {
        v as usize
    }

    #[must_use]
    pub fn usize_to_u32(v: usize) -> Option<u32> {
        v.try_into().ok()
    }

    /// Clamps to the valid `f32` range for a colour component.
    ///
    /// After clamping to `[0, 1]` the value cannot overflow `f32`; only its
    /// 53-bit mantissa is narrowed to 24 bits, which is the precision the format
    /// carries anyway.
    #[must_use]
    #[allow(clippy::cast_possible_truncation)]
    pub const fn clamp_color_channel(v: f64) -> f32 {
        v.clamp(0.0, 1.0) as f32
    }

    /// Parse decimal string with high precision.
    ///
    /// # Errors
    ///
    /// Returns the underlying [`rust_decimal::Error`] when `s` is not a decimal
    /// this crate can represent, naming the offending input.
    pub fn parse_decimal(s: &str) -> Result<Decimal, rust_decimal::Error> {
        s.parse()
    }
}

/// Le codage `value_json` d'une valeur typée.
///
/// Grammaire exacte de C++ `src/io/ValueJson.h` :
/// `{"type":"double","value":11.18}`. Le type est une donnée de la ligne, pas
/// une supposition du lecteur — une seule grammaire, un seul lecteur, des deux
/// côtés de la frontière.
pub mod value_json {
    use super::{Color, EnumValue, PropertyValue};

    /// Un canal couleur JSON (f64) vers f32.
    ///
    /// Le `as` rétrécit comme le `static_cast<float>` du lecteur C++ : même
    /// valeur stockée des deux côtés, pas deux arrondis différents.
    #[allow(clippy::cast_possible_truncation)]
    fn channel(component: &serde_json::Value) -> Option<f32> {
        component.as_f64().map(|v| v as f32)
    }

    /// Décode une ligne `value_json`. `None` = donnée invalide, pas un cas à
    /// deviner (même règle que le lecteur C++ strict).
    #[must_use]
    pub fn decode(text: &str) -> Option<DecodedValue> {
        let root: serde_json::Value = serde_json::from_str(text).ok()?;
        let tag = root.get("type")?.as_str()?;
        let value = root.get("value")?;
        let mut out = DecodedValue {
            prop_type: super::PropertyType::String,
            value: PropertyValue::String(String::new()),
            enum_values: Vec::new(),
        };
        match tag {
            "double" => {
                out.prop_type = super::PropertyType::Double;
                out.value = PropertyValue::Double(value.as_f64()?);
            }
            "int" => {
                out.prop_type = super::PropertyType::Int;
                out.value = PropertyValue::Int(value.as_i64()?);
            }
            "bool" => {
                out.prop_type = super::PropertyType::Bool;
                out.value = PropertyValue::Bool(value.as_bool()?);
            }
            "string" => {
                out.prop_type = super::PropertyType::String;
                out.value = PropertyValue::String(value.as_str()?.to_owned());
            }
            "color" => {
                out.prop_type = super::PropertyType::Color;
                out.value = PropertyValue::Color(Color {
                    r: channel(value.get("r")?)?,
                    g: channel(value.get("g")?)?,
                    b: channel(value.get("b")?)?,
                    a: channel(value.get("a")?)?,
                });
            }
            "enum" => {
                out.prop_type = super::PropertyType::Enum;
                // Un index hors `int` n'est pas une valeur : C++ le stocke en
                // `int`, un 64 bits ne passerait pas la réécriture.
                let index = i32::try_from(value.as_i64()?).ok()?;
                let domain: Vec<String> = root
                    .get("values")?
                    .as_array()?
                    .iter()
                    .filter_map(serde_json::Value::as_str)
                    .map(str::to_owned)
                    .collect();
                let label = domain
                    .get(usize::try_from(index).ok()?)
                    .cloned()
                    .unwrap_or_default();
                let enum_value = EnumValue {
                    index,
                    label,
                    values: domain,
                };
                out.enum_values.clone_from(&enum_value.values);
                out.value = PropertyValue::Enum(enum_value);
            }
            _ => return None,
        }
        Some(out)
    }

    /// Encode une valeur dans la grammaire C++. Le domaine d'un enum voyage
    /// avec lui (`values`) : c'est la seule façon de réinjecter un index sans
    /// le mettre hors domaine.
    #[must_use]
    pub fn encode(value: &PropertyValue, enum_values: &[String]) -> String {
        match value {
            PropertyValue::Double(v) => {
                format!(r#"{{"type":"double","value":{v}}}"#)
            }
            PropertyValue::Int(v) => format!(r#"{{"type":"int","value":{v}}}"#),
            PropertyValue::Bool(v) => format!(r#"{{"type":"bool","value":{v}}}"#),
            PropertyValue::String(v) => {
                format!(
                    r#"{{"type":"string","value":{}}}"#,
                    serde_json::Value::String(v.clone())
                )
            }
            PropertyValue::Color(c) => format!(
                r#"{{"type":"color","value":{{"r":{},"g":{},"b":{},"a":{}}}}}"#,
                c.r, c.g, c.b, c.a
            ),
            PropertyValue::Enum(e) => {
                let domain: Vec<serde_json::Value> = enum_values
                    .iter()
                    .map(|label| serde_json::Value::String(label.clone()))
                    .collect();
                format!(
                    r#"{{"type":"enum","value":{},"values":[{}]}}"#,
                    e.index,
                    domain
                        .iter()
                        .map(serde_json::Value::to_string)
                        .collect::<Vec<_>>()
                        .join(",")
                )
            }
        }
    }

    /// Une valeur décodée : son type, sa valeur, et le domaine d'un enum.
    #[derive(Debug, Clone, PartialEq)]
    pub struct DecodedValue {
        pub prop_type: super::PropertyType,
        pub value: PropertyValue,
        pub enum_values: Vec<String>,
    }
}

/// Géométrie native décodée d'une chaîne `params`.
///
/// Grammaires de `src/serialization/NativeSerializers.cpp`. Seuls les six
/// types natifs ont une grammaire connue ici : tout autre `type_id` reste
/// opaque (règle d'`UnknownEntity` — conservé, jamais deviné).
pub mod native_params {
    /// Géométrie d'une entité native, décodée de `params`.
    #[derive(Debug, Clone, PartialEq)]
    pub enum NativeGeometry {
        Point {
            position: [f64; 2],
        },
        Line {
            start: [f64; 2],
            end: [f64; 2],
        },
        Circle {
            center: [f64; 2],
            radius: f64,
        },
        Arc {
            center: [f64; 2],
            radius: f64,
            start_angle: f64,
            end_angle: f64,
        },
        Polyline {
            closed: bool,
            vertices: Vec<[f64; 2]>,
        },
        Text {
            position: [f64; 2],
            height: f64,
            rotation: f64,
            text: String,
        },
    }

    fn csv_numbers(params: &str) -> Option<Vec<f64>> {
        params
            .split(',')
            .map(|token| token.trim().parse::<f64>().map_err(|_| ()))
            .collect::<Result<Vec<_>, _>>()
            .ok()
    }

    /// Décode `params` pour un `type_id` natif. `None` = type non natif ou
    /// chaîne illisible : l'appelant conserve l'opaque, il ne l'invente pas.
    #[must_use]
    pub fn decode(type_id: &str, params: &str) -> Option<NativeGeometry> {
        match type_id {
            "bcad.Point" => {
                let v = csv_numbers(params)?;
                if v.len() < 2 {
                    return None;
                }
                Some(NativeGeometry::Point {
                    position: [v[0], v[1]],
                })
            }
            "bcad.Line" => {
                let v = csv_numbers(params)?;
                if v.len() < 4 {
                    return None;
                }
                Some(NativeGeometry::Line {
                    start: [v[0], v[1]],
                    end: [v[2], v[3]],
                })
            }
            "bcad.Circle" => {
                let v = csv_numbers(params)?;
                if v.len() < 3 {
                    return None;
                }
                Some(NativeGeometry::Circle {
                    center: [v[0], v[1]],
                    radius: v[2],
                })
            }
            "bcad.Arc" => {
                let v = csv_numbers(params)?;
                if v.len() < 5 {
                    return None;
                }
                Some(NativeGeometry::Arc {
                    center: [v[0], v[1]],
                    radius: v[2],
                    start_angle: v[3],
                    end_angle: v[4],
                })
            }
            "bcad.Polyline" => {
                let v = csv_numbers(params)?;
                if v.is_empty() {
                    return None;
                }
                let mut vertices = Vec::new();
                let mut i = 1;
                while i + 1 < v.len() {
                    vertices.push([v[i], v[i + 1]]);
                    i += 2;
                }
                Some(NativeGeometry::Polyline {
                    closed: v[0] != 0.0,
                    vertices,
                })
            }
            "bcad.Text" => {
                // Le texte peut contenir des virgules : les quatre premiers
                // champs sont numériques, le reste EST le texte.
                let mut parts = params.splitn(5, ',');
                let x: f64 = parts.next()?.trim().parse().ok()?;
                let y: f64 = parts.next()?.trim().parse().ok()?;
                let height: f64 = parts.next()?.trim().parse().ok()?;
                let rotation: f64 = parts.next()?.trim().parse().ok()?;
                let text = parts.next().unwrap_or("").to_owned();
                Some(NativeGeometry::Text {
                    position: [x, y],
                    height,
                    rotation,
                    text,
                })
            }
            _ => None,
        }
    }
}

/// Le sens métier que le format v1 portait sans le nommer.
///
/// L'entier de la colonne `type` et le `type_id` qu'il désignait. Mêmes règles
/// que C++ `legacyTypeIdOf` : 5 sans marqueur '|' était une polyligne jamais
/// promue.
pub mod legacy_v1 {
    /// L'identifiant de type qu'un fichier v1 portait pour les parcelles.
    pub const PARCEL_TYPE_ID: &str = "cadastre.parcel";
    /// Le préfixe de clé des six colonnes de `cadastre_parcels`.
    pub const KEY_PREFIX: &str = "cadastre.";
    /// Les six colonnes, dans l'ordre.
    pub const COLUMNS: [&str; 6] = [
        "section",
        "numero",
        "contenance",
        "commune",
        "proprietaire",
        "nature",
    ];

    /// Traduit l'entier v1 en `type_id`. `None` = entier que le format v1
    /// lui-même ne savait pas nommer : sans place nulle part, même en v2.
    #[must_use]
    pub const fn type_id_of(type_int: i64, params: &str) -> Option<&'static str> {
        match type_int {
            0 => Some("bcad.Point"),
            1 => Some("bcad.Line"),
            2 => Some("bcad.Circle"),
            3 => Some("bcad.Arc"),
            4 => Some("bcad.Polyline"),
            5 => {
                if contains_bar(params) {
                    Some(PARCEL_TYPE_ID)
                } else {
                    Some("bcad.Polyline")
                }
            }
            6 => Some("bcad.Text"),
            _ => None,
        }
    }

    const fn contains_bar(params: &str) -> bool {
        let bytes = params.as_bytes();
        let mut i = 0;
        while i < bytes.len() {
            if bytes[i] == b'|' {
                return true;
            }
            i += 1;
        }
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_format_version() {
        assert_eq!(FormatVersion::from_u32(1), Some(FormatVersion::V1));
        assert_eq!(FormatVersion::from_u32(2), Some(FormatVersion::V2));
        assert_eq!(FormatVersion::from_u32(3), Some(FormatVersion::V3));
        assert_eq!(FormatVersion::from_u32(4), None);
        assert!(FormatVersion::CURRENT.is_supported());
    }

    #[test]
    fn test_property_value_serde() {
        let val = PropertyValue::Double(1.5);
        let json = serde_json::to_string(&val).unwrap();
        assert!(json.contains("Double"));
        let parsed: PropertyValue = serde_json::from_str(&json).unwrap();
        assert_eq!(val, parsed);
    }

    #[test]
    fn test_color_serde() {
        let c = Color::from_rgb255(255, 128, 64, 200);
        let json = serde_json::to_string(&c).unwrap();
        let parsed: Color = serde_json::from_str(&json).unwrap();
        assert!((c.r - parsed.r).abs() < f32::EPSILON);
    }

    #[test]
    fn test_property_map() {
        let mut map = PropertyMap::new();
        map.insert("height", PropertyValue::Double(2.5));
        map.insert("name", PropertyValue::String("wall".to_string()));
        assert_eq!(map.get("height"), Some(&PropertyValue::Double(2.5)));
    }

    #[test]
    fn test_diagnostic_builder() {
        let diag = Diagnostic::error("TEST-001", "Test error")
            .with_file("test.dxf")
            .with_entity(42)
            .with_suggestion("Fix the thing");
        assert_eq!(diag.severity, Severity::Error);
        assert_eq!(diag.code, "TEST-001");
        assert_eq!(diag.file, Some("test.dxf".to_string()));
        assert_eq!(diag.entity_id, Some(42));
    }

    /// `clamp_color_channel` returns `f32`; compare with a tolerance rather than
    /// for equality, since the narrowing to 24 bits of mantissa is not exact.
    fn close(a: f32, b: f32) -> bool {
        (a - b).abs() < 1e-6
    }

    #[test]
    fn test_convert_clamp() {
        assert!(close(convert::clamp_color_channel(-1.0), 0.0));
        assert!(close(convert::clamp_color_channel(1.5), 1.0));
        assert!(close(convert::clamp_color_channel(0.5), 0.5));
    }

    // The casts below are the subject under test, not an oversight.
    #[allow(clippy::cast_precision_loss)]
    #[test]
    fn f64_to_i64_rejects_the_boundary_the_old_guard_let_through() {
        // 2^63 is the first f64 that no longer fits an i64. It must be an error,
        // not a silent saturation to i64::MAX: the previous guard compared
        // against `i64::MAX as f64`, which rounds up to 2^63 and so accepted it.
        // Compared on their bits: both are exactly 2^63, so equality is the
        // claim being made, and the bit pattern states it without relying on
        // float equality.
        let two_pow_63 = 9_223_372_036_854_775_808.0_f64;
        assert_eq!(
            (i64::MAX as f64).to_bits(),
            two_pow_63.to_bits(),
            "i64::MAX as f64 rounds up to 2^63"
        );
        assert!(convert::f64_to_i64(two_pow_63).is_err());
        // -2^63 is the one boundary that IS representable and IS in range.
        // -2^63 - 1.0 cannot be used as the negative probe: f64 has no such
        // value, it rounds back to -2^63. The next representable value below is
        // -2^63 - 2048, so that is what the guard has to reject.
        assert_eq!(
            convert::f64_to_i64(-two_pow_63).expect("-2^63 fits"),
            i64::MIN
        );
        assert!(convert::f64_to_i64(-two_pow_63 - 2048.0).is_err());
        assert!(convert::f64_to_i64(-1.0e300).is_err());
        assert!(convert::f64_to_i64(f64::NAN).is_err());
        assert!(convert::f64_to_i64(f64::INFINITY).is_err());
        assert!(convert::f64_to_i64(f64::NEG_INFINITY).is_err());
    }

    #[test]
    fn f64_to_i64_still_converts_the_values_a_coordinate_carries() {
        assert_eq!(convert::f64_to_i64(0.0).expect("zero"), 0);
        assert_eq!(convert::f64_to_i64(-42.9).expect("negative"), -42);
        assert_eq!(convert::f64_to_i64(42.9).expect("positive"), 42);
        let big = 4_503_599_627_370_496.0_f64; // 2^52, last exactly held step
        assert_eq!(
            convert::f64_to_i64(big).expect("2^52"),
            4_503_599_627_370_496
        );
    }

    #[test]
    fn value_json_round_trips_the_cpp_grammar() {
        // Les chaînes exactes que C++ écrit : le codec Rust doit les relire.
        let cases = [
            (r#"{"type":"double","value":1250.42}"#, PropertyType::Double),
            (r#"{"type":"int","value":3}"#, PropertyType::Int),
            (r#"{"type":"bool","value":true}"#, PropertyType::Bool),
            (r#"{"type":"string","value":"Lome"}"#, PropertyType::String),
            (
                r#"{"type":"color","value":{"r":1.0,"g":0.0,"b":0.0,"a":1.0}}"#,
                PropertyType::Color,
            ),
            (
                r#"{"type":"enum","value":1,"values":["pose","verifie"]}"#,
                PropertyType::Enum,
            ),
        ];
        for (text, prop_type) in cases {
            let decoded = value_json::decode(text).expect("grammaire C++");
            assert_eq!(decoded.prop_type, prop_type);
            let back = value_json::encode(&decoded.value, &decoded.enum_values);
            let again = value_json::decode(&back).expect("réécriture relisible");
            assert_eq!(again.value, decoded.value);
        }
        // Le domaine voyage avec l'enum, sinon l'index est hors domaine.
        let decoded =
            value_json::decode(r#"{"type":"enum","value":1,"values":["pose","verifie"]}"#).unwrap();
        assert_eq!(decoded.enum_values, vec!["pose", "verifie"]);
        // Strict : un tag inconnu est invalide, pas deviné.
        assert!(value_json::decode(r#"{"type":"date","value":"2026"}"#).is_none());
        assert!(value_json::decode("pas du json").is_none());
    }

    #[test]
    fn native_params_decode_the_six_native_grammars() {
        use native_params::{decode, NativeGeometry};
        assert_eq!(
            decode("bcad.Point", "1.5,2.5"),
            Some(NativeGeometry::Point {
                position: [1.5, 2.5]
            })
        );
        assert!(matches!(
            decode("bcad.Line", "0,0,10,5"),
            Some(NativeGeometry::Line { .. })
        ));
        assert!(matches!(
            decode("bcad.Circle", "0,0,5"),
            Some(NativeGeometry::Circle { .. })
        ));
        assert!(matches!(
            decode("bcad.Arc", "0,0,5,0,1.57"),
            Some(NativeGeometry::Arc { .. })
        ));
        assert_eq!(
            decode("bcad.Polyline", "1,0,0,30,0,30,20"),
            Some(NativeGeometry::Polyline {
                closed: true,
                vertices: vec![[0.0, 0.0], [30.0, 0.0], [30.0, 20.0]],
            })
        );
        // Le texte peut contenir des virgules : tout après le 4e champ.
        assert_eq!(
            decode("bcad.Text", "10,5,2.5,0,cote 12,50 m"),
            Some(NativeGeometry::Text {
                position: [10.0, 5.0],
                height: 2.5,
                rotation: 0.0,
                text: "cote 12,50 m".to_string(),
            })
        );
        // Inconnu ou illisible : opaque, jamais inventé.
        assert_eq!(decode("network.pipe", "DN200"), None);
        assert_eq!(decode("bcad.Line", "0,0"), None);
        assert_eq!(decode("bcad.Point", "abc,def"), None);
    }

    #[test]
    fn legacy_v1_maps_like_the_cpp_reader() {
        use legacy_v1::type_id_of;
        assert_eq!(type_id_of(1, "0,0,1,1"), Some("bcad.Line"));
        assert_eq!(type_id_of(5, "1,0,0|AB|0072"), Some("cadastre.parcel"));
        assert_eq!(type_id_of(5, "1,0,0,30,0"), Some("bcad.Polyline"));
        assert_eq!(type_id_of(9, "x"), None);
    }
}
