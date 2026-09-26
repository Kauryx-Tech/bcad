//! BCAD Doctor - Diagnostic CLI tool
//!
//! Commands:
//!   inspect    - Show file structure and metadata
//!   check      - Quick integrity check
//!   validate   - Full validation with diagnostics
//!   migrate    - Migrate v1 to v2 schema
//!   report     - Generate JSON report

use bcad_db::{self, Database};
use bcad_dxf::{parse_dxf_file, ParseLimits, ParseOptions, RecoveryMode};
use bcad_format::Severity;
use bcad_validation::{validate_dxf, ValidationOptions};
use clap::{Parser, Subcommand, ValueEnum};
use serde_json::json;
use std::path::PathBuf;
use std::process;

#[derive(Parser)]
#[command(name = "bcad-doctor", version, about = "BCAD diagnostic CLI tool")]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Inspect file structure and metadata
    Inspect {
        /// Input file path
        #[arg(value_name = "FILE")]
        file: PathBuf,

        /// Output format
        #[arg(short, long, value_enum, default_value = "human")]
        format: OutputFormat,
    },

    /// Quick integrity check
    Check {
        /// Input file path
        #[arg(value_name = "FILE")]
        file: PathBuf,

        /// Exit with error code on issues
        #[arg(short, long)]
        fail_on_warning: bool,
    },

    /// Full validation with diagnostics
    Validate {
        /// Input file path
        #[arg(value_name = "FILE")]
        file: PathBuf,

        /// Output format
        #[arg(short, long, value_enum, default_value = "human")]
        format: OutputFormat,

        /// Strict mode - fail on warnings
        #[arg(long)]
        strict: bool,
    },

    /// Migrate v1 schema to v2
    Migrate {
        /// Input file path
        #[arg(value_name = "INPUT")]
        input: PathBuf,

        /// Output file path
        #[arg(value_name = "OUTPUT")]
        output: PathBuf,

        /// Dry run - don't write output
        #[arg(long)]
        dry_run: bool,
    },

    /// Generate JSON report
    Report {
        /// Input file path
        #[arg(value_name = "FILE")]
        file: PathBuf,

        /// Output JSON file (stdout if omitted)
        #[arg(short, long)]
        output: Option<PathBuf>,

        /// Pretty print JSON
        #[arg(long)]
        pretty: bool,
    },

    /// Parse and validate DXF file
    Dxf {
        /// Input DXF file path
        #[arg(value_name = "FILE")]
        file: PathBuf,

        /// Recovery mode
        #[arg(short, long, value_enum, default_value = "recover")]
        recovery: RecoveryModeArg,

        /// Output parsed structure as JSON
        #[arg(short, long)]
        output: Option<PathBuf>,
    },
}

#[derive(Copy, Clone, ValueEnum)]
enum OutputFormat {
    Human,
    Json,
}

/// Mirrors [`RecoveryMode`]. Deliberately has no third variant: the library
/// offers strict and recover, and a `repair` flag here would promise a pass
/// that repairs malformed input when nothing in `bcad-dxf` does that.
#[derive(Copy, Clone, ValueEnum)]
enum RecoveryModeArg {
    Strict,
    Recover,
}

impl From<RecoveryModeArg> for RecoveryMode {
    fn from(mode: RecoveryModeArg) -> Self {
        match mode {
            RecoveryModeArg::Strict => RecoveryMode::Strict,
            RecoveryModeArg::Recover => RecoveryMode::Recover,
        }
    }
}

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        Commands::Inspect { file, format } => inspect_cmd(&file, format),
        Commands::Check {
            file,
            fail_on_warning,
        } => check_cmd(&file, fail_on_warning),
        Commands::Validate {
            file,
            format,
            strict,
        } => validate_cmd(&file, format, strict),
        Commands::Migrate {
            input,
            output,
            dry_run,
        } => migrate_cmd(&input, &output, dry_run),
        Commands::Report {
            file,
            output,
            pretty,
        } => report_cmd(&file, output, pretty),
        Commands::Dxf {
            file,
            recovery,
            output,
        } => dxf_cmd(&file, recovery, output),
    };

    if let Err(e) = result {
        eprintln!("Error: {}", e);
        process::exit(1);
    }
}

