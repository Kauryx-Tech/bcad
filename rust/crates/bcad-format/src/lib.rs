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
}

impl FormatVersion {
    pub const CURRENT: Self = Self::V2;
    pub const MIN_SUPPORTED: Self = Self::V1;
    pub const MAX_SUPPORTED: Self = Self::V2;

    #[must_use]
    pub const fn from_u32(v: u32) -> Option<Self> {
        match v {
            1 => Some(Self::V1),
            2 => Some(Self::V2),
            _ => None,
        }
    }

    #[must_use]
    pub const fn is_supported(self) -> bool {
        matches!(self, Self::V1 | Self::V2)
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_format_version() {
        assert_eq!(FormatVersion::from_u32(1), Some(FormatVersion::V1));
        assert_eq!(FormatVersion::from_u32(2), Some(FormatVersion::V2));
        assert_eq!(FormatVersion::from_u32(3), None);
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
}
