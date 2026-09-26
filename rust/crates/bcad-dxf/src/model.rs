//! The parsed DXF document.
//!
//! This module defines only what is genuinely specific to DXF: the document
//! itself and its HEADER variables. Every neutral type — [`ParsedEntity`],
//! [`ParsedEntityType`], [`ParsedLayer`], [`ParsedAppId`], [`RawGroup`],
//! [`XDataRecord`], [`Diagnostic`] — is defined once in `bcad-format` and
//! re-exported here for convenience. Nothing is redefined: a second definition
//! would be a second, silently divergent contract.

use bcad_format::{Diagnostic, ParsedAppId, ParsedEntity, ParsedLayer, Severity};
use serde::{Deserialize, Serialize};

pub use bcad_format::{
    Color, EnumValue, FormatVersion, ParseError, ParsedEntityType, PropertyMap, PropertyMeta,
    PropertyType, PropertyValue, RawGroup, TypeId, XDataRecord,
};

/// HEADER section variables retained by this reader.
///
/// Every field is optional: a DXF file may omit any HEADER variable, and an
/// absent variable is not an error. Values that are *present but malformed* are
/// reported as diagnostics by the parser rather than silently dropped.
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct ParsedHeader {
    /// `$ACADVER`, e.g. `AC1015`.
    pub acad_version: Option<String>,
    /// `$DWGCODEPAGE`, the code page the legacy text fields were written with.
    pub dwg_codepage: Option<String>,
    /// `$INSUNITS`, drawing units as an insert-scale code.
    pub insunits: Option<i32>,
    /// `$EXTMIN`, the drawing extents minimum.
    pub extmin: Option<[f64; 3]>,
    /// `$EXTMAX`, the drawing extents maximum.
    pub extmax: Option<[f64; 3]>,
    /// `$LIMMIN`, the drawing limits minimum.
    pub limmin: Option<[f64; 2]>,
    /// `$LIMMAX`, the drawing limits maximum.
    pub limmax: Option<[f64; 2]>,
}

/// A DXF file that has been read.
///
/// A `ParsedDxf` is produced even when diagnostics contain errors: the reader
/// keeps whatever it could interpret and reports the rest. Callers decide
/// whether to accept a document with errors via [`ParsedDxf::has_errors`].
#[derive(Debug, Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct ParsedDxf {
    /// HEADER variables.
    pub header: ParsedHeader,
    /// Layers declared in the TABLES section, in file order.
    pub layers: Vec<ParsedLayer>,
    /// Entities of the ENTITIES section, in file order.
    pub entities: Vec<ParsedEntity>,
    /// Registered application identifiers from the APPID table.
    pub app_ids: Vec<ParsedAppId>,
    /// Everything worth telling the caller about what was imperfect.
    pub diagnostics: Vec<Diagnostic>,
}

impl ParsedDxf {
    /// An empty document with no diagnostics.
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Records a diagnostic.
    pub fn add_diagnostic(&mut self, diagnostic: Diagnostic) {
        self.diagnostics.push(diagnostic);
    }

    /// `true` when at least one diagnostic is an error.
    #[must_use]
    pub fn has_errors(&self) -> bool {
        self.diagnostics
            .iter()
            .any(|d| d.severity == Severity::Error)
    }

    /// The diagnostics that are errors.
    pub fn errors(&self) -> impl Iterator<Item = &Diagnostic> {
        self.diagnostics
            .iter()
            .filter(|d| d.severity == Severity::Error)
    }

    /// The diagnostics that are warnings.
    pub fn warnings(&self) -> impl Iterator<Item = &Diagnostic> {
        self.diagnostics
            .iter()
            .filter(|d| d.severity == Severity::Warning)
    }

    /// Number of parsed entities.
    #[must_use]
    pub const fn entity_count(&self) -> usize {
        self.entities.len()
    }

    /// Number of declared layers.
    #[must_use]
    pub const fn layer_count(&self) -> usize {
        self.layers.len()
    }

    /// `true` when a layer with this name was declared in the TABLES section.
    #[must_use]
    pub fn has_layer(&self, name: &str) -> bool {
        self.layers.iter().any(|l| l.name == name)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_document_is_empty_and_error_free() {
        let dxf = ParsedDxf::new();
        assert_eq!(dxf.entity_count(), 0);
        assert_eq!(dxf.layer_count(), 0);
        assert!(!dxf.has_errors());
        assert!(dxf.diagnostics.is_empty());
    }

    #[test]
    fn has_errors_tracks_only_errors() {
        let mut dxf = ParsedDxf::new();
        dxf.add_diagnostic(Diagnostic::warning("W", "warn"));
        assert!(!dxf.has_errors());
        assert_eq!(dxf.warnings().count(), 1);

        dxf.add_diagnostic(Diagnostic::error("E", "err"));
        assert!(dxf.has_errors());
        assert_eq!(dxf.errors().count(), 1);
    }

    #[test]
    fn has_layer_finds_declared_layers() {
        let mut dxf = ParsedDxf::new();
        dxf.layers.push(ParsedLayer {
            name: "0".to_string(),
            color: 7,
            line_weight: -1.0,
            visible: true,
            locked: false,
            frozen: false,
            plot: true,
            line_type: "CONTINUOUS".to_string(),
        });
        assert!(dxf.has_layer("0"));
        assert!(!dxf.has_layer("MISSING"));
    }

    #[test]
    fn header_defaults_to_all_none() {
        assert_eq!(ParsedHeader::default().acad_version, None);
    }
}
