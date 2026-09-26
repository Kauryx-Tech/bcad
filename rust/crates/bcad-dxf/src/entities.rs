//! Interpretation of one entity's groups into canonical geometry.
//!
//! Each `parse_*` function takes the entity's own groups (its HEADER, handle,
//! layer and subclass-marker groups already removed) and returns the neutral
//! [`ParsedEntityType`]. They never fall back to a default: a group that is
//! required and absent is an error, a group that is present and malformed is an
//! error, and a value that is not finite is an error.
//!
//! Entities this reader does not interpret become
//! [`ParsedEntityType::Unknown`] with their groups preserved verbatim, so a
//! later pass can handle them without re-reading the file.

use bcad_format::{ParsedEntityType, RawGroup};

use crate::error::{DxfError, DxfResult};
use crate::group_code as gcode;
use crate::numeric::{coord3, optional_f64, optional_i32, require_f64};

/// Entity names this reader interprets into geometry.
///
/// Anything else is reported as `Unknown` with a diagnostic. `POLYLINE` and
/// `LWPOLYLINE` are both listed because both map onto the single neutral
/// [`ParsedEntityType::Polyline`] variant.
pub const INTERPRETED: &[&str] = &[
    "LINE",
    "POINT",
    "CIRCLE",
    "ARC",
    "TEXT",
    "LWPOLYLINE",
    "POLYLINE",
];

/// `true` when this reader turns `name` into a specific geometry variant.
#[must_use]
pub fn is_interpreted(name: &str) -> bool {
    INTERPRETED.contains(&name)
}

/// Interprets `groups` according to the entity type they came from.
///
/// `vertices` carries the collected `VERTEX` sub-entities of a classic
/// `POLYLINE`; it is ignored by every other entity type.
///
/// # Errors
///
/// Returns [`DxfError::InvalidNumeric`] or [`DxfError::NonFinite`] for a
/// malformed real, and [`DxfError::MissingGroup`] when a group the type requires
/// is absent. Callers decide whether to report or degrade to
/// [`ParsedEntityType::Unknown`].
pub fn interpret(
    name: &str,
    groups: &[RawGroup],
    vertices: &[Vec<RawGroup>],
) -> DxfResult<ParsedEntityType> {
    match name {
        "LINE" => line(groups),
        "POINT" => point(groups),
        "CIRCLE" => circle(groups),
        "ARC" => arc(groups),
        "TEXT" => text(groups),
        "LWPOLYLINE" => lwpolyline(groups),
        "POLYLINE" => polyline(groups, vertices),
        _ => Ok(ParsedEntityType::Unknown {
            type_name: name.to_string(),
            raw_groups: groups.to_vec(),
        }),
    }
}

/// LINE: two endpoints. Z is optional and defaults to 0.0.
fn line(groups: &[RawGroup]) -> DxfResult<ParsedEntityType> {
    let (start, _) = coord3(groups, gcode::COORD_1, "LINE")?;
    let (end, _) = coord3(groups, gcode::COORD_2, "LINE")?;
    Ok(ParsedEntityType::Line { start, end })
}

/// POINT: one position. Z is optional and defaults to 0.0.
fn point(groups: &[RawGroup]) -> DxfResult<ParsedEntityType> {
    let (position, _) = coord3(groups, gcode::COORD_1, "POINT")?;
    Ok(ParsedEntityType::Point { position })
}

/// CIRCLE: a centre and a strictly positive radius.
///
/// A zero or negative radius is rejected: it cannot be rendered, and passing it
/// on would produce a degenerate shape in the geometry kernel.
fn circle(groups: &[RawGroup]) -> DxfResult<ParsedEntityType> {
    let (center, _) = coord3(groups, gcode::COORD_1, "CIRCLE")?;
    let radius = require_f64(groups, gcode::RADIUS_OR_HEIGHT, "CIRCLE")?;

    if radius <= 0.0 {
        return Err(DxfError::invalid_numeric(
            radius.to_string(),
            gcode::RADIUS_OR_HEIGHT,
            groups.first().map_or(0, |g| g.line),
            "CIRCLE",
        ));
    }

    Ok(ParsedEntityType::Circle { center, radius })
}

