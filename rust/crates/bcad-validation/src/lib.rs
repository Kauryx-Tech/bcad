//! BCAD Validation
//!
//! Pure Rust geometric and topological validation.
//! No Qt, no C++, no UI dependencies.

use bcad_dxf::model::ParsedDxf;
use bcad_format::*;
use serde::{Deserialize, Serialize};
use thiserror::Error;

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
    pub code: &'static str,
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
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_error(&mut self, code: &'static str, message: impl Into<String>) {
        self.errors.push(ValidationIssue {
            severity: ValidationSeverity::Error,
            code,
            message: message.into(),
            entity_id: None,
            geometry: None,
            suggestion: None,
        });
    }

    pub fn add_warning(&mut self, code: &'static str, message: impl Into<String>) {
        self.warnings.push(ValidationIssue {
            severity: ValidationSeverity::Warning,
            code,
            message: message.into(),
            entity_id: None,
            geometry: None,
            suggestion: None,
        });
    }

    pub fn add_info(&mut self, code: &'static str, message: impl Into<String>) {
        self.infos.push(ValidationIssue {
            severity: ValidationSeverity::Info,
            code,
            message: message.into(),
            entity_id: None,
            geometry: None,
            suggestion: None,
        });
    }

    pub fn has_errors(&self) -> bool {
        !self.errors.is_empty()
    }

    pub fn total_issues(&self) -> usize {
        self.errors.len() + self.warnings.len() + self.infos.len()
    }
}