fn inspect_cmd(file: &PathBuf, format: OutputFormat) -> anyhow::Result<()> {
    let db = bcad_db::open_readonly(file)?;
    let version = db.schema_version()?;
    let layers = db.layers()?;
    let entities = db.entities()?;

    let info = json!({
        "file": file.to_string_lossy(),
        "schema_version": version,
        "layer_count": layers.len(),
        "entity_count": entities.len(),
        "layers": layers.iter().map(|l| json!({
            "name": l.name,
            "color": {"r": l.color.r, "g": l.color.g, "b": l.color.b, "a": l.color.a},
            "visible": l.visible,
            "locked": l.locked,
        })).collect::<Vec<_>>(),
        "entities": entities.iter().take(10).map(|e| json!({
            "id": e.id,
            "type_id": e.type_id,
            "layer": e.layer,
            "handle": e.handle,
            "property_count": e.properties.len(),
        })).collect::<Vec<_>>(),
    });

    match format {
        OutputFormat::Human => {
            println!("File: {}", file.display());
            println!("Schema version: {}", version);
            println!("Layers: {}", layers.len());
            println!("Entities: {}", entities.len());
            println!("\nLayers:");
            for l in &layers {
                println!(
                    "  {} (color: {:.2},{:.2},{:.2}, visible: {}, locked: {})",
                    l.name, l.color.r, l.color.g, l.color.b, l.visible, l.locked
                );
            }
            println!("\nEntities (first 10):");
            for e in entities.iter().take(10) {
                println!(
                    "  #{} {} @ {} [{}] ({} props)",
                    e.id,
                    e.type_id,
                    e.layer,
                    e.handle.as_deref().unwrap_or("-"),
                    e.properties.len()
                );
            }
            if entities.len() > 10 {
                println!("  ... and {} more", entities.len() - 10);
            }
        }
        OutputFormat::Json => {
            println!("{}", serde_json::to_string_pretty(&info)?);
        }
    }
    Ok(())
}

fn check_cmd(file: &PathBuf, fail_on_warning: bool) -> anyhow::Result<()> {
    let db = bcad_db::open_readonly(file)?;

    // SQLite integrity check
    let integrity = db.integrity_check()?;
    if integrity != "ok" {
        eprintln!("❌ SQLite integrity check failed: {}", integrity);
        return Err(anyhow::anyhow!("Integrity check failed"));
    }

    // Schema version
    let version = db.schema_version()?;
    if !(1..=2).contains(&version) {
        eprintln!("❌ Unsupported schema version: {}", version);
        return Err(anyhow::anyhow!("Unsupported schema version"));
    }

    // Quick entity check
    let entities = db.entities()?;
    let layers = db.layers()?;

    println!("✅ SQLite integrity: OK");
    println!("✅ Schema version: {} (supported)", version);
    println!("✅ Layers: {}", layers.len());
    println!("✅ Entities: {}", entities.len());

    // Check for unknown type IDs
    let mut unknown_types = 0;
    for e in &entities {
        if !is_known_type(&e.type_id) {
            unknown_types += 1;
            println!("⚠ Unknown type ID: {}", e.type_id);
        }
    }

    if unknown_types > 0 {
        println!("⚠ Unknown type IDs: {}", unknown_types);
        if fail_on_warning {
            return Err(anyhow::anyhow!("Unknown type IDs found"));
        }
    }

    println!("✅ Check passed");
    Ok(())
}

fn is_known_type(type_id: &str) -> bool {
    matches!(
        type_id,
        "bcad.Point"
            | "bcad.Line"
            | "bcad.Circle"
            | "bcad.Arc"
            | "bcad.Polyline"
            | "bcad.Text"
            | "cadastre.parcel"
            | "cadastre.boundary"
            | "cadastre.survey_mark"
            | "cadastre.easement"
    )
}

/// How much of a file the geometry pass could actually see.
///
/// Both `validate` and `report` need this, and they must not be able to drift
/// apart: a tool printing "0 errors" after having loaded nothing is worse than
/// no tool at all.
#[derive(Debug, Clone, Copy)]
struct GeometryScope {
    total: usize,
    with_geometry: usize,
}

impl GeometryScope {
    fn of(doc: &bcad_dxf::ParsedDxf) -> Self {
        Self {
            total: doc.entities.len(),
            with_geometry: doc
                .entities
                .iter()
                .filter(|e| !matches!(e.entity_type, bcad_format::ParsedEntityType::Unknown { .. }))
                .count(),
        }
    }

    fn note(&self, report: &mut bcad_validation::ValidationReport) {
        report.add_info(
            "VAL-SCOPE-001",
            format!(
                "{} of {} entities carried usable geometry and were checked \
                 geometrically; the rest were not, either because they carry no \
                 `geometry` property or because it could not be read (see the \
                 \"while reading the file\" section)",
                self.with_geometry, self.total
            ),
        );
        if self.with_geometry == 0 && self.total > 0 {
            report.add_warning(
                "VAL-SCOPE-002",
                "no entity in this file had readable geometry, so no geometry \
                 was actually validated"
                    .to_string(),
            );
        }
    }
}