/// ARC: a centre, a positive radius and a start/end angle in degrees.
fn arc(groups: &[RawGroup]) -> DxfResult<ParsedEntityType> {
    let (center, _) = coord3(groups, gcode::COORD_1, "ARC")?;
    let radius = require_f64(groups, gcode::RADIUS_OR_HEIGHT, "ARC")?;
    let start_angle_deg = require_f64(groups, gcode::START_ANGLE, "ARC")?;
    let end_angle_deg = require_f64(groups, gcode::END_ANGLE, "ARC")?;

    if radius <= 0.0 {
        return Err(DxfError::invalid_numeric(
            radius.to_string(),
            gcode::RADIUS_OR_HEIGHT,
            groups.first().map_or(0, |g| g.line),
            "ARC",
        ));
    }

    Ok(ParsedEntityType::Arc {
        center,
        radius,
        start_angle_deg,
        end_angle_deg,
    })
}

/// TEXT: a position, a string, a height and an optional rotation.
///
/// Height is required when present-but-zero is meaningless: DXF writes 2.5 by
/// default, but a writer that omits group 40 leaves the height unspecified, so
/// an absent height is an error rather than a guess.
fn text(groups: &[RawGroup]) -> DxfResult<ParsedEntityType> {
    let (position, _) = coord3(groups, gcode::COORD_1, "TEXT")?;
    let height = require_f64(groups, gcode::RADIUS_OR_HEIGHT, "TEXT")?;
    let rotation_deg = optional_f64(groups, gcode::START_ANGLE, "TEXT")?.unwrap_or(0.0);

    let value = groups
        .iter()
        .find(|g| g.code == gcode::TEXT_VALUE)
        .map_or_else(String::new, |g| g.value.clone());

    Ok(ParsedEntityType::Text {
        position,
        text: value,
        height,
        rotation_deg,
    })
}

/// LWPOLYLINE: inline vertices as repeating (10, 20) pairs.
///
/// The vertex elevation from group 38 is folded into each vertex's Z so the
/// result is genuinely 3D, matching what a classic POLYLINE produces.
fn lwpolyline(groups: &[RawGroup]) -> DxfResult<ParsedEntityType> {
    let elevation = optional_f64(groups, gcode::ELEVATION, "LWPOLYLINE")?.unwrap_or(0.0);
    let flags = optional_i32(groups, gcode::FLAGS, "LWPOLYLINE")?.unwrap_or(0);
    let closed = flags & 1 != 0;

    let mut vertices: Vec<[f64; 3]> = Vec::new();
    let mut current_x: Option<RawGroup> = None;

    for group in groups {
        match group.code {
            gcode::X => {
                // An X without a following Y is a malformed vertex, not a
                // vertex at y = 0.
                if let Some(x) = current_x.take() {
                    return Err(DxfError::missing_group(gcode::Y, "LWPOLYLINE", x.line));
                }
                current_x = Some(group.clone());
            }
            gcode::Y => {
                let x = current_x
                    .take()
                    .ok_or_else(|| DxfError::missing_group(gcode::X, "LWPOLYLINE", group.line))?;
                let x_value = crate::numeric::f64_field(&x, "LWPOLYLINE")?;
                let y_value = crate::numeric::f64_field(group, "LWPOLYLINE")?;
                vertices.push([x_value, y_value, elevation]);
            }
            _ => {}
        }
    }

    if let Some(dangling) = current_x {
        return Err(DxfError::missing_group(
            gcode::Y,
            "LWPOLYLINE",
            dangling.line,
        ));
    }

    Ok(ParsedEntityType::Polyline {
        vertices,
        closed,
        elevation,
    })
}

