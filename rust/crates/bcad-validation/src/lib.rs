//! BCAD Validation
//!
//! Pure Rust geometric and topological validation.
//! No Qt, no C++, no UI dependencies.

use bcad_dxf::model::ParsedDxf;
use bcad_format::{GeometryRef, ParsedEntity, ParsedEntityType};
use serde::{Deserialize, Serialize};

/// Validation severity
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum ValidationSeverity {
    Info,
    Warning,
    Error,
}

/// Validation issue
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ValidationIssue {
    pub severity: ValidationSeverity,
    /// Owned rather than `&'static str`: a report has to round-trip through
    /// serde, and a borrowed literal has no lifetime to lend to a deserializer.
    pub code: String,
    pub message: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub entity_id: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub geometry: Option<GeometryRef>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub suggestion: Option<String>,
}

/// Validation report
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ValidationReport {
    pub errors: Vec<ValidationIssue>,
    pub warnings: Vec<ValidationIssue>,
    pub infos: Vec<ValidationIssue>,
}

impl ValidationReport {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_error(&mut self, code: &str, message: impl Into<String>) {
        self.errors.push(ValidationIssue {
            severity: ValidationSeverity::Error,
            code: code.to_string(),
            message: message.into(),
            entity_id: None,
            geometry: None,
            suggestion: None,
        });
    }

    pub fn add_warning(&mut self, code: &str, message: impl Into<String>) {
        self.warnings.push(ValidationIssue {
            severity: ValidationSeverity::Warning,
            code: code.to_string(),
            message: message.into(),
            entity_id: None,
            geometry: None,
            suggestion: None,
        });
    }

    pub fn add_info(&mut self, code: &str, message: impl Into<String>) {
        self.infos.push(ValidationIssue {
            severity: ValidationSeverity::Info,
            code: code.to_string(),
            message: message.into(),
            entity_id: None,
            geometry: None,
            suggestion: None,
        });
    }

    #[must_use]
    pub const fn has_errors(&self) -> bool {
        !self.errors.is_empty()
    }

    #[must_use]
    pub const fn total_issues(&self) -> usize {
        self.errors.len() + self.warnings.len() + self.infos.len()
    }
}

/// Validation options.
///
/// Five booleans is past the point where a struct stops reading well, and a
/// bitmask is the textbook fix. It is also the wrong one here: call sites read
/// `ValidationOptions { check_degenerate: false, ..default() }`, and swapping
/// that for `1 << CHECK_DEGENERATE` trades a compiler-checked field name for a
/// positional constant. The named fields stay.
#[allow(clippy::struct_excessive_bools)]
#[derive(Debug, Clone)]
pub struct ValidationOptions {
    /// **Not honoured yet.** No self-intersection test runs, whatever this is
    /// set to. Kept so callers can already express the intent, and so the
    /// eventual implementation is not an API break. See `validate_polyline`.
    pub check_self_intersection: bool,
    /// Reject lines shorter than `tolerance`, and circles or arcs whose radius
    /// is at most `tolerance`.
    pub check_degenerate: bool,
    /// Warn on consecutive vertices closer than `tolerance`, and on a closed
    /// polyline whose last vertex repeats its first.
    pub check_duplicate_points: bool,
    /// Warn when the axis-aligned bounding boxes of two closed polylines
    /// intersect. This is a coarse screen, not a topology test: it will report
    /// containment, and it will report boxes that merely touch.
    pub check_overlap: bool,
    /// Length below which geometry counts as degenerate. Squared distances are
    /// compared against `tolerance * tolerance`.
    pub tolerance: f64,
    /// **Not honoured yet.** Validation is single-threaded regardless. The
    /// `rayon` dependency that would have served it has been removed rather than
    /// left in the graph unused.
    pub parallel: bool,
}

impl Default for ValidationOptions {
    fn default() -> Self {
        Self {
            check_self_intersection: true,
            check_degenerate: true,
            check_duplicate_points: true,
            check_overlap: true,
            tolerance: 1e-9,
            parallel: true,
        }
    }
}

