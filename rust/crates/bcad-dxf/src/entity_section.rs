//! The `ENTITIES` section: collecting, framing and interpreting entities.
//!
//! This module owns everything between `0/SECTION`/`2`/`ENTITIES` and
//! `0/ENDSEC`. Its hard problem is framing: a `0/` marker either names the next
//! entity or frames the file, and mistaking one for the other is what makes a
//! parser rewind onto a token it already consumed and spin forever.

use bcad_format::{ParsedEntity, ParsedEntityType, PropertyMap, RawGroup};

use crate::entities;
use crate::error::{DxfError, DxfResult};
use crate::group_code as gcode;
use crate::parser::Parser;
use crate::xdata::{self, EntityXData};

/// Everything gathered for one entity before it is interpreted.
struct EntityRaw {
    handle: Option<String>,
    layer: String,
    layer_seen: bool,
    groups: Vec<RawGroup>,
    xdata: EntityXData,
}

impl Parser {
    /// Reads the ENTITIES section.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first token of the section body.
    /// * **On exit**: current token is `0/ENDSEC`, *not* consumed.
    pub(crate) fn read_entities(&mut self) -> DxfResult<()> {
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
    pub(crate) fn at_pseudo_entity(&self) -> bool {
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
    pub(crate) fn read_entity(&mut self) -> DxfResult<()> {
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
            properties: PropertyMap::new(),
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
    pub(crate) fn read_polyline_vertices(
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
}

#[cfg(test)]
mod tests {
    use crate::testutil::{codes, entities, file, has_code, read, read_ok};
    use crate::{parse_dxf, ParseLimits, ParseOptions, ParsedEntityType, RecoveryMode};

    /// An entity the reader does not model is kept whole, with a warning, rather
    /// than dropped: a later pass may still be able to use it.
    #[test]
    fn an_uninterpreted_entity_is_kept_whole() {
        let text = file(&[entities(
            "0\nSPLINE\n5\nA1\n8\n0\n70\n8\n10\n0.0\n20\n0.0\n30\n0.0\n",
        )]);
        let dxf = read_ok(&text);
        assert!(has_code(&dxf, "DXF-ENTITY-001"), "{:?}", codes(&dxf));
        assert_eq!(dxf.entity_count(), 1);
        assert_eq!(dxf.entities[0].handle.as_deref(), Some("A1"));
        match &dxf.entities[0].entity_type {
            ParsedEntityType::Unknown {
                type_name,
                raw_groups,
            } => {
                assert_eq!(type_name, "SPLINE");
                assert!(!raw_groups.is_empty(), "the raw groups must survive");
            }
            other => panic!("expected Unknown, got {other:?}"),
        }
    }

    /// The recoverable case of the convention: an entity whose geometry cannot
    /// be interpreted degrades to `Unknown` and the *next* entity still reads.
    #[test]
    fn an_uninterpretable_entity_degrades_and_reading_continues() {
        // A CIRCLE needs a positive radius; 40/nope is a data defect, not a
        // framing one.
        let text = file(&[entities(
            "0\nCIRCLE\n5\nBAD\n8\n0\n10\n1.0\n20\n1.0\n30\n0.0\n40\nnope\n\
             0\nCIRCLE\n5\nGOOD\n8\n0\n10\n5.0\n20\n5.0\n30\n0.0\n40\n2.0\n",
        )]);

        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert!(dxf.has_errors(), "the bad radius must be reported");
        assert_eq!(dxf.entity_count(), 2, "both entities are kept");
        assert!(matches!(
            dxf.entities[0].entity_type,
            ParsedEntityType::Unknown { .. }
        ));
        assert!(
            matches!(dxf.entities[1].entity_type, ParsedEntityType::Circle { .. }),
            "the entity after the defect must still be interpreted: {:?}",
            dxf.entities[1].entity_type
        );

        // The fatal case: under Strict the import cannot continue.
        assert!(read(&text, RecoveryMode::Strict).is_err());
    }

    /// An empty layer name cannot be resolved, so it falls back to the default
    /// and says so rather than leaving the entity on a layer named "".
    #[test]
    fn an_empty_layer_name_falls_back_to_the_default_layer() {
        let text = file(&[entities(
            "0\nLINE\n5\nA1\n8\n\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n",
        )]);
        let dxf = read_ok(&text);
        assert!(has_code(&dxf, "DXF-ENTITY-002"), "{:?}", codes(&dxf));
        assert_eq!(dxf.entities[0].layer, "0");
    }

    /// Two entities sharing a handle is a real defect: a CAD file keyed by handle
    /// would resolve one of them arbitrarily.
    #[test]
    fn a_duplicate_entity_handle_is_reported() {
        let text = file(&[entities(
            "0\nLINE\n5\nSAME\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n\
             0\nLINE\n5\nSAME\n8\n0\n10\n2.0\n20\n0.0\n11\n3.0\n21\n0.0\n",
        )]);
        let dxf = read_ok(&text);
        assert!(has_code(&dxf, "DXF-HANDLE-001"), "{:?}", codes(&dxf));
        assert_eq!(dxf.entity_count(), 2, "neither entity is dropped");
    }

    /// Two handle groups on one entity is a different defect from a duplicate
    /// across entities, and is reported as an error rather than a warning.
    #[test]
    fn a_second_handle_group_on_one_entity_is_reported() {
        let text = file(&[entities(
            "0\nLINE\n5\nA1\n5\nA2\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n",
        )]);
        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert!(dxf.has_errors(), "a second handle must be reported");
        assert!(read(&text, RecoveryMode::Strict).is_err());
    }

    /// A POLYLINE without its SEQEND leaves the reader unable to know where the
    /// polyline stops, so it is reported — and the vertices read so far are kept
    /// rather than discarded.
    #[test]
    fn a_polyline_without_seqend_is_reported_and_keeps_its_vertices() {
        let text = file(&[entities(
            "0\nPOLYLINE\n5\nP1\n8\n0\n70\n0\n\
             0\nVERTEX\n8\n0\n10\n0.0\n20\n0.0\n30\n0.0\n\
             0\nVERTEX\n8\n0\n10\n1.0\n20\n0.0\n30\n0.0\n\
             0\nENDSEC\n",
        )]);
        let dxf = read_ok(&text);
        assert!(has_code(&dxf, "DXF-ENTITY-003"), "{:?}", codes(&dxf));
        match &dxf.entities[0].entity_type {
            ParsedEntityType::Polyline { vertices, .. } => {
                assert_eq!(vertices.len(), 2, "{vertices:?}");
            }
            other => panic!("expected Polyline, got {other:?}"),
        }
    }

    /// The `SEQEND` consumption contract: a well-formed polyline must consume it
    /// exactly once, so the entity after it still reads as an entity.
    #[test]
    fn a_well_formed_polyline_does_not_swallow_the_next_entity() {
        let text = file(&[entities(
            "0\nPOLYLINE\n5\nP1\n8\n0\n70\n0\n\
             0\nVERTEX\n8\n0\n10\n0.0\n20\n0.0\n30\n0.0\n\
             0\nVERTEX\n8\n0\n10\n1.0\n20\n0.0\n30\n0.0\n\
             0\nSEQEND\n\
             0\nLINE\n5\nA1\n8\n0\n10\n5.0\n20\n5.0\n11\n6.0\n21\n6.0\n",
        )]);
        let dxf = read_ok(&text);
        assert_eq!(dxf.entity_count(), 2, "{:?}", dxf.entities);
        assert!(!has_code(&dxf, "DXF-ENTITY-003"), "{:?}", codes(&dxf));
    }

    /// A degenerate polyline is legal and must not be an error.
    #[test]
    fn a_polyline_with_no_vertex_is_read() {
        let text = file(&[entities("0\nPOLYLINE\n5\nP1\n8\n0\n70\n0\n0\nSEQEND\n")]);
        let dxf = read_ok(&text);
        assert_eq!(dxf.entity_count(), 1);
        assert!(!dxf.has_errors(), "{:?}", codes(&dxf));
    }

    /// The polyline-vertex ceiling is a refusal, like every other ceiling.
    #[test]
    fn the_polyline_vertex_limit_is_enforced() {
        let text = file(&[entities(
            "0\nPOLYLINE\n5\nP1\n8\n0\n70\n0\n\
             0\nVERTEX\n8\n0\n10\n0.0\n20\n0.0\n30\n0.0\n\
             0\nVERTEX\n8\n0\n10\n1.0\n20\n0.0\n30\n0.0\n\
             0\nVERTEX\n8\n0\n10\n2.0\n20\n0.0\n30\n0.0\n\
             0\nSEQEND\n",
        )]);
        let err = parse_dxf(
            &text,
            &ParseOptions {
                limits: ParseLimits {
                    max_polyline_vertices: 2,
                    ..ParseLimits::default()
                },
                ..ParseOptions::default()
            },
        )
        .expect_err("the ceiling must be enforced");
        assert!(err.to_string().contains("vertex"), "{err}");
    }

    /// Group 100 is a subclass marker: structure, not data. It must not end up in
    /// the interpreted entity's groups.
    #[test]
    fn subclass_markers_are_structure_not_data() {
        let text = file(&[entities(
            "0\nLINE\n5\nA1\n8\n0\n100\nAcDbEntity\n\
             10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n",
        )]);
        let dxf = read_ok(&text);
        assert!(matches!(
            dxf.entities[0].entity_type,
            ParsedEntityType::Line { .. }
        ));
    }

    /// An entity with no layer group at all lands on the default layer without
    /// being reported: absence is not a defect.
    #[test]
    fn an_entity_without_a_layer_group_uses_the_default() {
        let text = file(&[entities("0\nPOINT\n5\nA1\n10\n1.0\n20\n2.0\n")]);
        let dxf = read_ok(&text);
        assert_eq!(dxf.entities[0].layer, "0");
        assert!(!has_code(&dxf, "DXF-ENTITY-002"), "{:?}", codes(&dxf));
    }

    /// A structural marker where an entity name belongs is the framing hazard:
    /// the reader must consume and report, never rewind, or it spins forever.
    #[test]
    fn a_structural_marker_where_an_entity_belongs_terminates() {
        let text = file(&[entities(
            "0\nLINE\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n0\nTABLE\n",
        )]);
        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert!(dxf.has_errors());
        assert_eq!(dxf.entity_count(), 1, "the LINE before the break is kept");
        assert!(read(&text, RecoveryMode::Strict).is_err());
    }

    /// XDATA is read on the entity, not as geometry groups.
    #[test]
    fn xdata_is_attached_to_its_entity() {
        let text = file(&[entities(
            "0\nLINE\n5\nA1\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n\
             1001\nMYAPP\n1000\npayload\n",
        )]);
        let dxf = read_ok(&text);
        assert_eq!(dxf.entity_count(), 1);
        let xdata = &dxf.entities[0].xdata;
        let records = xdata
            .get("MYAPP")
            .unwrap_or_else(|| panic!("the XDATA block must be kept: {xdata:?}"));
        assert!(!records.is_empty(), "{xdata:?}");
    }

    /// The entity ceiling is enforced by the section reader, not by the geometry
    /// interpreters, so it holds for uninterpreted types too.
    #[test]
    fn the_entity_ceiling_counts_uninterpreted_entities() {
        let text = file(&[entities(
            "0\nSPLINE\n5\nA1\n8\n0\n\
             0\nSPLINE\n5\nA2\n8\n0\n\
             0\nSPLINE\n5\nA3\n8\n0\n",
        )]);
        assert!(
            parse_dxf(
                &text,
                &ParseOptions {
                    limits: ParseLimits {
                        max_entities: 2,
                        ..ParseLimits::default()
                    },
                    ..ParseOptions::default()
                }
            )
            .is_err(),
            "unknown entities count towards the ceiling"
        );
    }
}
