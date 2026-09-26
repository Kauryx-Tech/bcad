//! XDATA blocks.
//!
//! An XDATA block starts with group `1001 <appid>` and continues with value
//! groups. Structured sub-records are delimited by `1002 {` and `1002 }`, and
//! the braces nest.
//!
//! Two rules matter here and are enforced by construction:
//!
//! * **Every** application identifier is kept, including ones this reader has
//!   never heard of. Filtering to a known-appid allowlist silently discards
//!   third-party data, which is data loss, not recovery.
//! * The block is attached to the entity that is being read, passed in as
//!   `out`. Reaching into a shared document to find "the last entity" is wrong:
//!   it attaches XDATA to the wrong entity the moment a future code path skips
//!   or reorders an entity, and it cannot express XDATA on an entity that has
//!   not been pushed yet.

use std::collections::BTreeMap;

use bcad_format::XDataRecord;

use crate::error::{DxfError, DxfResult};
use crate::group_code as gcode;
use crate::limits::ParseLimits;
use crate::token::Token;

/// The XDATA of a single entity, keyed by application identifier.
///
/// A `BTreeMap` rather than a `HashMap` so the parsed document serialises
/// deterministically: two runs over the same file produce identical output,
/// which is what makes the round-trip tests meaningful.
pub type EntityXData = BTreeMap<String, Vec<XDataRecord>>;

/// Reads one XDATA block.
///
/// # Token consumption contract
///
/// * **On entry**: `pos` addresses the `1001` group of the block, not consumed.
/// * **On exit**: `pos` addresses the first token *after* the block.
///
/// The block ends at whichever comes first: a `1002 }` at nesting depth 0, the
/// next `1001` (a following application), or a group code 0 (the next entity).
/// Each token is stepped over exactly once.
///
/// Returns the application identifier and its records. A file may attach the
/// same application twice to one entity; the records are appended rather than
/// overwritten.
///
/// # Errors
///
/// Returns [`DxfError::UnexpectedEof`] when the block is unterminated, and
/// [`DxfError::ResourceLimit`] when it exceeds [`ParseLimits::max_xdata_size`].
pub fn read_block(
    tokens: &[Token],
    pos: &mut usize,
    limits: &ParseLimits,
) -> DxfResult<(String, Vec<XDataRecord>)> {
    let Some(first) = tokens.get(*pos) else {
        return Err(DxfError::unexpected_eof(0));
    };

    if first.code != gcode::XDATA_APPID {
        return Err(DxfError::unexpected_group_code(
            gcode::XDATA_APPID,
            first.code,
            first.line,
        ));
    }

    let appid = first.value.clone();
    let appid_line = first.line;
    *pos += 1;

    let mut records: Vec<XDataRecord> = Vec::new();
    let mut payload_bytes: usize = 0;
    let mut depth: usize = 0;

    while let Some(token) = tokens.get(*pos) {
        // A group code 0 starts the next entity: the block ends unterminated.
        if token.code == crate::token::CODE_MARKER {
            break;
        }

        // A 1001 at depth 0 starts the next application block.
        if token.code == gcode::XDATA_APPID && depth == 0 {
            break;
        }

        if token.code == gcode::XDATA_CONTROL {
            if token.value == "{" {
                depth += 1;
                // The brace is structure, not data: it is not recorded.
                *pos += 1;
                continue;
            }
            if token.value == "}" {
                if depth == 0 {
                    // Closing brace without a matching opening one: consume it
                    // so the stream stays aligned, then report the problem.
                    *pos += 1;
                    return Err(DxfError::UnterminatedXData {
                        appid,
                        line: token.line,
                    });
                }
                depth -= 1;
                *pos += 1;
                // Closing a list does not end the block: further items may
                // follow for the same application. Only a 1001 at depth 0 or a
                // group code 0 terminates it.
                continue;
            }
        }

        payload_bytes += token.value.len();
        limits.check_xdata_size(payload_bytes)?;

        records.push(XDataRecord {
            code: token.code,
            value: token.value.clone(),
            line: token.line,
        });
        *pos += 1;
    }

    if depth != 0 {
        return Err(DxfError::UnterminatedXData {
            appid,
            line: appid_line,
        });
    }

    Ok((appid, records))
}

