//! Helpers for building DXF fixtures in tests.
//!
//! The section readers are tested through the real tokenizer and the real
//! parser, so a fixture is genuine DXF text rather than a hand-built token
//! stream. That is deliberate: a test that bypassed the tokenizer would not
//! exercise the framing contract these modules exist to uphold.

use crate::{parse_dxf, DxfResult, ParseLimits, ParseOptions, ParsedDxf, RecoveryMode};

/// Wraps `body` in `0/SECTION` / `2/<name>` / … / `0/ENDSEC`.
#[must_use]
pub fn section(name: &str, body: &str) -> String {
    format!("0\nSECTION\n2\n{name}\n{body}0\nENDSEC\n")
}

/// Builds a whole file from already-wrapped sections and closes it with `0/EOF`.
#[must_use]
pub fn file(sections: &[String]) -> String {
    let mut out = String::new();
    for s in sections {
        out.push_str(s);
    }
    out.push_str("0\nEOF\n");
    out
}

/// A `HEADER` section holding `body`.
#[must_use]
pub fn header(body: &str) -> String {
    section("HEADER", body)
}

/// A `TABLES` section holding `body`.
#[must_use]
pub fn tables(body: &str) -> String {
    section("TABLES", body)
}

/// An `ENTITIES` section holding `body`.
#[must_use]
pub fn entities(body: &str) -> String {
    section("ENTITIES", body)
}

/// Reads `text` under the given recovery mode, with default limits.
pub fn read(text: &str, recovery: RecoveryMode) -> DxfResult<ParsedDxf> {
    let options = ParseOptions {
        recovery,
        limits: ParseLimits::default(),
        strict_encoding: false,
    };
    parse_dxf(text, &options)
}

/// Reads `text` expecting success.
#[must_use]
pub fn read_ok(text: &str) -> ParsedDxf {
    read(text, RecoveryMode::Recover).expect("fixture must parse")
}

/// Every diagnostic code present, in order.
#[must_use]
pub fn codes(dxf: &ParsedDxf) -> Vec<&str> {
    dxf.diagnostics.iter().map(|d| d.code.as_str()).collect()
}

/// `true` when a diagnostic carries `code`.
#[must_use]
pub fn has_code(dxf: &ParsedDxf, code: &str) -> bool {
    dxf.diagnostics.iter().any(|d| d.code == code)
}

/// The first diagnostic message carrying `code`.
#[must_use]
pub fn message_for(dxf: &ParsedDxf, code: &str) -> String {
    dxf.diagnostics.iter().find(|d| d.code == code).map_or_else(
        || panic!("no diagnostic {code} in {codes:?}", codes = codes(dxf)),
        |d| d.message.clone(),
    )
}