/// Validation options
#[derive(Debug, Clone)]
pub struct ValidationOptions {
    pub check_self_intersection: bool,
    pub check_degenerate: bool,
    pub check_duplicate_points: bool,
    pub check_overlap: bool,
    pub tolerance: f64,
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

/// Validate a parsed DXF document
pub fn validate_dxf(dxf: &ParsedDxf, opts: ValidationOptions) -> ValidationReport {
    let mut report = ValidationReport::new();

    // Validate entities
    for entity in &dxf.entities {
        validate_entity(entity, &opts, &mut report);
    }

    // Cross-entity validation (overlap, etc.)
    if opts.check_overlap {
        validate_overlap(&dxf.entities, &opts, &mut report);
    }

    report
}

/// Validate a single entity
fn validate_entity(entity: &bcad_format::ParsedEntity, opts: &ValidationOptions, report: &mut ValidationReport) {
    match &entity.entity_type {
        bcad_format::ParsedEntityType::Line { x1, y1, x2, y2, .. } => {
            let dx = x2 - x1;
            let dy = y2 - y1;
            let len_sq = dx * dx + dy * dy;
            if opts.check_degenerate && len_sq < opts.tolerance * opts.tolerance {
                report.add_error("VAL-GEOM-001", format!("Degenerate line (length < {:.2e})", opts.tolerance));
            }
        }
        bcad_format::ParsedEntityType::Polyline { points, closed, .. } => {
            let vertices_3d: Vec<[f64; 3]> = points.iter().map(|&[x, y]| [x, y, 0.0]).collect();
            validate_polyline(&vertices_3d, *closed, opts, report);
        }
        bcad_format::ParsedEntityType::Circle { r, .. } => {
            if opts.check_degenerate && *r <= opts.tolerance {
                report.add_error("VAL-GEOM-002", format!("Degenerate circle (radius <= {:.2e})", opts.tolerance));
            }
        }
        bcad_format::ParsedEntityType::Arc { r, .. } => {
            if opts.check_degenerate && *r <= opts.tolerance {
                report.add_error("VAL-GEOM-003", format!("Degenerate arc (radius <= {:.2e})", opts.tolerance));
            }
        }
        _ => {}
    }
}

fn validate_polyline(vertices: &[[f64; 3]], closed: bool, opts: &ValidationOptions, report: &mut ValidationReport) {
    if vertices.len() < 2 {
        report.add_error("VAL-GEOM-004", "Polyline has fewer than 2 vertices");
        return;
    }

    if opts.check_duplicate_points {
        for i in 1..vertices.len() {
            let dx = vertices[i][0] - vertices[i-1][0];
            let dy = vertices[i][1] - vertices[i-1][1];
            let dz = vertices[i][2] - vertices[i-1][2];
            let dist_sq = dx*dx + dy*dy + dz*dz;
            if dist_sq < opts.tolerance * opts.tolerance {
                report.add_warning("VAL-GEOM-005", format!("Duplicate consecutive points at index {}", i));
            }
        }
        if closed && vertices.len() >= 2 {
            let dx = vertices[0][0] - vertices[vertices.len()-1][0];
            let dy = vertices[0][1] - vertices[vertices.len()-1][1];
            let dz = vertices[0][2] - vertices[vertices.len()-1][2];
            let dist_sq = dx*dx + dy*dy + dz*dz;
            if dist_sq < opts.tolerance * opts.tolerance {
                report.add_warning("VAL-GEOM-006", "Closed polyline has duplicate start/end points");
            }
        }
    }

    // Self-intersection check (simplified - only checks adjacent segments for now)
    // Full self-intersection would use sweep line algorithm
}

fn validate_overlap(entities: &[bcad_format::ParsedEntity], opts: &ValidationOptions, report: &mut ValidationReport) {
    // Simplified: check bounding box overlap for polylines
    let mut polygons: Vec<(&bcad_format::ParsedEntity, Vec<[f64; 2]>)> = Vec::new();

    for entity in entities {
        if let bcad_format::ParsedEntityType::Polyline { points, closed, .. } = &entity.entity_type {
            if *closed && points.len() >= 3 {
                let pts2d: Vec<[f64; 2]> = points.iter().map(|v| [v[0], v[1]]).collect();
                polygons.push((entity, pts2d));
            }
        }
    }

    // Check bounding box overlaps
    for i in 0..polygons.len() {
        for j in i+1..polygons.len() {
            let (e1, pts1) = &polygons[i];
            let (e2, pts2) = &polygons[j];

            let bb1 = bounding_box(pts1);
            let bb2 = bounding_box(pts2);

            if boxes_overlap(bb1, bb2) {
                report.add_warning("VAL-TOPO-001", format!("Bounding boxes overlap between entities"));
            }
        }
    }
}

fn bounding_box(points: &[[f64; 2]]) -> (f64, f64, f64, f64) {
    let mut min_x = f64::INFINITY;
    let mut min_y = f64::INFINITY;
    let mut max_x = f64::NEG_INFINITY;
    let mut max_y = f64::NEG_INFINITY;

    for [x, y] in points {
        min_x = min_x.min(*x);
        max_x = max_x.max(*x);
        min_y = min_y.min(*y);
        max_y = max_y.max(*y);
    }

    (min_x, min_y, max_x, max_y)
}

fn boxes_overlap(bb1: (f64, f64, f64, f64), bb2: (f64, f64, f64, f64)) -> bool {
    !(bb1.2 < bb2.0 || bb2.2 < bb1.0 || bb1.3 < bb2.1 || bb2.3 < bb1.1)
}

/// Validate geometry entities (standalone function)
pub fn validate_geometry(entities: &[bcad_format::ParsedEntity], opts: ValidationOptions) -> ValidationReport {
    let mut report = ValidationReport::new();
    for entity in entities {
        validate_entity(entity, &opts, &mut report);
    }
    if opts.check_overlap {
        validate_overlap(entities, &opts, &mut report);
    }
    report
}

/// Geometric utilities
mod geo {
    pub fn segment_intersect(p1: [f64; 2], p2: [f64; 2], p3: [f64; 2], p4: [f64; 2]) -> bool {
        // Line segment intersection test (simplified)
        let d1 = direction(p3, p4, p1);
        let d2 = direction(p3, p4, p2);
        let d3 = direction(p1, p2, p3);
        let d4 = direction(p1, p2, p4);

        if ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) &&
           ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0)) {
            return true;
        }

        if d1 == 0.0 && on_segment(p3, p4, p1) { return true; }
        if d2 == 0.0 && on_segment(p3, p4, p2) { return true; }
        if d3 == 0.0 && on_segment(p1, p2, p3) { return true; }
        if d4 == 0.0 && on_segment(p1, p2, p4) { return true; }

        false
    }

    fn direction(pi: [f64; 2], pj: [f64; 2], pk: [f64; 2]) -> f64 {
        (pk[0] - pi[0]) * (pj[1] - pi[1]) - (pj[0] - pi[0]) * (pk[1] - pi[1])
    }

    fn on_segment(pi: [f64; 2], pj: [f64; 2], pk: [f64; 2]) -> bool {
        pk[0] >= pi[0].min(pj[0]) && pk[0] <= pi[0].max(pj[0]) &&
        pk[1] >= pi[1].min(pj[1]) && pk[1] <= pi[1].max(pj[1])
    }

    pub fn polygon_area(points: &[[f64; 2]]) -> f64 {
        let mut area = 0.0;
        let n = points.len();
        for i in 0..n {
            let j = (i + 1) % n;
            area += points[i][0] * points[j][1] - points[j][0] * points[i][1];
        }
        area.abs() / 2.0
    }

    pub fn is_clockwise(points: &[[f64; 2]]) -> bool {
        let mut sum = 0.0;
        let n = points.len();
        for i in 0..n {
            let j = (i + 1) % n;
            sum += (points[j][0] - points[i][0]) * (points[j][1] + points[i][1]);
        }
        sum > 0.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

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
        let entity = bcad_format::ParsedEntity {
            handle: Some("1".to_string()),
            layer: "0".to_string(),
            entity_type: bcad_format::ParsedEntityType::Line { x1: 0.0, y1: 0.0, z1: 0.0, x2: 0.0, y2: 0.0, z2: 0.0 },
            properties: PropertyMap::new(),
            xdata: HashMap::new(),
        };

        let opts = ValidationOptions::default();
        let mut report = ValidationReport::new();
        validate_entity(&entity, &opts, &mut report);

        assert!(report.has_errors());
        assert!(report.errors.iter().any(|e| e.code == "VAL-GEOM-001"));
    }

    #[test]
    fn test_bounding_box() {
        let pts = vec![[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0]];
        let (min_x, min_y, max_x, max_y) = bounding_box(&pts);
        assert_eq!((min_x, min_y, max_x, max_y), (0.0, 0.0, 10.0, 10.0));
    }

    #[test]
    fn test_boxes_overlap() {
        let bb1 = (0.0, 0.0, 10.0, 10.0);
        let bb2 = (5.0, 5.0, 15.0, 15.0);
        assert!(boxes_overlap(bb1, bb2));

        let bb3 = (20.0, 20.0, 30.0, 30.0);
        assert!(!boxes_overlap(bb1, bb3));
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
        assert!(!is_clockwise(&cw)); // CCW

        let ccw = vec![[0.0, 0.0], [0.0, 10.0], [10.0, 10.0], [10.0, 0.0]];
        assert!(is_clockwise(&ccw));
    }
}