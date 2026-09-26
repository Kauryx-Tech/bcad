//! BCAD DXF reader.
//!
//! Reads untrusted DXF input into a neutral document. It is a *reader*: it
//! interprets the parts of DXF that map onto BCAD geometry and preserves
//! everything else verbatim, so a later pass can act on it without re-reading
//! the file.
//!
//! # Guarantees
//!
//! * **Total.** Every input produces either a `ParsedDxf` or a `DxfError`. There
//!   is no `unwrap`, `expect` or `panic` on the parsing path, so a hostile file
//!   cannot terminate the process.
//! * **Bounded.** Resource limits are checked against values the *file itself*
//!   declares, not only against what ends up in memory.
//! * **Located.** Every error and most diagnostics name the source line, the
//!   group code, and the entity.
//! * **Lossless where it matters.** Uninterpreted entities keep their raw
//!   groups; XDATA keeps every application identifier, including unknown ones.
//!
//! # Example
//!
//! ```
//! use bcad_dxf::{parse_dxf, ParseOptions};
//!
//! let dxf = parse_dxf(
//!     "0\nSECTION\n2\nENTITIES\n0\nLINE\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n0\nENDSEC\n0\nEOF\n",
//!     &ParseOptions::default(),
//! )
//! .expect("valid DXF");
//!
//! assert_eq!(dxf.entity_count(), 1);
//! ```
//!
//! # Scope
//!
//! The HEADER, TABLES (LAYER, APPID) and ENTITIES sections are read. The
//! BLOCKS, CLASSES, OBJECTS and THUMBNAILIMAGE sections are skipped wholesale:
//! their contents are not BCAD geometry, and skipping them is reported rather
//! than passed off as complete coverage.

#![forbid(unsafe_code)]
#![warn(missing_docs)]

// The three section readers are `impl Parser` blocks living in private
// modules: they are one type's behaviour split along DXF structure, not public
// API. `Parser`, `parse_dxf` and the model remain the whole surface.
mod entity_section;
mod header;
mod tables;
#[cfg(test)]
mod testutil;

pub mod entities;
pub mod error;
pub mod group_code;
pub mod limits;
pub mod model;
pub mod numeric;
pub mod parser;
pub mod recovery;
pub mod token;
pub mod tokenizer;
pub mod xdata;

use std::path::Path;

pub use error::{DxfError, DxfResult};
pub use group_code as groups;
pub use limits::ParseLimits;
pub use model::{ParsedDxf, ParsedHeader};
pub use parser::Parser;
pub use recovery::RecoveryMode;
pub use token::Token;
pub use tokenizer::tokenize;

pub use bcad_format::{
    Diagnostic, FormatVersion, ParseError, ParsedAppId, ParsedEntity, ParsedEntityType,
    ParsedLayer, PropertyMap, PropertyValue, RawGroup, Severity, XDataRecord,
};

/// Knobs controlling how a file is read.
#[derive(Debug, Clone, Default)]
pub struct ParseOptions {
    /// What to do with input that cannot be fully interpreted.
    pub recovery: RecoveryMode,
    /// Resource ceilings applied while reading.
    pub limits: ParseLimits,
    /// Reject input that is not valid UTF-8 instead of substituting
    /// replacement characters.
    pub strict_encoding: bool,
}

impl ParseOptions {
    /// Options that stop at the first problem.
    #[must_use]
    pub const fn strict() -> Self {
        Self {
            recovery: RecoveryMode::Strict,
            limits: ParseLimits::strict(),
            strict_encoding: true,
        }
    }
}

/// Reads DXF text.
///
/// The text is tokenized once, then read. Use [`Parser::parse_text`] directly to
/// reuse a set of options across many files.
///
/// # Errors
///
/// Returns [`DxfError`] for malformed input, a breached [`ParseLimits`] bound, or
/// — under [`RecoveryMode::Strict`] — the first defect encountered. Under
/// [`RecoveryMode::Recover`] recoverable defects are collected in
/// [`ParsedDxf::diagnostics`] instead.
pub fn parse_dxf(input: &str, options: &ParseOptions) -> DxfResult<ParsedDxf> {
    Parser::parse_text(input, options.limits.clone(), options.recovery)
}

/// Reads a DXF file from disk.
///
/// # Errors
///
/// Returns [`DxfError::Encoding`] when the file cannot be read, and otherwise
/// behaves exactly as [`parse_dxf_bytes`].
pub fn parse_dxf_file(path: impl AsRef<Path>, options: &ParseOptions) -> DxfResult<ParsedDxf> {
    let path = path.as_ref();
    let bytes = std::fs::read(path)
        .map_err(|e| DxfError::Encoding(format!("cannot read {}: {e}", path.display())))?;
    parse_dxf_bytes(&bytes, options)
}