/// POLYLINE: a header record plus its `VERTEX` sub-entities.
///
/// The `VERTEX` records are folded into a single `Polyline`, and the trailing
/// `SEQEND` is consumed by the caller. This is the one representation both
/// polyline flavours produce.
fn polyline(groups: &[RawGroup], vertex_groups: &[Vec<RawGroup>]) -> DxfResult<ParsedEntityType> {
    let elevation = optional_f64(groups, gcode::ELEVATION, "POLYLINE")?.unwrap_or(0.0);
    let flags = optional_i32(groups, gcode::FLAGS, "POLYLINE")?.unwrap_or(0);
    let closed = flags & 1 != 0;

    let mut vertices: Vec<[f64; 3]> = Vec::with_capacity(vertex_groups.len());
    for record in vertex_groups {
        let (position, has_z) = coord3(record, gcode::COORD_1, "VERTEX")?;
        // A VERTEX without its own Z inherits the polyline elevation; an
        // explicit Z wins.
        let z = if has_z { position[2] } else { elevation };
        vertices.push([position[0], position[1], z]);
    }

    Ok(ParsedEntityType::Polyline {
        vertices,
        closed,
        elevation,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Tolerance for values that made a decimal text round trip.
    const EPS: f64 = 1e-9;

    /// `true` when two reals agree to within [`EPS`].
    fn close(a: f64, b: f64) -> bool {
        (a - b).abs() < EPS
    }

    /// `true` when two 3D points agree to within [`EPS`] per component.
    fn close3(a: [f64; 3], b: [f64; 3]) -> bool {
        a.iter().zip(b).all(|(x, y)| close(*x, y))
    }

    fn g(code: i32, value: &str) -> RawGroup {
        RawGroup {
            code,
            value: value.to_string(),
            line: 10,
        }
    }

    #[test]
    fn line_is_3d_and_defaults_z_to_zero() {
        let groups = vec![g(10, "1"), g(20, "2"), g(11, "3"), g(21, "4")];
        match interpret("LINE", &groups, &[]).expect("valid") {
            ParsedEntityType::Line { start, end } => {
                assert!(close3(start, [1.0, 2.0, 0.0]), "{start:?}");
                assert!(close3(end, [3.0, 4.0, 0.0]), "{end:?}");
            }
            other => panic!("expected Line, got {other:?}"),
        }
    }

    #[test]
    fn line_keeps_explicit_z() {
        let groups = vec![
            g(10, "1"),
            g(20, "2"),
            g(30, "5"),
            g(11, "3"),
            g(21, "4"),
            g(31, "6"),
        ];
        match interpret("LINE", &groups, &[]).expect("valid") {
            ParsedEntityType::Line { start, end } => {
                assert!(close(start[2], 5.0));
                assert!(close(end[2], 6.0));
            }
            other => panic!("expected Line, got {other:?}"),
        }
    }

    #[test]
    fn line_missing_coordinate_is_an_error() {
        let groups = vec![g(10, "1"), g(20, "2")];
        let err = interpret("LINE", &groups, &[]).expect_err("must fail");
        assert!(err.to_string().contains("LINE"), "{err}");
    }

    #[test]
    fn circle_rejects_non_positive_radius() {
        let groups = vec![g(10, "0"), g(20, "0"), g(40, "0")];
        assert!(interpret("CIRCLE", &groups, &[]).is_err());

        let groups = vec![g(10, "0"), g(20, "0"), g(40, "-3")];
        assert!(interpret("CIRCLE", &groups, &[]).is_err());
    }

    #[test]
    fn arc_reads_degrees() {
        let groups = vec![g(10, "0"), g(20, "0"), g(40, "5"), g(50, "0"), g(51, "90")];
        match interpret("ARC", &groups, &[]).expect("valid") {
            ParsedEntityType::Arc {
                radius,
                start_angle_deg,
                end_angle_deg,
                ..
            } => {
                assert!(close(radius, 5.0));
                assert!(close(start_angle_deg, 0.0));
                assert!(close(end_angle_deg, 90.0));
            }
            other => panic!("expected Arc, got {other:?}"),
        }
    }

    #[test]
    fn lwpolyline_pairs_coordinates_and_folds_elevation() {
        let groups = vec![
            g(38, "2.5"),
            g(70, "1"),
            g(10, "0"),
            g(20, "0"),
            g(10, "10"),
            g(20, "0"),
        ];
        match interpret("LWPOLYLINE", &groups, &[]).expect("valid") {
            ParsedEntityType::Polyline {
                vertices, closed, ..
            } => {
                assert!(closed);
                assert_eq!(vertices, vec![[0.0, 0.0, 2.5], [10.0, 0.0, 2.5]]);
            }
            other => panic!("expected Polyline, got {other:?}"),
        }
    }

    #[test]
    fn lwpolyline_rejects_dangling_x() {
        let groups = vec![g(10, "0")];
        let err = interpret("LWPOLYLINE", &groups, &[]).expect_err("must fail");
        assert!(err.to_string().contains("20"), "{err}");
    }

    #[test]
    fn lwpolyline_rejects_y_without_x() {
        let groups = vec![g(20, "0")];
        assert!(interpret("LWPOLYLINE", &groups, &[]).is_err());
    }

    #[test]
    fn classic_polyline_and_lwpolyline_share_one_variant() {
        let lw = vec![g(10, "1"), g(20, "2")];
        let lw_result = interpret("LWPOLYLINE", &lw, &[]).expect("valid");

        let header = vec![g(38, "9")];
        let vertex = vec![g(10, "1"), g(20, "2")];
        let poly_result = interpret("POLYLINE", &header, &[vertex]).expect("valid");

        assert!(matches!(lw_result, ParsedEntityType::Polyline { .. }));
        assert!(matches!(poly_result, ParsedEntityType::Polyline { .. }));
    }

    #[test]
    fn classic_polyline_vertex_z_overrides_elevation() {
        let header = vec![g(38, "9")];
        let explicit = vec![g(10, "1"), g(20, "2"), g(30, "3")];
        match interpret("POLYLINE", &header, &[explicit]).expect("valid") {
            ParsedEntityType::Polyline { vertices, .. } => {
                assert!(close3(vertices[0], [1.0, 2.0, 3.0]), "{:?}", vertices[0]);
            }
            other => panic!("expected Polyline, got {other:?}"),
        }
    }

    #[test]
    fn text_defaults_rotation_to_zero() {
        let groups = vec![g(10, "1"), g(20, "2"), g(40, "3.5"), g(1, "Hello")];
        match interpret("TEXT", &groups, &[]).expect("valid") {
            ParsedEntityType::Text {
                text,
                rotation_deg,
                height,
                ..
            } => {
                assert_eq!(text, "Hello");
                assert!(close(height, 3.5));
                assert!(close(rotation_deg, 0.0));
            }
            other => panic!("expected Text, got {other:?}"),
        }
    }

    #[test]
    fn uninterpreted_entity_keeps_its_groups() {
        let groups = vec![g(1, "payload")];
        match interpret("HATCH", &groups, &[]).expect("valid") {
            ParsedEntityType::Unknown {
                type_name,
                raw_groups,
            } => {
                assert_eq!(type_name, "HATCH");
                assert_eq!(raw_groups, groups);
            }
            other => panic!("expected Unknown, got {other:?}"),
        }
    }

    #[test]
    fn is_interpreted_matches_the_table() {
        assert!(is_interpreted("LINE"));
        assert!(is_interpreted("POLYLINE"));
        assert!(!is_interpreted("MTEXT"));
        assert!(!is_interpreted("INSERT"));
        assert!(!is_interpreted("HATCH"));
    }

    #[test]
    fn nan_radius_is_rejected() {
        let groups = vec![g(10, "0"), g(20, "0"), g(40, "NaN")];
        assert!(interpret("CIRCLE", &groups, &[]).is_err());
    }
}