/// Validate a parsed DXF document.
///
/// `opts` is taken by value: the struct is five booleans and an `f64`, and by
/// reference it would force a caller validating several documents in a loop to
/// clone it each time. `bcad-doctor` already hands over an owned value here and
/// is a frozen crate, so the signature stays as it is.
#[must_use]
#[allow(clippy::needless_pass_by_value)]
pub fn validate_dxf(dxf: &ParsedDxf, opts: ValidationOptions) -> ValidationReport {
    let mut report = ValidationReport::new();

    // Validate entities
    for entity in &dxf.entities {
        validate_entity(entity, &opts, &mut report);
    }

    // Cross-entity validation (overlap, etc.)
    if opts.check_overlap {
        validate_overlap(&dxf.entities, &mut report);
    }

    report
}

/// Validate a single entity
fn validate_entity(entity: &ParsedEntity, opts: &ValidationOptions, report: &mut ValidationReport) {
    match &entity.entity_type {
        ParsedEntityType::Line { start, end } => {
            // Full 3D length. Measuring dx/dy only would call a segment from
            // (0,0,0) to (0,0,5) degenerate, because its horizontal projection
            // collapses to a point.
            let d = [end[0] - start[0], end[1] - start[1], end[2] - start[2]];
            let len_sq = d[2].mul_add(d[2], d[1].mul_add(d[1], d[0] * d[0]));
            if opts.check_degenerate && len_sq < opts.tolerance * opts.tolerance {
                report.add_error(
                    "VAL-GEOM-001",
                    format!("Degenerate line (length < {:.2e})", opts.tolerance),
                );
            }
        }
        ParsedEntityType::Polyline {
            vertices, closed, ..
        } => {
            validate_polyline(vertices, *closed, opts, report);
        }
        ParsedEntityType::Circle { radius, .. } => {
            if opts.check_degenerate && *radius <= opts.tolerance {
                report.add_error(
                    "VAL-GEOM-002",
                    format!("Degenerate circle (radius <= {:.2e})", opts.tolerance),
                );
            }
        }
        ParsedEntityType::Arc { radius, .. }
            if opts.check_degenerate && *radius <= opts.tolerance =>
        {
            report.add_error(
                "VAL-GEOM-003",
                format!("Degenerate arc (radius <= {:.2e})", opts.tolerance),
            );
        }
        _ => {}
    }
}

fn validate_polyline(
    vertices: &[[f64; 3]],
    closed: bool,
    opts: &ValidationOptions,
    report: &mut ValidationReport,
) {
    if vertices.len() < 2 {
        report.add_error("VAL-GEOM-004", "Polyline has fewer than 2 vertices");
        return;
    }

    if opts.check_duplicate_points {
        for i in 1..vertices.len() {
            let dx = vertices[i][0] - vertices[i - 1][0];
            let dy = vertices[i][1] - vertices[i - 1][1];
            let dz = vertices[i][2] - vertices[i - 1][2];
            let dist_sq = dz.mul_add(dz, dy.mul_add(dy, dx * dx));
            if dist_sq < opts.tolerance * opts.tolerance {
                report.add_warning(
                    "VAL-GEOM-005",
                    format!("Duplicate consecutive points at index {i}"),
                );
            }
        }
        if closed && vertices.len() >= 2 {
            let dx = vertices[0][0] - vertices[vertices.len() - 1][0];
            let dy = vertices[0][1] - vertices[vertices.len() - 1][1];
            let dz = vertices[0][2] - vertices[vertices.len() - 1][2];
            let dist_sq = dz.mul_add(dz, dy.mul_add(dy, dx * dx));
            if dist_sq < opts.tolerance * opts.tolerance {
                report.add_warning(
                    "VAL-GEOM-006",
                    "Closed polyline has duplicate start/end points",
                );
            }
        }
    }

    // `check_self_intersection` is deliberately not consulted here. The
    // predicates that would implement it live in `geo` but are unreachable, and
    // wiring them up needs a decision this crate has not made: whether a
    // non-planar polyline is validated by projection onto a plane, or reported
    // as unsupported. Until that is settled the option is a no-op, and it says
    // so in its own documentation rather than silently returning a clean
    // report for a check that never ran.
}

