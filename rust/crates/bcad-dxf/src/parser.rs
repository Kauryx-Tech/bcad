//! The DXF reader: a `Vec<Token>` in, a [`ParsedDxf`] out.
//!
//! # Token ownership
//!
//! [`Parser`] owns its tokens in a single `Vec<Token>`. There is one
//! constructor. The parser never borrows the token stream, never re-tokenizes,
//! and never holds a reference into the original text, so the parsed document
//! has the same lifetime as the data it describes.
//!
//! # Marker consumption contract
//!
//! DXF structure is expressed by `0`-code markers. Getting these wrong —
//! consuming a marker twice, or not at all — is the classic source of parsers
//! that appear to work on small files and drift on large ones. Every method
//! below therefore documents its entry and exit state.
//!
//! Convention used throughout: a method that *consumes* a marker advances past
//! it, and a method that *peeks* at a marker leaves it as the current token.
//! The methods that stop on a boundary (`parse_header`, `parse_tables`,
//! `parse_entities`, …) leave the closing marker as the current token; the
//! caller consumes it. `expect_marker` is the only way a closing marker is
//! consumed, so each of `ENDSEC`/`ENDTAB`/`SEQEND` is consumed exactly once and
//! by exactly one caller.
//!
//! # Layout
//!
//! This module holds the state, the token cursor and the section dispatch. Each
//! section is then read by the module that owns its vocabulary, because the
//! three fail in different ways and are tested separately:
//!
//! | Module | Reads | Vocabulary |
//! |---|---|---|
//! | [`crate::header`] | `HEADER` | `$`-variables, X/Y/Z points |
//! | [`crate::tables`] | `TABLES` | `LAYER` and `APPID` records |
//! | [`crate::entity_section`] | `ENTITIES` | entities, handles, `XDATA`, `VERTEX` |
//!
//! The fields and cursor helpers are `pub(crate)` because those modules are
//! `impl Parser` blocks: they are one type's behaviour split along DXF
//! structure, not independent types.

use std::collections::{BTreeMap, BTreeSet};

use bcad_format::Diagnostic;

use crate::error::{DxfError, DxfResult};
use crate::group_code as gcode;
use crate::limits::ParseLimits;
use crate::model::ParsedDxf;
use crate::recovery::RecoveryMode;
use crate::token::Token;
use crate::tokenizer::tokenize;

/// Reads a token stream into a [`ParsedDxf`].
pub struct Parser {
    /// The complete group stream. Owned: the parser is `'static` with respect
    /// to the tokens it reads.
    pub(crate) tokens: Vec<Token>,
    /// Index of the current token. `tokens.len()` means end of stream.
    pub(crate) pos: usize,
    pub(crate) limits: ParseLimits,
    pub(crate) recovery: RecoveryMode,
    /// The document under construction.
    pub(crate) dxf: ParsedDxf,
    /// Handles already seen, to report duplicates instead of silently keeping
    /// the last one.
    pub(crate) seen_handles: BTreeSet<String>,
    /// Line of the most recently consumed token, used to report a location when
    /// the stream has run out.
    pub(crate) last_line: usize,
}

impl Parser {
    /// Takes ownership of `tokens` and prepares to read them.
    ///
    /// This is the only constructor: there is no borrowed variant, so a
    /// `Parser` always knows the full extent of its input.
    #[must_use]
    pub fn new(tokens: Vec<Token>, limits: ParseLimits, recovery: RecoveryMode) -> Self {
        Self {
            tokens,
            pos: 0,
            limits,
            recovery,
            dxf: ParsedDxf::new(),
            seen_handles: BTreeSet::new(),
            last_line: 0,
        }
    }

    /// Tokenizes `input` and reads it.
    ///
    /// # Errors
    ///
    /// Propagates any tokenizing failure from [`tokenize`], then whatever
    /// [`Parser::read`] reports.
    pub fn parse_text(
        input: &str,
        limits: ParseLimits,
        recovery: RecoveryMode,
    ) -> DxfResult<ParsedDxf> {
        let tokens = tokenize(input, &limits)?;
        Self::new(tokens, limits, recovery).read()
    }

    // -- token cursor ------------------------------------------------------
    //
    // All cursor access goes through these four helpers. They return owned
    // values or `bool`s rather than borrows, which keeps the calling code free
    // of borrow conflicts between "look at the current token" and "consume it".

