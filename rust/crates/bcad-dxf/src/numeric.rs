//! Contextual conversion of raw DXF group values to Rust numbers.
//!
//! Every failure carries the group code, the offending value, the source line
//! and the entity it belongs to. There is no `unwrap_or(0.0)` path: a value
//! that cannot be represented is either an error or, for genuinely optional
//! groups, an explicit `Ok(None)`.

use crate::error::DxfError;
use bcad_format::RawGroup;

/// Converts a real-valued group, rejecting `NaN` and infinities.
///
/// DXF group values for coordinates and angles are finite reals by definition.
/// `NaN` and `inf` parse successfully through `f64::from_str`, so the finite
/// check is load-bearing, not defensive noise: it is what stops a poison float
/// from reaching the geometry kernel.
///
/// # Errors
///
/// Returns [`DxfError::InvalidNumeric`] when the text is not a number, and
/// [`DxfError::NonFinite`] when it parses to `NaN` or an infinity. Both name the
/// group code, the source line and the entity.
pub fn f64_field(group: &RawGroup, entity_type: &str) -> Result<f64, DxfError> {
    let parsed: f64 = group.value.trim().parse().map_err(|_| {
        DxfError::invalid_numeric(&group.value, group.code, group.line, entity_type)
    })?;

    if !parsed.is_finite() {
        return Err(DxfError::non_finite(
            &group.value,
            group.code,
            group.line,
            entity_type,
        ));
    }

    Ok(parsed)
}

/// Converts an integer-valued group.
///
/// # Errors
///
/// Returns [`DxfError::InvalidNumeric`] when the text is not an `i32`, naming
/// the group code, source line and entity.
pub fn i32_field(group: &RawGroup, entity_type: &str) -> Result<i32, DxfError> {
    group
        .value
        .trim()
        .parse()
        .map_err(|_| DxfError::invalid_numeric(&group.value, group.code, group.line, entity_type))
}

/// Converts an integer-valued group that DXF writes as a real
/// (e.g. `70.0` instead of `70`).
///
/// Some writers emit a decimal point on integer groups. Accepting only that
/// spelling keeps the intent explicit while tolerating the common variant.
///
/// # Errors
///
/// Returns [`DxfError::InvalidNumeric`] when the text is neither an integer nor
/// a whole-valued real.
pub fn integerish_field(group: &RawGroup, entity_type: &str) -> Result<i32, DxfError> {
    let value = group.value.trim();
    if let Ok(v) = value.parse::<i32>() {
        return Ok(v);
    }
    if let Ok(v) = value.parse::<f64>() {
        if v.is_finite() && v % 1.0 == 0.0 && v >= f64::from(i32::MIN) && v <= f64::from(i32::MAX) {
            #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
            return Ok(v as i32);
        }
    }
    Err(DxfError::invalid_numeric(
        &group.value,
        group.code,
        group.line,
        entity_type,
    ))
}

/// Reads a real-valued group as a 3D coordinate triple.
///
/// DXF omits the Z groups for planar entities; a missing Z is a *specified*
/// zero, not a parse failure, and is reported as such through the boolean.
///
/// # Errors
///
/// Returns [`DxfError::MissingGroup`] when X or Y is absent, and propagates any
/// numeric failure from [`f64_field`].
pub fn coord3(
    groups: &[RawGroup],
    codes: (i32, i32, i32),
    entity_type: &str,
) -> Result<([f64; 3], bool), DxfError> {
    let (cx, cy, cz) = codes;
    let x = require_f64(groups, cx, entity_type)?;
    let y = require_f64(groups, cy, entity_type)?;
    let z_present = groups.iter().any(|g| g.code == cz);
    let z = if z_present {
        require_f64(groups, cz, entity_type)?
    } else {
        0.0
    };
    Ok(([x, y, z], z_present))
}

/// Reads a required real-valued group, reporting the entity type when absent.
///
/// # Errors
///
/// Returns [`DxfError::MissingGroup`] when the group is absent, and propagates
/// any numeric failure from [`f64_field`].
pub fn require_f64(groups: &[RawGroup], code: i32, entity_type: &str) -> Result<f64, DxfError> {
    groups.iter().find(|g| g.code == code).map_or_else(
        || Err(DxfError::missing_group(code, entity_type, line_of(groups))),
        |group| f64_field(group, entity_type),
    )
}