fn validate_cmd(file: &PathBuf, format: OutputFormat, strict: bool) -> anyhow::Result<()> {
    let db = bcad_db::open_readonly(file)?;
    let entities = db.entities()?;

    let opts = ValidationOptions {
        check_degenerate: true,
        check_duplicate_points: true,
        check_overlap: true,
        tolerance: 1e-9,
        // Both of these are no-ops today, see their docs in bcad-validation.
        // Left at false so this does not read as a request for a check that
        // never runs.
        check_self_intersection: false,
        parallel: false,
    };

    let document = parse_dxf_from_db(&db)?;
    let scope = GeometryScope::of(&document);
    let mut report = validate_dxf(&document, opts);
    scope.note(&mut report);

    // Add entity-level validation
    for entity in &entities {
        let bcad_db::EntityRecord { properties, .. } = entity;
        // A `match` rather than nested `if let`: the workspace is edition 2021
        // so let-chains are unavailable, and clippy asks for the collapse.
        match properties.get("cadastre.section") {
            Some(bcad_format::PropertyValue::String(s)) if s.is_empty() => {
                report.add_warning("VAL-CAD-001", "Empty cadastre.section");
            }
            _ => {}
        }
    }

    output_report(&report, format, strict, &document.diagnostics)
}

/// Rebuilds a document from the database so `validate` and `report` have
/// something to inspect.
///
/// Geometry comes from the `geometry` property, holding a serialised GeoJSON
/// geometry, which is the same convention `bcad-export` reads. Nothing in the
/// workspace writes that property in production yet, so on a real database most
/// entities land in `Unknown`; they are kept rather than dropped so the counts
/// stay truthful. An entity whose geometry cannot be read is reported through
/// the returned diagnostics instead of being silently skipped.
fn parse_dxf_from_db(db: &Database) -> anyhow::Result<bcad_dxf::ParsedDxf> {
    let mut doc = bcad_dxf::ParsedDxf::new();
    // The database stores layer colour as RGBA, `ParsedLayer` wants an AutoCAD
    // Color Index. The 256-entry ACI palette lives in no crate here, and
    // reverse-engineering one from RGB would invent a mapping nobody agreed to,
    // so every layer reports ACI 7 (white, the default) and an info diagnostic
    // says the real index is not recoverable.
    const ACI_WHITE: i32 = 7;
    for l in db.layers()? {
        doc.diagnostics.push(bcad_format::Diagnostic::info(
            "DOC-LAYER-001",
            format!(
                "layer {}: stored as RGBA, reported as ACI {ACI_WHITE}",
                l.name
            ),
        ));
        doc.layers.push(bcad_format::ParsedLayer {
            name: l.name,
            color: ACI_WHITE,
            line_weight: l.line_weight,
            visible: l.visible,
            locked: l.locked,
            // `frozen` and `plot` have no counterpart in the database schema,
            // so they take their DXF defaults rather than a guessed value.
            frozen: false,
            plot: true,
            line_type: l.line_type.to_string(),
        });
    }

    for record in db.entities()? {
        let entity_type = match geometry_of(&record) {
            Some(Ok(parsed)) => parsed,
            // Unusable geometry still lands in the document as `Unknown`, with a
            // diagnostic saying why. Dropping it would make the entity counts
            // disagree with the database, and the count is the number a user
            // checks the file against.
            Some(Err(why)) => {
                doc.diagnostics.push(bcad_format::Diagnostic::warning(
                    "DOC-GEOM-001",
                    format!("entity {} ({}): {why}", record.id, record.type_id),
                ));
                bcad_format::ParsedEntityType::Unknown {
                    type_name: record.type_id,
                    raw_groups: Vec::new(),
                }
            }
            None => bcad_format::ParsedEntityType::Unknown {
                type_name: record.type_id,
                raw_groups: Vec::new(),
            },
        };
        doc.entities.push(bcad_format::ParsedEntity {
            handle: record.handle,
            layer: record.layer,
            entity_type,
            properties: record.properties,
            xdata: Default::default(),
        });
    }

    Ok(doc)
}