    /// `true` when the stream is exhausted.
    #[must_use]
    pub(crate) const fn at_end(&self) -> bool {
        self.pos >= self.tokens.len()
    }

    /// Group code of the current token, or `None` at end of stream.
    pub(crate) fn code_here(&self) -> Option<i32> {
        self.tokens.get(self.pos).map(|t| t.code)
    }

    /// `true` when the current token is `0/<marker>`.
    pub(crate) fn at_marker(&self, marker: &str) -> bool {
        self.tokens
            .get(self.pos)
            .is_some_and(|t| t.is_marker(marker))
    }

    /// `true` when the current token has the given group code.
    pub(crate) fn at_code(&self, code: i32) -> bool {
        self.code_here() == Some(code)
    }

    /// Consumes the current token, returning it.
    pub(crate) fn bump(&mut self) -> Option<Token> {
        let token = self.tokens.get(self.pos).cloned();
        if let Some(t) = &token {
            self.last_line = t.line;
            self.pos += 1;
        }
        token
    }

    /// Source line to use when reporting a problem at the current position.
    pub(crate) fn line_here(&self) -> usize {
        self.tokens.get(self.pos).map_or(self.last_line, |t| t.line)
    }

    /// Consumes the current token, requiring a specific group code.
    pub(crate) fn expect_code(&mut self, code: i32) -> DxfResult<String> {
        match self.tokens.get(self.pos) {
            None => Err(DxfError::unexpected_eof(self.last_line)),
            Some(token) if token.code == code => {
                let value = token.value.clone();
                self.last_line = token.line;
                self.pos += 1;
                Ok(value)
            }
            Some(token) => Err(DxfError::unexpected_group_code(
                code, token.code, token.line,
            )),
        }
    }

    /// Consumes `0/<marker>`, or fails without consuming anything.
    ///
    /// This is the single place a structural marker is consumed, which is what
    /// guarantees each marker is consumed exactly once.
    pub(crate) fn expect_marker(&mut self, marker: &'static str) -> DxfResult<()> {
        match self.tokens.get(self.pos) {
            None => Err(DxfError::unexpected_eof(self.last_line)),
            Some(token) if token.is_marker(marker) => {
                self.last_line = token.line;
                self.pos += 1;
                Ok(())
            }
            Some(token) => Err(DxfError::unexpected_marker(
                marker,
                token.value.clone(),
                token.line,
            )),
        }
    }

    /// Skips forward until the current token is `0/<marker>`.
    ///
    /// Stops *on* the marker without consuming it. Returns `false` if the
    /// stream ended first, leaving `pos` at the end.
    pub(crate) fn skip_to_marker(&mut self, marker: &str) -> bool {
        while let Some(token) = self.tokens.get(self.pos) {
            if token.is_marker(marker) {
                return true;
            }
            self.last_line = token.line;
            self.pos += 1;
        }
        false
    }

    // -- diagnostics -------------------------------------------------------

    /// Records a problem, or propagates it when running strictly.
    ///
    /// A breached [`ParseLimits`] ceiling is propagated in *both* modes; see
    /// [`DxfError::is_resource_limit`].
    pub(crate) fn report(&mut self, error: DxfError) -> DxfResult<()> {
        if self.recovery.is_strict() || error.is_resource_limit() {
            return Err(error);
        }
        self.record(&error);
        Ok(())
    }

    /// Records a warning. Warnings never abort the parse.
    pub(crate) fn warn(&mut self, code: &str, message: impl Into<String>, line: usize) {
        self.dxf
            .add_diagnostic(Diagnostic::warning(code, message).with_context("line", line));
    }

    /// Records an error as a diagnostic, with the line attached.
    pub(crate) fn record(&mut self, error: &DxfError) {
        let mut diagnostic =
            Diagnostic::error(diagnostic_code(&error.to_string()), error.to_string());
        if let Some(line) = error.line() {
            diagnostic = diagnostic.with_context("line", line);
        }
        self.dxf.add_diagnostic(diagnostic);
    }

    // -- top level ---------------------------------------------------------