fn validate_overlap(entities: &[ParsedEntity], report: &mut ValidationReport) {
    // Simplified: check bounding box overlap for polylines
    let mut polygons: Vec<Vec<[f64; 3]>> = Vec::new();

    for entity in entities {
        if let ParsedEntityType::Polyline {
            vertices, closed, ..
        } = &entity.entity_type
        {
            if *closed && vertices.len() >= 3 {
                polygons.push(vertices.clone());
            }
        }
    }

    // Check bounding box overlaps
    for i in 0..polygons.len() {
        for j in i + 1..polygons.len() {
            let bb1 = bounding_box(&polygons[i]);
            let bb2 = bounding_box(&polygons[j]);

            if boxes_overlap(bb1, bb2) {
                report.add_warning("VAL-TOPO-001", "Bounding boxes overlap between entities");
            }
        }
    }
}

/// Axis-aligned bounding box as `(min_x, min_y, min_z, max_x, max_y, max_z)`.
///
/// Three-dimensional on purpose. Dropping `z` would make two closed polylines
/// stacked at different elevations report as overlapping, which is the common
/// case for a floor plan drawn on several levels.
fn bounding_box(points: &[[f64; 3]]) -> (f64, f64, f64, f64, f64, f64) {
    let mut min = [f64::INFINITY; 3];
    let mut max = [f64::NEG_INFINITY; 3];

    for p in points {
        for axis in 0..3 {
            min[axis] = min[axis].min(p[axis]);
            max[axis] = max[axis].max(p[axis]);
        }
    }

    (min[0], min[1], min[2], max[0], max[1], max[2])
}

fn boxes_overlap(bb1: (f64, f64, f64, f64, f64, f64), bb2: (f64, f64, f64, f64, f64, f64)) -> bool {
    !(bb1.3 < bb2.0
        || bb2.3 < bb1.0
        || bb1.4 < bb2.1
        || bb2.4 < bb1.1
        || bb1.5 < bb2.2
        || bb2.5 < bb1.2)
}

/// Validate a bare list of entities, with no document around them.
#[must_use]
#[allow(clippy::needless_pass_by_value)]
pub fn validate_geometry(entities: &[ParsedEntity], opts: ValidationOptions) -> ValidationReport {
    let mut report = ValidationReport::new();
    for entity in entities {
        validate_entity(entity, &opts, &mut report);
    }
    if opts.check_overlap {
        validate_overlap(entities, &mut report);
    }
    report
}

