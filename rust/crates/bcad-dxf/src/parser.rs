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

use std::collections::BTreeSet;

use bcad_format::{Diagnostic, ParsedAppId, ParsedEntity, ParsedEntityType, ParsedLayer, RawGroup};

use crate::entities;
use crate::error::{DxfError, DxfResult};
use crate::group_code as gcode;
use crate::limits::ParseLimits;
use crate::model::ParsedDxf;
use crate::numeric::{f64_field, integerish_field};
use crate::recovery::RecoveryMode;
use crate::token::Token;
use crate::tokenizer::tokenize;
use crate::xdata::{self, EntityXData};

/// Reads a token stream into a [`ParsedDxf`].
pub struct Parser {
    /// The complete group stream. Owned: the parser is `'static` with respect
    /// to the tokens it reads.
    tokens: Vec<Token>,
    /// Index of the current token. `tokens.len()` means end of stream.
    pos: usize,
    limits: ParseLimits,
    recovery: RecoveryMode,
    /// The document under construction.
    dxf: ParsedDxf,
    /// Handles already seen, to report duplicates instead of silently keeping
    /// the last one.
    seen_handles: BTreeSet<String>,
    /// Line of the most recently consumed token, used to report a location when
    /// the stream has run out.
    last_line: usize,
}

/// Everything gathered for one entity before it is interpreted.
struct EntityRaw {
    handle: Option<String>,
    layer: String,
    layer_seen: bool,
    groups: Vec<RawGroup>,
    xdata: EntityXData,
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
    const fn at_end(&self) -> bool {
        self.pos >= self.tokens.len()
    }

    /// Group code of the current token, or `None` at end of stream.
    fn code_here(&self) -> Option<i32> {
        self.tokens.get(self.pos).map(|t| t.code)
    }

    /// `true` when the current token is `0/<marker>`.
    fn at_marker(&self, marker: &str) -> bool {
        self.tokens
            .get(self.pos)
            .is_some_and(|t| t.is_marker(marker))
    }

    /// `true` when the current token has the given group code.
    fn at_code(&self, code: i32) -> bool {
        self.code_here() == Some(code)
    }

    /// Consumes the current token, returning it.
    fn bump(&mut self) -> Option<Token> {
        let token = self.tokens.get(self.pos).cloned();
        if let Some(t) = &token {
            self.last_line = t.line;
            self.pos += 1;
        }
        token
    }

    /// Source line to use when reporting a problem at the current position.
    fn line_here(&self) -> usize {
        self.tokens.get(self.pos).map_or(self.last_line, |t| t.line)
    }