/// Reads DXF from raw bytes.
///
/// DXF predates Unicode, so files are commonly written in a legacy code page.
/// Bytes are decoded as UTF-8 with a BOM honoured; invalid sequences become
/// replacement characters unless [`ParseOptions::strict_encoding`] is set, in
/// which case they are reported. Either way the caller is told through a
/// diagnostic rather than being left to guess.
///
/// # Errors
///
/// Returns [`DxfError::Encoding`] for undecodable bytes under
/// [`ParseOptions::strict_encoding`], [`DxfError::ResourceLimit`] when the file
/// is too large, and otherwise behaves exactly as [`parse_dxf`].
pub fn parse_dxf_bytes(bytes: &[u8], options: &ParseOptions) -> DxfResult<ParsedDxf> {
    options.limits.check_file_size(bytes.len())?;

    let (decoded, had_errors) = encoding_rs::UTF_8.decode_with_bom_removal(bytes);

    if had_errors && options.strict_encoding {
        return Err(DxfError::Encoding(
            "input is not valid UTF-8; a legacy code page is in use".to_string(),
        ));
    }

    let mut dxf = parse_dxf(&decoded, options)?;

    if had_errors {
        dxf.add_diagnostic(
            Diagnostic::warning(
                "DXF-ENCODING-001",
                "input contained bytes that are not valid UTF-8; they were replaced",
            )
            .with_suggestion("re-encode the file as UTF-8, or read $DWGCODEPAGE"),
        );
    }

    Ok(dxf)
}

#[cfg(test)]
mod tests {
    use super::*;
    use bcad_format::ParsedEntityType;

    const LINE_DXF: &str = "\
0
SECTION
2
ENTITIES
0
LINE
8
0
10
0.0
20
0.0
11
10.0
21
5.0
0
ENDSEC
0
EOF
";

    #[test]
    fn reads_a_line() {
        let dxf = parse_dxf(LINE_DXF, &ParseOptions::default()).expect("valid");
        assert_eq!(dxf.entity_count(), 1);
        let entity = &dxf.entities[0];
        assert_eq!(entity.layer, "0");
        assert!(matches!(entity.entity_type, ParsedEntityType::Line { .. }));
    }

    #[test]
    fn reports_an_undefined_layer_without_failing() {
        let text = LINE_DXF.replace("8\n0\n", "8\nGHOST\n");
        let dxf = parse_dxf(&text, &ParseOptions::default()).expect("recovers");
        assert_eq!(dxf.entity_count(), 1);
        assert!(dxf.diagnostics.iter().any(|d| d.code == "DXF-LAYER-001"));
    }

    #[test]
    fn strict_mode_rejects_an_undefined_layer() {
        let text = LINE_DXF.replace("8\n0\n", "8\nGHOST\n");
        // A layer reference is a warning, not a hard error, so strict reading
        // still succeeds; what strict changes is the treatment of entities
        // that cannot be interpreted at all.
        assert!(parse_dxf(&text, &ParseOptions::strict()).is_ok());
    }

    /// A header that declares `$EXTMIN` but supplies only X.
    const TRUNCATED_EXTMIN: &str = "\
0
SECTION
2
HEADER
9
$EXTMIN
10
1.0
0
ENDSEC
0
EOF
";

    /// An `ENTITIES` section that is never closed, so a `0/SECTION` from the
    /// next section appears where an entity name is expected.
    const UNCLOSED_ENTITIES: &str = "\
0
SECTION
2
ENTITIES
0
LINE
8
0
10
0.0
20
0.0
11
10.0
21
5.0
0
SECTION
2
HEADER
0
ENDSEC
0
EOF
";

    /// An `ENTITIES` section holding a bare `0/EOF`, which would also rewind.
    const EOF_INSIDE_ENTITIES: &str = "\
0
SECTION
2
ENTITIES
0
LINE
8
0
10
0.0
20
0.0
11
10.0
21
5.0
0
EOF
";

    #[test]
    fn strict_mode_fails_on_a_truncated_header_point() {
        // Regression: the point used to be zero-padded into a plausible-looking
        // [1.0, 0.0, 0.0] instead of being reported.
        let err = parse_dxf(TRUNCATED_EXTMIN, &ParseOptions::strict())
            .expect_err("a half-written $EXTMIN must not be accepted");
        assert!(
            err.to_string().contains("$EXTMIN"),
            "error should name the variable: {err}"
        );
    }

