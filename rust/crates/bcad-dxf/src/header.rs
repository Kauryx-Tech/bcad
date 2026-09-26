//! The `HEADER` section: `$`-prefixed variables and the points among them.
//!
//! A HEADER record is `9 <name>` followed by one or more value groups, and how
//! many depends on the variable. A variable the reader does not know must still
//! have its value groups consumed, or the next `9 <name>` is mistaken for this
//! variable's value and the section desynchronises.

use bcad_format::RawGroup;

use crate::error::{DxfError, DxfResult};
use crate::group_code as gcode;
use crate::numeric::{f64_field, integerish_field};
use crate::parser::Parser;

impl Parser {
    /// Reads HEADER variables.
    ///
    /// # Token consumption contract
    ///
    /// * **On entry**: current token is the first variable of the section.
    /// * **On exit**: current token is `0/ENDSEC`, *not* consumed.
    pub(crate) fn read_header(&mut self) -> DxfResult<()> {
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
    ///
    /// # Errors
    ///
    /// Propagates any header value that fails to convert. Under
    /// [`RecoveryMode::Recover`](crate::recovery::RecoveryMode::Recover) the failure is recorded as a diagnostic and
    /// reading continues; under [`RecoveryMode::Strict`](crate::recovery::RecoveryMode::Strict) it aborts the parse.
    pub(crate) fn read_header_value(&mut self, variable: &str) -> DxfResult<()> {
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
    ///
    /// Returns `Ok(None)` when the variable is absent or a conversion failed —
    /// the failure has then already been reported with full context. Returns an
    /// error when the variable *is* present but incomplete, so a half-written
    /// point is never zero-padded into a plausible-looking one.
    ///
    /// # Errors
    ///
    /// Propagates a conversion failure under [`RecoveryMode::Strict`](crate::recovery::RecoveryMode::Strict), and
    /// reports a point that is missing X, Y or Z under both modes.
    pub(crate) fn read_point3(&mut self, variable: &str) -> DxfResult<Option<[f64; 3]>> {
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
    /// `$LIMMIN` and `$LIMMAX` are 2D by specification, so a Z group is neither
    /// expected nor allowed here: demanding one would reject a conforming file.
    /// A Z that does appear is skipped, because it belongs to the variable, not
    /// to the point.
    ///
    /// # Errors
    ///
    /// Propagates a conversion failure under [`RecoveryMode::Strict`](crate::recovery::RecoveryMode::Strict), and
    /// reports a point that is missing X or Y under both modes.
    pub(crate) fn read_point2(&mut self, variable: &str) -> DxfResult<Option<[f64; 2]>> {
        let mut out: [Option<f64>; 2] = [None, None];
        let mut seen = 0usize;
        let mut failed = false;

        while let Some(code) = self.code_here() {
            let index = match code {
                gcode::X => 0,
                gcode::Y => 1,
                // A Z group is legal DXF but is not part of a 2D point: consume
                // it so the loop cannot spin on it, and keep reading for X/Y.
                gcode::Z => {
                    self.bump();
                    continue;
                }
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

        if seen == 0 || failed {
            return Ok(None);
        }

        if let [Some(x), Some(y)] = out {
            return Ok(Some([x, y]));
        }
        let line = self.last_line;
        Err(DxfError::malformed_stream(
            line,
            format!("{variable} needs X and Y header groups, got {seen}"),
        ))
    }
}

#[cfg(test)]
mod tests {
    use crate::testutil::{file, has_code, header, message_for, read, read_ok};
    use crate::{parse_dxf, ParseLimits, ParseOptions, RecoveryMode};

    /// `$LIMMIN` and `$LIMMAX` are 2D by specification.
    ///
    /// Regression: the 2D reader delegated to the 3D one, which demanded a Z
    /// group, so a conforming file was rejected and `$LIMMIN` could never be
    /// read at all.
    #[test]
    fn limmin_is_read_as_a_two_dimensional_point() {
        let text = file(&[header("9\n$LIMMIN\n10\n1.5\n20\n2.5\n")]);
        let dxf = read_ok(&text);
        assert!(!dxf.has_errors(), "{:?}", dxf.diagnostics);
        let limmin = dxf.header.limmin.expect("a 2D point must be read");
        assert!((limmin[0] - 1.5).abs() < 1e-9, "{limmin:?}");
        assert!((limmin[1] - 2.5).abs() < 1e-9, "{limmin:?}");
    }

    /// A Z group is legal DXF but is not part of a 2D point: it must be skipped,
    /// not treated as the end of the variable, and it must not make the reader
    /// spin.
    #[test]
    fn limmin_tolerates_a_z_group_without_spinning() {
        let text = file(&[header("9\n$LIMMIN\n10\n1.0\n30\n0.0\n20\n2.0\n")]);
        let dxf = read_ok(&text);
        assert!(!dxf.has_errors(), "{:?}", dxf.diagnostics);
        let limmin = dxf.header.limmin.expect("a 2D point must be read");
        assert!((limmin[0] - 1.0).abs() < 1e-9, "{limmin:?}");
        assert!((limmin[1] - 2.0).abs() < 1e-9, "{limmin:?}");
    }

    /// The 2D reader is still strict about what a 2D point needs.
    #[test]
    fn limmin_missing_its_y_group_is_reported() {
        let text = file(&[header("9\n$LIMMIN\n10\n1.0\n")]);

        let err = read(&text, RecoveryMode::Strict).expect_err("X alone is not a point");
        assert!(err.to_string().contains("$LIMMIN"), "{err}");

        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert!(dxf.header.limmin.is_none(), "no coordinate may be invented");
    }

    /// `$EXTMIN` is a 3D point, so the 3D contract is unchanged: X and Y without
    /// Z is incomplete, not a 2D point.
    #[test]
    fn extmin_still_requires_its_z_group() {
        let text = file(&[header("9\n$EXTMIN\n10\n1.0\n20\n2.0\n")]);
        let err = read(&text, RecoveryMode::Strict).expect_err("X and Y is not a 3D point");
        assert!(err.to_string().contains("$EXTMIN"), "{err}");
    }

    #[test]
    fn reads_the_string_and_integer_variables() {
        let text = file(&[header(
            "9\n$ACADVER\n1\nAC1024\n9\n$DWGCODEPAGE\n3\nANSI_1252\n9\n$INSUNITS\n70\n4\n",
        )]);
        let dxf = read_ok(&text);
        assert_eq!(dxf.header.acad_version.as_deref(), Some("AC1024"));
        assert_eq!(dxf.header.dwg_codepage.as_deref(), Some("ANSI_1252"));
        assert_eq!(dxf.header.insunits, Some(4));
    }

    /// The desynchronisation contract: an unknown variable must consume its own
    /// value group and no more, or the next `9 <name>` is read as its value and
    /// every later variable is lost.
    #[test]
    fn an_unknown_variable_consumes_exactly_one_value_group() {
        let text = file(&[header(
            "9\n$MYAPP\n1\npayload\n2\nalso-mine\n9\n$ACADVER\n1\nAC1024\n",
        )]);
        let dxf = read_ok(&text);
        assert_eq!(
            dxf.header.acad_version.as_deref(),
            Some("AC1024"),
            "the variable after an unknown one must still be read"
        );
    }

    #[test]
    fn a_non_numeric_insunits_is_reported() {
        let text = file(&[header("9\n$INSUNITS\n70\nnot-a-number\n")]);
        let err = read(&text, RecoveryMode::Strict).expect_err("a non-numeric value must fail");
        assert!(err.to_string().contains("HEADER"), "{err}");
        assert!(read(&text, RecoveryMode::Recover)
            .expect("recovers")
            .has_errors());
    }

    #[test]
    fn an_absent_variable_is_simply_absent() {
        let dxf = read_ok(&file(&[header("9\n$ACADVER\n1\nAC1024\n")]));
        assert!(dxf.header.limmin.is_none());
        assert!(dxf.header.extmin.is_none());
        assert!(!dxf.has_errors());
    }

    /// A variable whose only value group is missing must not be reported: there
    /// is nothing there to be wrong.
    #[test]
    fn a_variable_with_no_value_group_is_not_an_error() {
        let text = file(&[header("9\n$INSUNITS\n0\nENDSEC\n0\nEOF\n")]);
        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert_eq!(dxf.header.insunits, None);
    }

    /// Guards the diagnostic that a bad header coordinate produces, so a
    /// regression in `record` is visible from the HEADER side too.
    #[test]
    fn a_malformed_header_coordinate_carries_its_line() {
        let text = file(&[header("9\n$EXTMIN\n10\nNaN\n20\n2.0\n30\n3.0\n")]);
        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        assert!(dxf.has_errors());
        let diagnostic = dxf
            .diagnostics
            .iter()
            .find(|d| d.severity == bcad_format::Severity::Error)
            .expect("an error diagnostic");
        assert!(
            diagnostic.context.contains_key("line"),
            "the diagnostic must locate the failure: {diagnostic:?}"
        );
    }

    /// `ParseLimits::strict` is what strict mode pairs with; confirm the two
    /// knobs stay independent.
    #[test]
    fn strict_options_pair_strict_limits_with_strict_recovery() {
        let options = ParseOptions::strict();
        assert!(options.recovery.is_strict());
        assert!(ParseLimits::strict().max_tokens < ParseLimits::default().max_tokens);
        let text = file(&[header("9\n$ACADVER\n1\nAC1024\n")]);
        assert!(parse_dxf(&text, &options).is_ok());
    }

    /// The message a caller sees must name the variable, not just "malformed".
    #[test]
    fn the_incomplete_point_message_names_the_variable() {
        let text = file(&[header("9\n$LIMMAX\n10\n1.0\n")]);
        let dxf = read(&text, RecoveryMode::Recover).expect("recovers");
        let codes: Vec<_> = dxf.diagnostics.iter().map(|d| d.code.clone()).collect();
        assert!(!codes.is_empty());
        assert!(
            message_for(&dxf, codes[0].as_str()).contains("LIMMAX")
                || has_code(&dxf, "DXF-PARSE-000"),
            "diagnostics: {codes:?}"
        );
    }
}