/// Merges one block into an entity's XDATA, appending on repeated applications.
pub fn merge(target: &mut EntityXData, appid: String, records: Vec<XDataRecord>) {
    target.entry(appid).or_default().extend(records);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn t(code: i32, value: &str, line: usize) -> Token {
        Token::new(code, value, line)
    }

    fn limits() -> ParseLimits {
        ParseLimits::default()
    }

    #[test]
    fn reads_a_simple_block() {
        let tokens = vec![
            t(gcode::XDATA_APPID, "ACAD", 10),
            t(gcode::XDATA_STRING, "hello", 11),
            t(1070, "42", 12),
        ];
        let mut pos = 0;
        let (appid, records) = read_block(&tokens, &mut pos, &limits()).expect("valid");
        assert_eq!(appid, "ACAD");
        assert_eq!(records.len(), 2);
        assert_eq!(records[0].value, "hello");
        assert_eq!(records[1].code, 1070);
        assert_eq!(pos, 3, "every token consumed exactly once");
    }

    #[test]
    fn keeps_unknown_applications() {
        let tokens = vec![t(gcode::XDATA_APPID, "VENDOR_XYZ", 10), t(1000, "v", 11)];
        let mut pos = 0;
        let (appid, _) = read_block(&tokens, &mut pos, &limits()).expect("valid");
        assert_eq!(appid, "VENDOR_XYZ");
    }

    #[test]
    fn stops_before_the_next_application() {
        let tokens = vec![
            t(gcode::XDATA_APPID, "A", 1),
            t(1000, "a", 2),
            t(gcode::XDATA_APPID, "B", 3),
            t(1000, "b", 4),
        ];
        let mut pos = 0;
        let (appid, records) = read_block(&tokens, &mut pos, &limits()).expect("valid");
        assert_eq!(appid, "A");
        assert_eq!(records.len(), 1);
        assert_eq!(pos, 2, "pos addresses the next 1001");
    }

    #[test]
    fn handles_nested_braces() {
        let tokens = vec![
            t(gcode::XDATA_APPID, "A", 1),
            t(gcode::XDATA_CONTROL, "{", 2),
            t(1000, "inner", 3),
            t(gcode::XDATA_CONTROL, "{", 4),
            t(1000, "deeper", 5),
            t(gcode::XDATA_CONTROL, "}", 6),
            t(gcode::XDATA_CONTROL, "}", 7),
            t(1000, "after", 8),
        ];
        let mut pos = 0;
        let (appid, records) = read_block(&tokens, &mut pos, &limits()).expect("valid");
        assert_eq!(appid, "A");
        // The braces are structure, not data, so only the three items remain.
        let values: Vec<&str> = records.iter().map(|r| r.value.as_str()).collect();
        assert_eq!(values, vec!["inner", "deeper", "after"]);
        assert_eq!(pos, 8);
    }

    #[test]
    fn a_closed_list_does_not_end_the_block() {
        let tokens = vec![
            t(gcode::XDATA_APPID, "A", 1),
            t(gcode::XDATA_CONTROL, "{", 2),
            t(1000, "in-list", 3),
            t(gcode::XDATA_CONTROL, "}", 4),
            t(1000, "after-list", 5),
        ];
        let mut pos = 0;
        let (_, records) = read_block(&tokens, &mut pos, &limits()).expect("valid");
        assert_eq!(records.len(), 2);
        assert_eq!(pos, 5);
    }

    #[test]
    fn stops_at_the_next_entity() {
        let tokens = vec![
            t(gcode::XDATA_APPID, "A", 1),
            t(1000, "a", 2),
            t(0, "LINE", 3),
        ];
        let mut pos = 0;
        let (_, records) = read_block(&tokens, &mut pos, &limits()).expect("valid");
        assert_eq!(records.len(), 1);
        assert_eq!(pos, 2, "stops before the next entity marker");
    }

    #[test]
    fn reports_unterminated_nesting() {
        let tokens = vec![
            t(gcode::XDATA_APPID, "A", 1),
            t(gcode::XDATA_CONTROL, "{", 2),
            t(1000, "a", 3),
        ];
        let mut pos = 0;
        let err = read_block(&tokens, &mut pos, &limits()).expect_err("must fail");
        assert!(matches!(err, DxfError::UnterminatedXData { .. }), "{err}");
    }

    #[test]
    fn reports_stray_closing_brace_and_stays_aligned() {
        let tokens = vec![
            t(gcode::XDATA_APPID, "A", 1),
            t(gcode::XDATA_CONTROL, "}", 2),
            t(0, "ENDSEC", 3),
        ];
        let mut pos = 0;
        assert!(read_block(&tokens, &mut pos, &limits()).is_err());
        assert_eq!(
            pos, 2,
            "the stray brace is consumed so parsing can continue"
        );
    }

    #[test]
    fn rejects_a_non_appid_token() {
        let tokens = vec![t(1000, "not-an-appid", 1)];
        let mut pos = 0;
        assert!(read_block(&tokens, &mut pos, &limits()).is_err());
    }

    #[test]
    fn enforces_the_xdata_size_limit() {
        let limits = ParseLimits {
            max_xdata_size: 4,
            ..ParseLimits::default()
        };
        let tokens = vec![t(gcode::XDATA_APPID, "A", 1), t(1000, "aaaaaaaaaa", 2)];
        let mut pos = 0;
        assert!(read_block(&tokens, &mut pos, &limits).is_err());
    }

    #[test]
    fn merge_appends_repeated_applications() {
        let mut target = EntityXData::new();
        merge(
            &mut target,
            "A".to_string(),
            vec![XDataRecord {
                code: 1000,
                value: "one".to_string(),
                line: 1,
            }],
        );
        merge(
            &mut target,
            "A".to_string(),
            vec![XDataRecord {
                code: 1000,
                value: "two".to_string(),
                line: 2,
            }],
        );
        assert_eq!(target.get("A").map(Vec::len), Some(2));
    }
}