    #[test]
    fn recover_mode_records_a_truncated_header_point() {
        let dxf = parse_dxf(TRUNCATED_EXTMIN, &ParseOptions::default()).expect("recovers");
        assert!(dxf.has_errors(), "the truncation must be reported");
        assert!(
            dxf.header.extmin.is_none(),
            "no coordinate may be invented for a truncated point"
        );
    }

    #[test]
    fn a_complete_header_point_is_kept() {
        let text = TRUNCATED_EXTMIN.replace("10\n1.0\n", "10\n1.0\n20\n2.0\n30\n3.0\n");
        let dxf = parse_dxf(&text, &ParseOptions::default()).expect("valid");
        let extmin = dxf.header.extmin.expect("a complete point must be read");
        assert!((extmin[0] - 1.0).abs() < 1e-9, "{extmin:?}");
        assert!((extmin[1] - 2.0).abs() < 1e-9, "{extmin:?}");
        assert!((extmin[2] - 3.0).abs() < 1e-9, "{extmin:?}");
    }

    #[test]
    fn strict_mode_fails_on_a_nested_section_inside_entities() {
        // Regression: the pseudo-entity guard used to rewind, so this input
        // spun forever instead of returning.
        let err = parse_dxf(UNCLOSED_ENTITIES, &ParseOptions::strict())
            .expect_err("an unclosed ENTITIES section must not be accepted");
        assert!(err.to_string().contains("ENTITIES"), "{err}");
    }

    #[test]
    fn recover_mode_stops_at_a_nested_section_inside_entities() {
        let dxf = parse_dxf(UNCLOSED_ENTITIES, &ParseOptions::default()).expect("recovers");
        assert!(dxf.has_errors());
        assert_eq!(dxf.entity_count(), 1, "the LINE before the break is kept");
    }

    #[test]
    fn a_bare_eof_inside_entities_terminates_the_parse() {
        // Same rewind hazard as the nested SECTION.
        let dxf = parse_dxf(EOF_INSIDE_ENTITIES, &ParseOptions::default()).expect("recovers");
        assert!(dxf.has_errors());
        assert_eq!(dxf.entity_count(), 1);
        assert!(parse_dxf(EOF_INSIDE_ENTITIES, &ParseOptions::strict()).is_err());
    }

    #[test]
    fn empty_input_reports_a_missing_eof() {
        let dxf = parse_dxf("", &ParseOptions::default()).expect("recovers");
        assert!(dxf
            .diagnostics
            .iter()
            .any(|d| d.code == "DXF-STRUCTURE-002"));
    }

    #[test]
    fn bytes_entry_point_accepts_a_bom() {
        let mut bytes = vec![0xEF, 0xBB, 0xBF];
        bytes.extend_from_slice(LINE_DXF.as_bytes());
        let dxf = parse_dxf_bytes(&bytes, &ParseOptions::default()).expect("valid");
        assert_eq!(dxf.entity_count(), 1);
        assert!(!dxf.diagnostics.iter().any(|d| d.code == "DXF-ENCODING-001"));
    }

    #[test]
    fn invalid_utf8_is_a_diagnostic_by_default_and_an_error_when_strict() {
        // The invalid bytes sit in a value position, so the group stream itself
        // stays well formed and only the *encoding* is wrong. Appending them
        // after EOF would instead be a structural error, which is a different
        // test.
        let mut bytes =
            b"0\nSECTION\n2\nENTITIES\n0\nTEXT\n8\n0\n10\n0.0\n20\n0.0\n40\n1.0\n1\n".to_vec();
        bytes.extend_from_slice(&[0xFF, 0xFE]);
        bytes.extend_from_slice(b"\n0\nENDSEC\n0\nEOF\n");

        let lenient = parse_dxf_bytes(&bytes, &ParseOptions::default()).expect("recovers");
        assert_eq!(lenient.entity_count(), 1);
        assert!(lenient
            .diagnostics
            .iter()
            .any(|d| d.code == "DXF-ENCODING-001"));

        let strict = ParseOptions {
            strict_encoding: true,
            ..ParseOptions::default()
        };
        assert!(parse_dxf_bytes(&bytes, &strict).is_err());
    }

    #[test]
    fn file_size_limit_applies_to_bytes() {
        let options = ParseOptions {
            limits: ParseLimits {
                max_file_size: 4,
                ..ParseLimits::default()
            },
            ..ParseOptions::default()
        };
        assert!(parse_dxf_bytes(LINE_DXF.as_bytes(), &options).is_err());
    }
}