/// Reads the `geometry` property of `record`.
///
/// `None` when the entity carries no geometry at all, which is not an error:
/// the type is simply not one validation can reason about. `Some(Err)` when a
/// geometry is present but unusable, which is worth telling the user about.
fn geometry_of(
    record: &bcad_db::EntityRecord,
) -> Option<Result<bcad_format::ParsedEntityType, String>> {
    let raw = match record.properties.get("geometry") {
        Some(bcad_format::PropertyValue::String(s)) => s,
        Some(_) => return Some(Err("`geometry` is not a string".to_string())),
        None => return None,
    };

    let value: serde_json::Value = match serde_json::from_str(raw) {
        Ok(v) => v,
        Err(e) => return Some(Err(format!("`geometry` is not valid JSON: {e}"))),
    };

    let coords = value.get("coordinates");
    let pair = |v: &serde_json::Value| -> Option<[f64; 3]> {
        let a = v.as_array()?;
        if a.len() < 2 {
            return None;
        }
        Some([
            a[0].as_f64()?,
            a[1].as_f64()?,
            a.get(2).and_then(serde_json::Value::as_f64).unwrap_or(0.0),
        ])
    };

    let entity_type = match value.get("type").and_then(serde_json::Value::as_str) {
        Some("Point") => match coords.and_then(pair) {
            Some(position) => bcad_format::ParsedEntityType::Point { position },
            None => return Some(Err("Point needs [x, y]".to_string())),
        },
        // Two points are a segment, more are a polyline. GeoJSON has no
        // separate segment type, so the distinction is ours to make.
        Some("LineString") => {
            let points: Option<Vec<[f64; 3]>> = coords
                .and_then(serde_json::Value::as_array)
                .and_then(|a| a.iter().map(pair).collect::<Option<Vec<_>>>());
            match points {
                Some(p) if p.len() == 2 => bcad_format::ParsedEntityType::Line {
                    start: p[0],
                    end: p[1],
                },
                Some(p) if p.len() > 2 => bcad_format::ParsedEntityType::Polyline {
                    vertices: p,
                    closed: false,
                    elevation: 0.0,
                },
                _ => return Some(Err("LineString needs at least 2 points".to_string())),
            }
        }
        Some("Polygon") => {
            let ring: Option<Vec<[f64; 3]>> = value
                .get("coordinates")
                .and_then(serde_json::Value::as_array)
                .and_then(|rings| rings.first())
                .and_then(serde_json::Value::as_array)
                .and_then(|a| a.iter().map(pair).collect::<Option<Vec<_>>>());
            match ring {
                Some(vertices) => bcad_format::ParsedEntityType::Polyline {
                    vertices,
                    closed: true,
                    elevation: 0.0,
                },
                None => return Some(Err("Polygon needs a non-empty exterior ring".to_string())),
            }
        }
        other => {
            return Some(Err(format!(
                "unsupported geometry type {:?}",
                other.unwrap_or("<missing>")
            )))
        }
    };

    Some(Ok(entity_type))
}

fn migrate_cmd(input: &PathBuf, output: &PathBuf, dry_run: bool) -> anyhow::Result<()> {
    let db = bcad_db::open_readwrite(input)?;
    let version = db.schema_version()?;

    if version == 2 {
        println!("Already at schema version 2, no migration needed");
        return Ok(());
    }

    if version != 1 {
        return Err(anyhow::anyhow!(
            "Unsupported schema version for migration: {}",
            version
        ));
    }

    println!("Migrating from v1 to v2...");
    println!("Input: {}", input.display());
    println!("Output: {}", output.display());

    if dry_run {
        println!("DRY RUN - no changes written");
        return Ok(());
    }

    // No v1 schema exists anywhere in the workspace: `create_new` writes v2,
    // and `bcad_db::migrate_v1_to_v2` is itself an empty `execute_batch`.
    // Migrating would mean inventing the source layout, so this refuses. It
    // used to print a notice and return `Ok(())`, exiting 0 with the database
    // untouched: a migration that silently does nothing and reports success.
    let _ = (output, dry_run);
    Err(anyhow::anyhow!(
        "v1 to v2 migration is not implemented: the v1 schema is not defined in \
         this workspace, so there is nothing to migrate from. Reading a v1 \
         database and writing it back as v2 needs the v1 table layout first."
    ))
}

