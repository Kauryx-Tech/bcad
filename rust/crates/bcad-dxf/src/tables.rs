//! The `TABLES` section: the `LAYER` and `APPID` records the reader keeps.
//!
//! Other tables (`STYLE`, `DIMSTYLE`, `VPORT`, …) hold no BCAD state, so they
//! are skipped whole and the skip is reported rather than passed off as
//! coverage.

use bcad_format::{ParsedAppId, ParsedLayer, RawGroup};

use crate::error::{DxfError, DxfResult};
use crate::group_code as gcode;
use crate::numeric::{f64_field, integerish_field};
use crate::parser::Parser;

impl Parser {
    /// Reads the TABLES section.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first token of the section body.
    /// * **On exit**: current token is `0/ENDSEC`, *not* consumed.
    pub(crate) fn read_tables(&mut self) -> DxfResult<()> {
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
    pub(crate) fn read_table(&mut self) -> DxfResult<()> {
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
    pub(crate) fn read_layer_table(&mut self) -> DxfResult<()> {
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
    pub(crate) fn read_layer_record(&mut self) -> DxfResult<()> {
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
                    // Group 290 counts *inverted* flags: 0 is on, 1 is off.
                    // Reading it as a boolean hides exactly the layers the
                    // author turned off. An absent flag leaves the `true`
                    // set when the record is created, which is also right.
                    integerish_field(&group, "LAYER").map(|v| layer.visible = v == 0)
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
    pub(crate) fn read_appid_table(&mut self) {
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
}

#[cfg(test)]
mod tests {
    use crate::testutil::{codes, entities, file, has_code, header, read, read_ok, tables};
    use crate::{parse_dxf, ParseLimits, ParseOptions, RecoveryMode};

    /// The whole LAYER record vocabulary, in one table.
    #[test]
    fn reads_a_layer_record_with_its_fields() {
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nWALLS\n62\n5\n6\nCONTINUOUS\n370\n0.5\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        assert!(!dxf.has_errors(), "{:?}", dxf.diagnostics);
        assert_eq!(dxf.layers.len(), 1);
        let layer = &dxf.layers[0];
        assert_eq!(layer.name, "WALLS");
        assert_eq!(layer.color, 5);
        assert_eq!(layer.line_type, "CONTINUOUS");
        assert!((layer.line_weight - 0.5).abs() < 1e-9, "{layer:?}");
        assert!(layer.visible, "{layer:?}");
    }

    /// Group 70 is a bitmask: 1 = frozen, 4 = locked, 16 = do not plot.
    #[test]
    fn layer_flags_map_to_frozen_locked_and_plot() {
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nFROZEN\n70\n1\n0\nLAYER\n2\nBOTH\n70\n5\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        let by_name = |n: &str| dxf.layers.iter().find(|l| l.name == n).expect("layer");

        let frozen = by_name("FROZEN");
        assert!(frozen.frozen, "{frozen:?}");
        assert!(!frozen.locked, "{frozen:?}");
        assert!(frozen.plot, "{frozen:?}");

        // 1 + 4 = frozen and locked, plot bit still clear.
        let both = by_name("BOTH");
        assert!(both.frozen && both.locked, "{both:?}");
        assert!(both.plot, "{both:?}");

        // 16 alone turns plotting off without freezing or locking.
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nNOPLOT\n70\n16\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        let noplot = &dxf.layers[0];
        assert!(!noplot.plot, "{noplot:?}");
        assert!(!noplot.frozen && !noplot.locked, "{noplot:?}");
    }

    /// Group 290 is a single flag whose zero means *on*. Both poles are pinned
    /// here, because the natural mistake is to read it as a boolean and land on
    /// exactly the inverse: every layer the author switched off becomes drawn.
    #[test]
    fn a_hidden_layer_is_marked_invisible() {
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nHIDDEN\n290\n1\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        assert!(!dxf.layers[0].visible, "{:?}", dxf.layers[0]);
    }

    /// The other pole: 0 means on, not off.
    #[test]
    fn an_explicitly_switched_on_layer_is_visible() {
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nSHOWN\n290\n0\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        assert!(dxf.layers[0].visible, "{:?}", dxf.layers[0]);
    }

    /// No flag at all: a layer is drawn unless something says otherwise.
    #[test]
    fn a_layer_with_no_visibility_flag_is_visible() {
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nPLAIN\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        assert!(dxf.layers[0].visible, "{:?}", dxf.layers[0]);
    }

    /// A nameless record cannot be referenced by anything, so it is reported and
    /// dropped rather than stored under an empty name.
    #[test]
    fn a_layer_record_without_a_name_is_reported_and_dropped() {
        let text = file(&[tables("0\nTABLE\n2\nLAYER\n0\nLAYER\n62\n3\n0\nENDTAB\n")]);
        let dxf = read_ok(&text);
        assert!(has_code(&dxf, "DXF-LAYER-002"), "{:?}", codes(&dxf));
        assert!(dxf.layers.is_empty(), "{:?}", dxf.layers);
    }

    /// The `ENDTAB` consumption contract: a skipped table must not eat the
    /// marker, or every table after it is lost.
    #[test]
    fn an_unsupported_table_is_skipped_and_the_next_one_still_reads() {
        let text = file(&[tables(
            "0\nTABLE\n2\nSTYLE\n0\nSTYLE\n2\nStandard\n0\nENDTAB\n\
             0\nTABLE\n2\nAPPID\n0\nAPPID\n2\nACAD\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        assert!(has_code(&dxf, "DXF-TABLE-001"), "{:?}", codes(&dxf));
        assert_eq!(
            dxf.app_ids.len(),
            1,
            "the APPID table after STYLE must read"
        );
        assert_eq!(dxf.app_ids[0].name, "ACAD");
    }

    #[test]
    fn reads_appid_records_and_ignores_nameless_ones() {
        let text = file(&[tables(
            "0\nTABLE\n2\nAPPID\n0\nAPPID\n2\nACAD\n0\nAPPID\n70\n0\n0\nENDTAB\n",
        )]);
        let dxf = read_ok(&text);
        assert_eq!(dxf.app_ids.len(), 1, "{:?}", dxf.app_ids);
        assert_eq!(dxf.app_ids[0].name, "ACAD");
    }

    /// A layer declared in TABLES silences the end-of-parse cross-check, which
    /// is the whole point of reading the table at all.
    #[test]
    fn a_declared_layer_silences_the_undefined_layer_warning() {
        let body = "0\nLINE\n8\nWALLS\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n";

        let undeclared = read_ok(&file(&[entities(body)]));
        assert!(
            has_code(&undeclared, "DXF-LAYER-001"),
            "{:?}",
            codes(&undeclared)
        );

        let declared = read_ok(&file(&[
            tables("0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nWALLS\n0\nENDTAB\n"),
            entities(body),
        ]));
        assert!(
            !has_code(&declared, "DXF-LAYER-001"),
            "a declared layer must not warn: {:?}",
            codes(&declared)
        );
    }

    /// The cross-check runs after the whole file, so a layer declared *after* the
    /// entity that uses it must still count as declared.
    #[test]
    fn a_layer_declared_after_its_entities_still_counts() {
        let text = file(&[
            entities("0\nLINE\n8\nLATE\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n"),
            tables("0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nLATE\n0\nENDTAB\n"),
        ]);
        let dxf = read_ok(&text);
        assert!(!has_code(&dxf, "DXF-LAYER-001"), "{:?}", codes(&dxf));
    }

    /// The layer-count ceiling is a refusal to go further, not a defect to
    /// record.
    ///
    /// Regression: under `Recover` the breach used to come back as `Ok` with a
    /// truncated layer list and one error diagnostic, so a caller that trusted
    /// `Ok` would save a partial drawing as if it were whole. `limits.rs`
    /// promises the opposite: "exceeding one aborts the parse".
    #[test]
    fn the_layer_count_limit_is_enforced() {
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n\
             0\nLAYER\n2\nL0\n\
             0\nLAYER\n2\nL1\n\
             0\nLAYER\n2\nL2\n\
             0\nLAYER\n2\nL3\n\
             0\nLAYER\n2\nL4\n\
             0\nENDTAB\n",
        )]);

        let options = ParseOptions {
            limits: ParseLimits {
                max_layers: 2,
                ..ParseLimits::default()
            },
            ..ParseOptions::default()
        };

        // Both recovery modes, because the ceiling is not negotiable.
        for recovery in [RecoveryMode::Recover, RecoveryMode::Strict] {
            let options = ParseOptions {
                recovery,
                ..options.clone()
            };
            let err = parse_dxf(&text, &options).expect_err("the ceiling must be enforced");
            assert!(err.to_string().contains("layer"), "{err}");
        }
    }

    /// The entity-count ceiling obeys the same rule as the layer ceiling.
    #[test]
    fn the_entity_count_limit_is_enforced_in_both_modes() {
        let text = file(&[entities(
            "0\nLINE\n5\nH0\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n\
             0\nLINE\n5\nH1\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n\
             0\nLINE\n5\nH2\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n\
             0\nLINE\n5\nH3\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n\
             0\nLINE\n5\nH4\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n",
        )]);

        let limits = ParseLimits {
            max_entities: 2,
            ..ParseLimits::default()
        };
        for recovery in [RecoveryMode::Recover, RecoveryMode::Strict] {
            let err = parse_dxf(
                &text,
                &ParseOptions {
                    recovery,
                    limits: limits.clone(),
                    strict_encoding: false,
                },
            )
            .expect_err("the ceiling must be enforced");
            assert!(err.to_string().contains("entit"), "{err}");
        }
    }

    /// A malformed field is reported but does not discard the rest of the record.
    #[test]
    fn a_malformed_layer_field_keeps_the_record() {
        let text = file(&[tables(
            "0\nTABLE\n2\nLAYER\n0\nLAYER\n2\nGOOD\n62\nnot-a-number\n0\nENDTAB\n",
        )]);
        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert!(dxf.has_errors(), "the bad colour must be reported");
        assert_eq!(dxf.layers.len(), 1, "the layer itself is still usable");
        assert_eq!(dxf.layers[0].name, "GOOD");
        assert!(read(&text, RecoveryMode::Strict).is_err());
    }

    /// A TABLES section that is never closed must not hang or silently succeed.
    #[test]
    fn an_unclosed_tables_section_terminates() {
        let text = file(&[
            header("9\n$ACADVER\n1\nAC1024\n"),
            tables("0\nTABLE\n2\nLAYER\n"),
        ]);
        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert!(!dxf.layers.is_empty() || dxf.has_errors());
    }
}