    /// Reads the whole stream.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: `pos` is 0.
    /// * **On exit**: `pos` is past `EOF`, or at the end of the stream if the
    ///   file is truncated.
    ///
    /// # Errors
    ///
    /// Under [`RecoveryMode::Strict`] this returns the first structural problem
    /// it meets. Under [`RecoveryMode::Recover`] recoverable problems are
    /// recorded in [`ParsedDxf::diagnostics`] and reading continues, so an `Ok`
    /// result may still describe an imperfect file — check
    /// [`ParsedDxf::has_errors`].
    pub fn read(mut self) -> DxfResult<ParsedDxf> {
        self.read_sections()?;
        self.check_layer_references();
        Ok(self.dxf)
    }

    /// Consumes sections until `EOF` or end of stream.
    fn read_sections(&mut self) -> DxfResult<()> {
        while !self.at_end() {
            if self.at_marker(gcode::SECTION) {
                if let Err(e) = self.read_section() {
                    // A structural failure means the stream can no longer be
                    // trusted: stop rather than resynchronise on arbitrary
                    // tokens. Under Strict that failure is the result. A breached
                    // limit is the result under either mode.
                    if self.recovery.is_strict() || e.is_resource_limit() {
                        return Err(e);
                    }
                    self.record(&e);
                    return Ok(());
                }
            } else if self.at_marker(gcode::EOF) {
                self.bump();
                return Ok(());
            } else {
                let line = self.line_here();
                let token = self.bump();
                if let Some(token) = token {
                    self.warn(
                        "DXF-STRUCTURE-001",
                        format!(
                            "ignoring token {} / {:?} outside any section",
                            token.code, token.value
                        ),
                        line,
                    );
                }
            }
        }

        self.warn(
            "DXF-STRUCTURE-002",
            "file ended without an EOF marker",
            self.last_line,
        );
        Ok(())
    }

    // -- section dispatch --------------------------------------------------

    /// Reads one `SECTION`.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is `0/SECTION`, not consumed.
    /// * **On exit**: current token is *after* `ENDSEC`. `SECTION` and
    ///   `ENDSEC` are each consumed exactly once, here.
    fn read_section(&mut self) -> DxfResult<()> {
        self.bump(); // SECTION
        let name = self.expect_code(gcode::NAME)?;

        match name.as_str() {
            gcode::SECTION_HEADER => self.read_header()?,
            gcode::SECTION_TABLES => self.read_tables()?,
            gcode::SECTION_ENTITIES => self.read_entities()?,
            other => {
                let line = self.last_line;
                self.warn(
                    "DXF-SECTION-001",
                    format!("skipping unsupported section {other:?}"),
                    line,
                );
                if !self.skip_to_marker(gcode::ENDSEC) {
                    return Err(DxfError::unexpected_eof(self.last_line));
                }
            }
        }

        self.expect_marker(gcode::ENDSEC)
    }

    // -- cross-entity validation -------------------------------------------

    /// Warns about entities that reference a layer the file never declared.
    ///
    /// This runs once the whole file has been read, because a layer may be
    /// declared after the entity that uses it.
    fn check_layer_references(&mut self) {
        let declared: BTreeSet<&str> = self.dxf.layers.iter().map(|l| l.name.as_str()).collect();

        // Owned keys: the diagnostics are appended to `self.dxf`, so this map
        // must not borrow from it.
        let mut missing: BTreeMap<String, usize> = BTreeMap::new();
        for entity in &self.dxf.entities {
            if !declared.contains(entity.layer.as_str()) {
                *missing.entry(entity.layer.clone()).or_default() += 1;
            }
        }

        for (layer, count) in missing {
            self.dxf.add_diagnostic(
                Diagnostic::warning(
                    "DXF-LAYER-001",
                    format!("layer {layer:?} is used by {count} entities but never declared"),
                )
                .with_context("layer", layer.as_str())
                .with_context("entity_count", count),
            );
        }
    }
}

/// Derives a stable diagnostic code from an error message.
///
/// Errors raised by this crate already carry a code; this fallback keeps the
/// diagnostic's `code` field non-empty for messages that do not.
fn diagnostic_code(message: &str) -> String {
    if message.starts_with("entity ") && message.contains("missing required group") {
        "DXF-PARSE-001".to_string()
    } else {
        "DXF-PARSE-000".to_string()
    }
}