/// Geometric predicates.
///
/// Not reachable from `validate_*` yet, see the note in `validate_polyline`.
/// Kept because they are the tested primitives a self-intersection pass will
/// build on, and deleting them would discard working, covered code.
#[allow(dead_code)]
mod geo {
    pub fn segment_intersect(p1: [f64; 2], p2: [f64; 2], p3: [f64; 2], p4: [f64; 2]) -> bool {
        // Line segment intersection test (simplified)
        let d1 = direction(p3, p4, p1);
        let d2 = direction(p3, p4, p2);
        let d3 = direction(p1, p2, p3);
        let d4 = direction(p1, p2, p4);

        if ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0))
            && ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0))
        {
            return true;
        }

        if d1 == 0.0 && on_segment(p3, p4, p1) {
            return true;
        }
        if d2 == 0.0 && on_segment(p3, p4, p2) {
            return true;
        }
        if d3 == 0.0 && on_segment(p1, p2, p3) {
            return true;
        }
        if d4 == 0.0 && on_segment(p1, p2, p4) {
            return true;
        }

        false
    }

    fn direction(pi: [f64; 2], pj: [f64; 2], pk: [f64; 2]) -> f64 {
        (pj[0] - pi[0]).mul_add(-(pk[1] - pi[1]), (pk[0] - pi[0]) * (pj[1] - pi[1]))
    }

    fn on_segment(pi: [f64; 2], pj: [f64; 2], pk: [f64; 2]) -> bool {
        pk[0] >= pi[0].min(pj[0])
            && pk[0] <= pi[0].max(pj[0])
            && pk[1] >= pi[1].min(pj[1])
            && pk[1] <= pi[1].max(pj[1])
    }

    pub fn polygon_area(points: &[[f64; 2]]) -> f64 {
        let mut area = 0.0;
        let n = points.len();
        for i in 0..n {
            let j = (i + 1) % n;
            area += points[j][0].mul_add(-points[i][1], points[i][0] * points[j][1]);
        }
        area.abs() / 2.0
    }

    pub fn is_clockwise(points: &[[f64; 2]]) -> bool {
        let mut sum = 0.0;
        let n = points.len();
        for i in 0..n {
            let j = (i + 1) % n;
            sum = (points[j][0] - points[i][0]).mul_add(points[j][1] + points[i][1], sum);
        }
        sum > 0.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bcad_format::PropertyMap;
    use std::collections::BTreeMap;

    #[test]
    fn test_validation_report() {
        let mut report = ValidationReport::new();
        report.add_error("TEST-001", "Test error");
        report.add_warning("TEST-002", "Test warning");
        assert!(report.has_errors());
        assert_eq!(report.total_issues(), 2);
    }

    #[test]
    fn test_validate_line() {
        let entity = ParsedEntity {
            handle: Some("1".to_string()),
            layer: "0".to_string(),
            entity_type: ParsedEntityType::Line {
                start: [0.0, 0.0, 0.0],
                end: [0.0, 0.0, 0.0],
            },
            properties: PropertyMap::new(),
            xdata: BTreeMap::new(),
        };

        let opts = ValidationOptions::default();
        let mut report = ValidationReport::new();
        validate_entity(&entity, &opts, &mut report);

        assert!(report.has_errors());
        assert!(report.errors.iter().any(|e| e.code == "VAL-GEOM-001"));
    }

    #[test]
    fn test_bounding_box() {
        let pts = vec![
            [0.0, 0.0, 0.0],
            [10.0, 0.0, 4.0],
            [10.0, 10.0, 4.0],
            [0.0, 10.0, 0.0],
        ];
        let (min_x, min_y, min_z, max_x, max_y, max_z) = bounding_box(&pts);
        assert_eq!(
            (min_x, min_y, min_z, max_x, max_y, max_z),
            (0.0, 0.0, 0.0, 10.0, 10.0, 4.0)
        );
    }

    #[test]
    fn test_boxes_overlap() {
        let bb1 = (0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
        let bb2 = (5.0, 5.0, 5.0, 15.0, 15.0, 15.0);
        assert!(boxes_overlap(bb1, bb2));

        let bb3 = (20.0, 20.0, 20.0, 30.0, 30.0, 30.0);
        assert!(!boxes_overlap(bb1, bb3));

        // Same footprint, different elevation: must not be reported as an
        // overlap. This is the case a 2D bounding box got wrong.
        let above = (0.0, 0.0, 50.0, 10.0, 10.0, 60.0);
        assert!(!boxes_overlap(bb1, above));
    }

    /// A line that runs purely in z is not degenerate even though its horizontal
    /// projection is a single point.
    #[test]
    fn test_vertical_line_is_not_degenerate() {
        let entity = ParsedEntity {
            handle: None,
            layer: "0".to_string(),
            entity_type: ParsedEntityType::Line {
                start: [0.0, 0.0, 0.0],
                end: [0.0, 0.0, 5.0],
            },
            properties: PropertyMap::new(),
            xdata: BTreeMap::new(),
        };

        let mut report = ValidationReport::new();
        validate_entity(&entity, &ValidationOptions::default(), &mut report);
        assert!(
            !report.errors.iter().any(|e| e.code == "VAL-GEOM-001"),
            "a 5-unit vertical segment was reported degenerate: {:?}",
            report.errors
        );
    }

    #[test]
    fn test_report_round_trips_through_json() {
        // `code` is owned precisely so this holds; with `&'static str` the
        // Deserialize impl did not compile.
        let mut report = ValidationReport::new();
        report.add_error("VAL-GEOM-001", "boom");
        let json = serde_json::to_string(&report).expect("serialise");
        let back: ValidationReport = serde_json::from_str(&json).expect("deserialise");
        assert_eq!(back.errors.len(), 1);
        assert_eq!(back.errors[0].code, "VAL-GEOM-001");
    }

    #[test]
    fn test_polygon_area() {
        let square = vec![[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0]];
        let area = geo::polygon_area(&square);
        assert!((area - 100.0).abs() < 1e-9);
    }

    #[test]
    fn test_is_clockwise() {
        let cw = vec![[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0]];
        assert!(!geo::is_clockwise(&cw)); // CCW

        let ccw = vec![[0.0, 0.0], [0.0, 10.0], [10.0, 10.0], [10.0, 0.0]];
        assert!(geo::is_clockwise(&ccw));
    }
}