fn report_cmd(file: &PathBuf, output: Option<PathBuf>, pretty: bool) -> anyhow::Result<()> {
    let db = bcad_db::open_readonly(file)?;
    let dxf = parse_dxf_from_db(&db)?;

    let opts = ValidationOptions::default();
    let scope = GeometryScope::of(&dxf);
    let mut report = validate_dxf(&dxf, opts);
    scope.note(&mut report);

    let json = json!({
        "file": file.to_string_lossy(),
        // Without this a consumer cannot tell an all-clear from a run that
        // never saw any geometry.
        "scope": {
            "entities": scope.total,
            "entities_with_geometry": scope.with_geometry,
            "geometry_validated": scope.with_geometry > 0,
        },
        "summary": {
            "errors": report.errors.len(),
            "warnings": report.warnings.len(),
            "infos": report.infos.len(),
        },
        "errors": report.errors,
        "warnings": report.warnings,
        "infos": report.infos,
        // What went wrong while reading: unusable `geometry` values, layers
        // whose ACI could not be recovered. This used to be dropped, which is
        // how a document parsed into nothing could still report all clear.
        "parse_diagnostics": dxf.diagnostics,
    });

    let output_str = if pretty {
        serde_json::to_string_pretty(&json)?
    } else {
        serde_json::to_string(&json)?
    };

    match output {
        Some(path) => std::fs::write(path, output_str)?,
        None => println!("{}", output_str),
    }

    Ok(())
}

fn dxf_cmd(
    file: &PathBuf,
    recovery: RecoveryModeArg,
    output: Option<PathBuf>,
) -> anyhow::Result<()> {
    let opts = ParseOptions {
        recovery: recovery.into(),
        limits: ParseLimits::default(),
        strict_encoding: false,
    };

    let dxf = parse_dxf_file(file, &opts)?;

    if dxf.has_errors() {
        eprintln!(
            "⚠ DXF has {} errors",
            dxf.diagnostics
                .iter()
                .filter(|d| d.severity == Severity::Error)
                .count()
        );
    }

    println!("DXF parsed successfully");
    println!("  Header: {:?}", dxf.header.acad_version);
    println!("  Layers: {}", dxf.layers.len());
    println!("  Entities: {}", dxf.entities.len());
    println!("  Diagnostics: {}", dxf.diagnostics.len());

    if let Some(out_path) = output {
        let json = serde_json::to_string_pretty(&dxf)?;
        // `&out_path`, not `out_path`: the latter moves the PathBuf and the
        // line below would then borrow a moved value.
        std::fs::write(&out_path, json)?;
        println!("Written to {}", out_path.display());
    }

    Ok(())
}

/// Prints what happened while the file was being read.
///
/// Separate from the validation report on purpose: these diagnostics explain
/// why an entity carries no geometry, which is the question a user has when a
/// scope line says "2 of 7 entities checked" and nothing else.
fn print_parse_diagnostics(diagnostics: &[bcad_format::Diagnostic]) {
    if diagnostics.is_empty() {
        return;
    }
    println!("\n📖 While reading the file:");
    for d in diagnostics {
        let marker = match d.severity {
            bcad_format::Severity::Error => "✗",
            bcad_format::Severity::Warning => "⚠",
            bcad_format::Severity::Info => "ℹ",
        };
        println!("  {marker} [{}] {}", d.code, d.message);
    }
}

fn output_report(
    report: &bcad_validation::ValidationReport,
    format: OutputFormat,
    strict: bool,
    parse_diagnostics: &[bcad_format::Diagnostic],
) -> anyhow::Result<()> {
    match format {
        OutputFormat::Human => {
            println!("Validation Report");
            println!("================");
            println!("Errors:   {}", report.errors.len());
            println!("Warnings: {}", report.warnings.len());
            println!("Infos:    {}", report.infos.len());

            if !report.errors.is_empty() {
                println!("\n❌ Errors:");
                for e in &report.errors {
                    println!("  [{}] {}", e.code, e.message);
                    if let Some(s) = &e.suggestion {
                        println!("    💡 {}", s);
                    }
                }
            }
            if !report.warnings.is_empty() {
                println!("\n⚠ Warnings:");
                for w in &report.warnings {
                    println!("  [{}] {}", w.code, w.message);
                }
            }
            if !report.infos.is_empty() {
                println!("\nℹ Infos:");
                for i in &report.infos {
                    println!("  [{}] {}", i.code, i.message);
                }
            }
            print_parse_diagnostics(parse_diagnostics);
        }
        OutputFormat::Json => {
            let json = json!({
                "errors": report.errors,
                "warnings": report.warnings,
                "infos": report.infos,
                "summary": {
                    "error_count": report.errors.len(),
                    "warning_count": report.warnings.len(),
                    "info_count": report.infos.len(),
                    "has_errors": report.has_errors(),
                }
            });
            println!("{}", serde_json::to_string_pretty(&json)?);
        }
    }

    if strict && (report.has_errors() || !report.warnings.is_empty()) {
        return Err(anyhow::anyhow!("Validation failed (strict mode)"));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cli_parsing() {
        use clap::CommandFactory;
        Cli::command().debug_assert();
    }
}
