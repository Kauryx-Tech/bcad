//! DXF group codes used by this reader.
//!
//! One canonical name per code. The previous revision of this crate defined
//! several aliases for the same code (`X_COORD`/`START_X`/`CENTER_X`/`VERTEX_X`
//! were all `10`), which made it impossible to tell which entity a group
//! belonged to. Codes are grouped by meaning; the *meaning within an entity* is
//! resolved by the entity parsers, not by redefining the code.

// ---------------------------------------------------------------------------
// Structural
// ---------------------------------------------------------------------------

pub use crate::token::CODE_MARKER as MARKER;

/// Group code 1: primary text value (layer/entity name is code 2, but TEXT and
/// ATTRIB carry their string here).
pub const TEXT_VALUE: i32 = 1;

/// Group code 2: name (section name, table name, layer name, block name).
pub const NAME: i32 = 2;

/// Group code 5: entity handle.
pub const HANDLE: i32 = 5;

/// Group code 6: line type name.
pub const LINE_TYPE_NAME: i32 = 6;

/// Group code 8: layer name an entity or table record belongs to.
pub const LAYER: i32 = 8;

/// Group code 9: header variable name; introduces a HEADER variable record.
pub const HEADER_VARIABLE: i32 = 9;

/// Group code 70: entity/table flags, and table record count.
pub const FLAGS: i32 = 70;

/// Group code 90: vertex count (polyline-like entities).
pub const VERTEX_COUNT: i32 = 90;

/// Group code 100: subclass marker. Carries no data; skipped by this reader.
pub const SUBCLASS_MARKER: i32 = 100;

// ---------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------

/// First 3D coordinate triple: (10, 20, 30).
pub const COORD_1: (i32, i32, i32) = (10, 20, 30);

/// Second 3D coordinate triple: (11, 21, 31). Used by LINE.
pub const COORD_2: (i32, i32, i32) = (11, 21, 31);

/// X of the first coordinate triple.
pub const X: i32 = 10;
/// Y of the first coordinate triple.
pub const Y: i32 = 20;
/// Z of the first coordinate triple. Absent in 2D entities, which implies 0.0.
pub const Z: i32 = 30;

/// X of the second coordinate triple (LINE endpoint).
pub const X2: i32 = 11;
/// Y of the second coordinate triple (LINE endpoint).
pub const Y2: i32 = 21;
/// Z of the second coordinate triple (LINE endpoint).
pub const Z2: i32 = 31;

/// Elevation applied to every vertex of a polyline.
pub const ELEVATION: i32 = 38;

/// Signed thickness along the extrusion normal.
pub const THICKNESS: i32 = 39;

// ---------------------------------------------------------------------------
// Entity parameters
// ---------------------------------------------------------------------------

/// Radius (CIRCLE, ARC); constant width / text height (TEXT, LWPOLYLINE).
///
/// The meaning is entity-relative and is resolved by the entity parsers.
pub const RADIUS_OR_HEIGHT: i32 = 40;

/// Start width of an LWPOLYLINE segment.
pub const START_WIDTH: i32 = 41;

/// End width of an LWPOLYLINE segment.
pub const END_WIDTH: i32 = 42;

/// Arc bulge of an LWPOLYLINE segment.
pub const BULGE: i32 = 42;

/// Start angle in degrees, measured counter-clockwise from the +X axis (ARC).
pub const START_ANGLE: i32 = 50;

/// End angle in degrees, measured counter-clockwise from the +X axis (ARC).
pub const END_ANGLE: i32 = 51;

// ---------------------------------------------------------------------------
// Extrusion direction (arbitrary entity placement)
// ---------------------------------------------------------------------------

/// X component of the extrusion direction.
pub const EXTRUSION_X: i32 = 210;
/// Y component of the extrusion direction.
pub const EXTRUSION_Y: i32 = 220;
/// Z component of the extrusion direction.
pub const EXTRUSION_Z: i32 = 230;

// ---------------------------------------------------------------------------
// Table records
// ---------------------------------------------------------------------------

/// `AutoCAD` colour index (layer colour). `0` = `ByBlock`, `256` = `ByLayer`.
pub const COLOR: i32 = 62;

/// Layer on/off flag (0 = off, 1 = on).
pub const LAYER_ON_OFF: i32 = 290;

/// Line weight in 1/100 mm (layer). -3 = default, -2 = by block, -1 = by layer.
pub const LINE_WEIGHT: i32 = 370;

// ---------------------------------------------------------------------------
// XDATA
// ---------------------------------------------------------------------------

/// First group of an XDATA block: the owning application identifier.
pub const XDATA_APPID: i32 = 1001;

/// XDATA control string: `{` opens and `}` closes a structured sub-record.
pub const XDATA_CONTROL: i32 = 1002;

/// First value code of an XDATA string item.
pub const XDATA_STRING: i32 = 1000;

// ---------------------------------------------------------------------------
// Markers
// ---------------------------------------------------------------------------

/// Opens a `SECTION`.
pub const SECTION: &str = "SECTION";
/// Closes a `SECTION`.
pub const ENDSEC: &str = "ENDSEC";
/// Opens a `TABLE` inside `TABLES`.
pub const TABLE: &str = "TABLE";
/// Closes a `TABLE`.
pub const ENDTAB: &str = "ENDTAB";
/// Terminates the file.
pub const EOF: &str = "EOF";
/// Header section.
pub const SECTION_HEADER: &str = "HEADER";
/// Tables section.
pub const SECTION_TABLES: &str = "TABLES";
/// Entities section.
pub const SECTION_ENTITIES: &str = "ENTITIES";

/// Layer table name.
pub const TABLE_LAYER: &str = "LAYER";
/// Registered-application table name.
pub const TABLE_APPID: &str = "APPID";

/// Layer table record marker.
pub const RECORD_LAYER: &str = "LAYER";
/// Registered-application table record marker.
pub const RECORD_APPID: &str = "APPID";

/// Conventional name of the layer every drawing is guaranteed to define.
pub const DEFAULT_LAYER: &str = "0";

/// Class-documentation marker, skipped by this reader.
pub const CLASS: &str = "CLASS";
/// Class-end marker, skipped by this reader.
pub const CLASS_END: &str = "ENDCLASS";

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn coordinate_codes_are_distinct() {
        assert_ne!(X, X2);
        assert_ne!(Y, Y2);
        assert_ne!(Z, Z2);
    }

    #[test]
    fn structural_codes_match_token_marker() {
        assert_eq!(MARKER, crate::token::CODE_MARKER);
    }

    #[test]
    fn flags_and_vertex_count_are_separate() {
        assert_ne!(FLAGS, VERTEX_COUNT);
        assert_ne!(FLAGS, LINE_WEIGHT);
        assert_ne!(FLAGS, LAYER_ON_OFF);
    }
}