/// Reads an optional real-valued group.
///
/// A group that is present but malformed is still an error: the distinction
/// being made is "absent" versus "present and wrong", not "bad values are fine".
///
/// # Errors
///
/// Returns [`DxfError::InvalidNumeric`] or [`DxfError::NonFinite`] when the
/// group is present but unusable. `Ok(None)` means the group is genuinely absent.
pub fn optional_f64(
    groups: &[RawGroup],
    code: i32,
    entity_type: &str,
) -> Result<Option<f64>, DxfError> {
    groups
        .iter()
        .find(|g| g.code == code)
        .map_or(Ok(None), |group| f64_field(group, entity_type).map(Some))
}

/// Reads an optional integer-valued group.
///
/// # Errors
///
/// Returns [`DxfError::InvalidNumeric`] when the group is present but is not a
/// whole number. `Ok(None)` means the group is genuinely absent.
pub fn optional_i32(
    groups: &[RawGroup],
    code: i32,
    entity_type: &str,
) -> Result<Option<i32>, DxfError> {
    groups
        .iter()
        .find(|g| g.code == code)
        .map_or(Ok(None), |group| {
            integerish_field(group, entity_type).map(Some)
        })
}

/// Source line used when a required group is missing: the first group of the
/// entity, so the diagnostic still points at the right place.
fn line_of(groups: &[RawGroup]) -> usize {
    groups.first().map_or(0, |g| g.line)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Tolerance for values that made a decimal text round trip.
    const EPS: f64 = 1e-9;

    fn group(code: i32, value: &str) -> RawGroup {
        RawGroup {
            code,
            value: value.to_string(),
            line: 12,
        }
    }

    #[test]
    fn parses_plain_real() {
        let g = group(10, "1.5");
        assert!((f64_field(&g, "LINE").expect("valid") - 1.5).abs() < EPS);
    }

    #[test]
    fn rejects_non_numeric_with_context() {
        let g = group(10, "abc");
        let err = f64_field(&g, "LINE").expect_err("must fail");
        let text = err.to_string();
        assert!(text.contains("abc"), "{text}");
        assert!(text.contains("LINE"), "{text}");
        assert!(text.contains("line 12"), "{text}");
    }

    #[test]
    fn rejects_nan_and_infinities() {
        for bad in ["NaN", "nan", "inf", "-inf", "Infinity"] {
            let g = group(10, bad);
            let err = f64_field(&g, "LINE").expect_err("must fail");
            assert!(
                err.to_string().to_lowercase().contains("finite")
                    || err.to_string().contains("numeric"),
                "unexpected error for {bad}: {err}"
            );
        }
    }

    #[test]
    fn missing_required_group_reports_entity() {
        let groups = vec![group(8, "0")];
        let err = require_f64(&groups, 10, "CIRCLE").expect_err("must fail");
        assert!(err.to_string().contains("CIRCLE"), "{err}");
    }

    #[test]
    fn absent_z_is_zero_not_an_error() {
        let groups = vec![group(10, "1"), group(20, "2")];
        let (coord, z_present) = coord3(&groups, (10, 20, 30), "LINE").expect("valid");
        assert!((coord[0] - 1.0).abs() < EPS);
        assert!((coord[1] - 2.0).abs() < EPS);
        assert!(coord[2].abs() < EPS);
        assert!(!z_present);
    }

    #[test]
    fn present_but_malformed_optional_group_is_an_error() {
        let groups = vec![group(40, "oops")];
        assert!(optional_f64(&groups, 40, "CIRCLE").is_err());
    }

    #[test]
    fn integerish_accepts_writer_decimal_spelling() {
        let g = group(70, "1.0");
        assert_eq!(integerish_field(&g, "LWPOLYLINE").expect("valid"), 1);
        let g = group(70, "1.5");
        assert!(integerish_field(&g, "LWPOLYLINE").is_err());
    }
}