    /// Consumes the current token, requiring a specific group code.
    fn expect_code(&mut self, code: i32) -> DxfResult<String> {
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
    fn expect_marker(&mut self, marker: &'static str) -> DxfResult<()> {
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
    fn skip_to_marker(&mut self, marker: &str) -> bool {
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
    fn report(&mut self, error: DxfError) -> DxfResult<()> {
        if self.recovery.is_strict() {
            return Err(error);
        }
        self.record(&error);
        Ok(())
    }

    /// Records a warning. Warnings never abort the parse.
    fn warn(&mut self, code: &str, message: impl Into<String>, line: usize) {
        self.dxf
            .add_diagnostic(Diagnostic::warning(code, message).with_context("line", line));
    }

    /// Records an error as a diagnostic, with the line attached.
    fn record(&mut self, error: &DxfError) {
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
                    // tokens. Under Strict that failure is the result.
                    if self.recovery.is_strict() {
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

    // -- sections ----------------------------------------------------------

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

    /// Reads HEADER variables.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first variable of the section.
    /// * **On exit**: current token is `0/ENDSEC`, *not* consumed.
    fn read_header(&mut self) -> DxfResult<()> {
        while !self.at_end() {
            if self.at_marker(gcode::ENDSEC) {
                return Ok(());
            }

            if self.at_code(gcode::HEADER_VARIABLE) {
                let variable = self.expect_code(gcode::HEADER_VARIABLE)?;
                self.read_header_value(&variable)?;
            } else {
                self.bump();
            }
        }

        Ok(())
    }

    /// Reads the value group(s) belonging to one HEADER variable.
    ///
    /// A HEADER record is `9 <name>` followed by one or more value groups. How
    /// many depends on the variable, so this consumes the groups the variable
    /// needs and no more.
    /// # Errors
    ///
    /// Propagates any header value that fails to convert. Under
    /// [`RecoveryMode::Recover`] the failure is recorded as a diagnostic and
    /// reading continues; under [`RecoveryMode::Strict`] it aborts the parse.
    fn read_header_value(&mut self, variable: &str) -> DxfResult<()> {
        match variable {
            "$ACADVER" | "$DWGCODEPAGE" | "$HANDSEED" => {
                if let Some(value) = self.bump() {
                    match variable {
                        "$ACADVER" => self.dxf.header.acad_version = Some(value.value),
                        "$DWGCODEPAGE" => self.dxf.header.dwg_codepage = Some(value.value),
                        _ => {}
                    }
                }
            }
            "$INSUNITS" => {
                if let Some(token) = self.bump() {
                    let group = RawGroup {
                        code: token.code,
                        value: token.value,
                        line: token.line,
                    };
                    match integerish_field(&group, "HEADER") {
                        Ok(v) => self.dxf.header.insunits = Some(v),
                        Err(e) => self.report(e)?,
                    }
                }
            }
            "$EXTMIN" => self.dxf.header.extmin = self.read_point3("$EXTMIN")?,
            "$EXTMAX" => self.dxf.header.extmax = self.read_point3("$EXTMAX")?,
            "$LIMMIN" => self.dxf.header.limmin = self.read_point2("$LIMMIN")?,
            "$LIMMAX" => self.dxf.header.limmax = self.read_point2("$LIMMAX")?,
            _ => {
                // Unknown variable: consume its single value group so the next
                // 9/<name> is not mistaken for this variable's value.
                self.bump();
            }
        }
        Ok(())
    }

    /// Reads a HEADER point given as an X/Y/Z triple.
    fn read_point3(&mut self, variable: &str) -> DxfResult<Option<[f64; 3]>> {
        let mut out: [Option<f64>; 3] = [None, None, None];
        let mut seen = 0usize;
        let mut failed = false;
        while seen < 3 {
            let Some(code) = self.code_here() else { break };
            let index = match code {
                gcode::X => 0,
                gcode::Y => 1,
                gcode::Z => 2,
                // Anything else ends this variable's value groups.
                _ => break,
            };
            let Some(token) = self.bump() else { break };
            let group = RawGroup {
                code: token.code,
                value: token.value,
                line: token.line,
            };
            match f64_field(&group, variable) {
                Ok(v) => out[index] = Some(v),
                Err(e) => {
                    failed = true;
                    self.report(e)?;
                }
            }
            seen += 1;
        }

        // No value groups at all: the variable is simply absent.
        if seen == 0 || failed {
            // A failed conversion has already been reported with full context.
            return Ok(None);
        }

        if let [Some(x), Some(y), Some(z)] = out {
            return Ok(Some([x, y, z]));
        }
        let line = self.last_line;
        Err(DxfError::malformed_stream(
            line,
            format!("{variable} needs X, Y and Z header groups, got {seen}"),
        ))
    }

    /// Reads a HEADER point given as an X/Y pair.
    ///
    /// # Errors
    ///
    /// Propagates the same failures as [`Parser::read_point3`].
    fn read_point2(&mut self, variable: &str) -> DxfResult<Option<[f64; 2]>> {
        let Some(point) = self.read_point3(variable)? else {
            return Ok(None);
        };
        Ok(Some([point[0], point[1]]))
    }

    // -- tables ------------------------------------------------------------

    /// Reads the TABLES section.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first token of the section body.
    /// * **On exit**: current token is `0/ENDSEC`, *not* consumed.
    fn read_tables(&mut self) -> DxfResult<()> {
        while !self.at_end() {
            if self.at_marker(gcode::ENDSEC) {
                return Ok(());
            }
            if self.at_marker(gcode::TABLE) {
                self.read_table()?;
            } else {
                self.bump();
            }
        }
        Ok(())
    }

    /// Reads one `TABLE`.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is `0/TABLE`, not consumed.
    /// * **On exit**: current token is *after* `ENDTAB`. `TABLE` and `ENDTAB`
    ///   are each consumed exactly once, here.
    fn read_table(&mut self) -> DxfResult<()> {
        self.bump(); // TABLE
        let name = self.expect_code(gcode::NAME)?;

        match name.as_str() {
            gcode::TABLE_LAYER => self.read_layer_table()?,
            gcode::TABLE_APPID => self.read_appid_table(),
            other => {
                let line = self.last_line;
                self.warn(
                    "DXF-TABLE-001",
                    format!("skipping unsupported table {other:?}"),
                    line,
                );
                if !self.skip_to_marker(gcode::ENDTAB) {
                    return Err(DxfError::unexpected_eof(self.last_line));
                }
            }
        }

        self.expect_marker(gcode::ENDTAB)
    }

    /// Reads the LAYER table records.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first token after the table name.
    /// * **On exit**: current token is `0/ENDTAB`, *not* consumed.
    fn read_layer_table(&mut self) -> DxfResult<()> {
        while !self.at_end() {
            if self.at_marker(gcode::ENDTAB) {
                return Ok(());
            }
            if self.at_marker(gcode::RECORD_LAYER) {
                self.read_layer_record()?;
            } else {
                self.bump();
            }
        }
        Ok(())
    }

    /// Reads one LAYER table record.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is `0/LAYER`, not consumed.
    /// * **On exit**: current token is the next `0/...` marker, not consumed.
    fn read_layer_record(&mut self) -> DxfResult<()> {
        self.bump(); // LAYER

        let mut layer = ParsedLayer {
            name: String::new(),
            color: ParsedLayer::DEFAULT_COLOR,
            line_weight: ParsedLayer::DEFAULT_LINE_WEIGHT,
            visible: true,
            locked: false,
            frozen: false,
            plot: true,
            line_type: String::new(),
        };

        while !self.at_end() {
            if self.at_code(gcode::MARKER) {
                break;
            }

            let Some(token) = self.bump() else { break };
            let group = RawGroup {
                code: token.code,
                value: token.value,
                line: token.line,
            };

            let outcome: DxfResult<()> = match group.code {
                gcode::NAME => {
                    layer.name.clone_from(&group.value);
                    Ok(())
                }
                gcode::LINE_TYPE_NAME => {
                    layer.line_type.clone_from(&group.value);
                    Ok(())
                }
                gcode::COLOR => integerish_field(&group, "LAYER").map(|v| layer.color = v),
                gcode::LAYER_ON_OFF => {
                    integerish_field(&group, "LAYER").map(|v| layer.visible = v != 0)
                }
                gcode::LINE_WEIGHT => f64_field(&group, "LAYER").map(|v| layer.line_weight = v),
                gcode::FLAGS => integerish_field(&group, "LAYER").map(|flags| {
                    layer.frozen = flags & 1 != 0;
                    layer.locked = flags & 4 != 0;
                    layer.plot = flags & 16 == 0;
                }),
                _ => Ok(()),
            };

            if let Err(e) = outcome {
                self.report(e)?;
            }
        }

        if layer.name.is_empty() {
            let line = self.last_line;
            self.warn(
                "DXF-LAYER-002",
                "ignoring a layer record with no name",
                line,
            );
            return Ok(());
        }

        self.limits.check_layer_count(self.dxf.layers.len() + 1)?;
        self.dxf.layers.push(layer);
        Ok(())
    }

    /// Reads the APPID table records.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first token after the table name.
    /// * **On exit**: current token is `0/ENDTAB`, *not* consumed.
    fn read_appid_table(&mut self) {
        while !self.at_end() {
            if self.at_marker(gcode::ENDTAB) {
                return;
            }
            if self.at_marker(gcode::RECORD_APPID) {
                self.bump(); // APPID
                let mut name = String::new();
                while !self.at_end() && !self.at_code(gcode::MARKER) {
                    let Some(token) = self.bump() else { break };
                    if token.code == gcode::NAME {
                        name = token.value;
                    }
                }
                if !name.is_empty() {
                    self.dxf.app_ids.push(ParsedAppId { name });
                }
            } else {
                self.bump();
            }
        }
    }

    // -- entities ----------------------------------------------------------

    /// Reads the ENTITIES section.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first token of the section body.
    /// * **On exit**: current token is `0/ENDSEC`, *not* consumed.
    fn read_entities(&mut self) -> DxfResult<()> {
        while !self.at_end() {
            if self.at_marker(gcode::ENDSEC) {
                return Ok(());
            }
            if self.at_code(gcode::MARKER) {
                // A structural marker here means the ENTITIES section is not
                // closed. Rewinding would spin forever, so consume it, report,
                // and stop: past this point the framing is untrustworthy.
                if self.at_pseudo_entity() {
                    let line = self.line_here();
                    let found = self.tokens[self.pos].value.clone();
                    self.bump();
                    let error = DxfError::malformed_stream(
                        line,
                        format!("structural marker {found:?} inside ENTITIES"),
                    );
                    if self.recovery.is_strict() {
                        return Err(error);
                    }
                    self.record(&error);
                    return Ok(());
                }
                self.read_entity()?;
            } else {
                self.bump();
            }
        }
        Ok(())
    }

    /// `true` when the current token is a `0/` marker that frames the file
    /// rather than naming an entity.
    fn at_pseudo_entity(&self) -> bool {
        self.at_marker(gcode::SECTION)
            || self.at_marker(gcode::EOF)
            || self.at_marker(gcode::TABLE)
            || self.at_marker(gcode::ENDTAB)
    }

    /// Collects one entity's groups, handle, layer and XDATA.
    ///
    /// Stops before the next `0/` marker, so the following entity or `ENDSEC` is
    /// left as the current token. Subclass markers are structure, not data, and
    /// are dropped rather than kept in `groups`.
    ///
    /// # Errors
    ///
    /// Propagates anything from [`xdata::read_block`] that
    /// [`Parser::report`] escalates, and any I/O-free structural failure while
    /// walking the token stream.
    fn read_entity_raw(&mut self) -> DxfResult<EntityRaw> {
        let mut handle: Option<String> = None;
        let mut layer = gcode::DEFAULT_LAYER.to_string();
        let mut layer_seen = false;
        let mut groups: Vec<RawGroup> = Vec::new();
        let mut xdata_out: EntityXData = EntityXData::new();

        while !self.at_end() {
            if self.at_code(gcode::MARKER) {
                break;
            }

            if self.at_code(gcode::XDATA_APPID) {
                match xdata::read_block(&self.tokens, &mut self.pos, &self.limits) {
                    Ok((appid, records)) => xdata::merge(&mut xdata_out, appid, records),
                    Err(e) => {
                        if let Some(token) = self.tokens.get(self.pos) {
                            self.last_line = token.line;
                        }
                        self.report(e)?;
                    }
                }
                continue;
            }

            let Some(token) = self.bump() else { break };

            match token.code {
                gcode::HANDLE => {
                    if let Some(previous) = handle.replace(token.value.clone()) {
                        self.report(DxfError::malformed_stream(
                            token.line,
                            format!(
                                "entity has a second handle ({previous} then {})",
                                token.value
                            ),
                        ))?;
                    }
                }
                gcode::LAYER => {
                    layer.clone_from(&token.value);
                    layer_seen = true;
                }
                gcode::SUBCLASS_MARKER => {}
                _ => groups.push(RawGroup {
                    code: token.code,
                    value: token.value,
                    line: token.line,
                }),
            }
        }

        Ok(EntityRaw {
            handle,
            layer,
            layer_seen,
            groups,
            xdata: xdata_out,
        })
    }

    /// Reads one entity.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is `0/<entity type>`, not consumed.
    /// * **On exit**: current token is the next `0/...` marker, not consumed.
    fn read_entity(&mut self) -> DxfResult<()> {
        let Some(type_token) = self.bump() else {
            return Ok(());
        };
        let type_name = type_token.value.clone();
        let entity_line = type_token.line;

        // Defence in depth: `read_entities` rejects these before we get here.
        // Consume rather than rewind, so no path can stall on the same token.
        if matches!(
            type_name.as_str(),
            gcode::SECTION | gcode::EOF | gcode::TABLE | gcode::ENDTAB
        ) {
            let error = DxfError::malformed_stream(
                entity_line,
                format!("structural marker {type_name:?} where an entity was expected"),
            );
            if self.recovery.is_strict() {
                return Err(error);
            }
            self.record(&error);
            return Ok(());
        }

        let mut raw = self.read_entity_raw()?;

        if raw.layer_seen && raw.layer.is_empty() {
            self.warn(
                "DXF-ENTITY-002",
                "entity declares an empty layer name, using the default layer",
                entity_line,
            );
            raw.layer = gcode::DEFAULT_LAYER.to_string();
        }

        if let Some(handle_value) = &raw.handle {
            if !self.seen_handles.insert(handle_value.clone()) {
                let line = entity_line;
                self.warn(
                    "DXF-HANDLE-001",
                    format!("duplicate entity handle {handle_value:?}"),
                    line,
                );
            }
        }

        // A classic POLYLINE owns the following VERTEX records and its SEQEND.
        let vertices = if type_name == "POLYLINE" {
            self.read_polyline_vertices(&mut raw.xdata)?
        } else {
            Vec::new()
        };

        if let Err(e) = self.limits.check_polyline_vertices(vertices.len()) {
            return self.report(e);
        }

        let entity_type = match entities::interpret(&type_name, &raw.groups, &vertices) {
            Ok(t) => t,
            Err(e) => {
                if self.recovery.is_strict() {
                    return Err(e);
                }
                self.record(&e);
                ParsedEntityType::Unknown {
                    type_name: type_name.clone(),
                    raw_groups: raw.groups.clone(),
                }
            }
        };

        if !entities::is_interpreted(&type_name) {
            let line = entity_line;
            self.warn(
                "DXF-ENTITY-001",
                format!("entity type {type_name:?} was not interpreted"),
                line,
            );
        }

        self.limits
            .check_entity_count(self.dxf.entities.len() + 1)?;
        self.dxf.entities.push(ParsedEntity {
            handle: raw.handle,
            layer: raw.layer,
            entity_type,
            properties: bcad_format::PropertyMap::new(),
            xdata: raw.xdata,
        });

        Ok(())
    }

    /// Reads the `VERTEX` records of a classic `POLYLINE` and its `SEQEND`.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first `0/VERTEX`, or something else
    ///   if the polyline has no vertices.
    /// * **On exit**: current token is *after* `0/SEQEND`. `SEQEND` is
    ///   consumed exactly once, here.
    fn read_polyline_vertices(
        &mut self,
        entity_xdata: &mut EntityXData,
    ) -> DxfResult<Vec<Vec<RawGroup>>> {
        let mut records: Vec<Vec<RawGroup>> = Vec::new();

        while self.at_marker("VERTEX") {
            self.bump(); // VERTEX
            let mut groups: Vec<RawGroup> = Vec::new();

            while !self.at_end() && !self.at_code(gcode::MARKER) {
                if self.at_code(gcode::XDATA_APPID) {
                    match xdata::read_block(&self.tokens, &mut self.pos, &self.limits) {
                        Ok((appid, records_for_vertex)) => {
                            xdata::merge(entity_xdata, appid, records_for_vertex);
                        }
                        Err(e) => {
                            self.report(e)?;
                        }
                    }
                    continue;
                }
                let Some(token) = self.bump() else { break };
                groups.push(RawGroup {
                    code: token.code,
                    value: token.value,
                    line: token.line,
                });
            }

            records.push(groups);
        }

        if self.at_marker("SEQEND") {
            self.bump();
        } else {
            let line = self.line_here();
            self.warn(
                "DXF-ENTITY-003",
                "POLYLINE is not terminated by a SEQEND record",
                line,
            );
        }

        Ok(records)
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

use std::collections::BTreeMap;

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
