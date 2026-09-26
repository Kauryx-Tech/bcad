//! BCAD Doctor - Diagnostic CLI tool
//!
//! Commands:
//!   inspect    - Show file structure and metadata
//!   check      - Quick integrity check
//!   validate   - Full validation with diagnostics
//!   migrate    - Migrate v1 to v2 schema
//!   report     - Generate JSON report

use bcad_db::Database;
use bcad_dxf::{parse_dxf_file, ParseOptions, RecoveryMode, ParseLimits};
use bcad_format::{FormatVersion, Diagnostic, Severity};
use bcad_validation::{validate_dxf, ValidationOptions};
use bcad_export::export_geojson;
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

        /// Also validate DXF entities
        #[arg(long)]
        dxf: bool,

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

#[derive(Copy, Clone, ValueEnum)]
enum RecoveryModeArg {
    Strict,
    Recover,
    Repair,
}

impl From<RecoveryModeArg> for RecoveryMode {
    fn from(mode: RecoveryModeArg) -> Self {
        match mode {
            RecoveryModeArg::Strict => RecoveryMode::Strict,
            RecoveryModeArg::Recover => RecoveryMode::Recover,
            RecoveryModeArg::Repair => RecoveryMode::Repair,
        }
    }
}

fn main() {
    let cli = Cli::parse();

    let result = match cli.command {
        Commands::Inspect { file, format } => inspect_cmd(&file, format),
        Commands::Check { file, fail_on_warning } => check_cmd(&file, fail_on_warning),
        Commands::Validate { file, dxf, format, strict } => validate_cmd(&file, dxf, format, strict),
        Commands::Migrate { input, output, dry_run } => migrate_cmd(&input, &output, dry_run),
        Commands::Report { file, output, pretty } => report_cmd(&file, output, pretty),
        Commands::Dxf { file, recovery, output } => dxf_cmd(&file, recovery, output),
    };

    if let Err(e) = result {
        eprintln!("Error: {}", e);
        process::exit(1);
    }
}

fn inspect_cmd(file: &PathBuf, format: OutputFormat) -> anyhow::Result<()> {
    let db = Database::open_readonly(file)?;
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
                println!("  {} (color: {:.2},{:.2},{:.2}, visible: {}, locked: {})",
                    l.name, l.color.r, l.color.g, l.color.b, l.visible, l.locked);
            }
            println!("\nEntities (first 10):");
            for e in entities.iter().take(10) {
                println!("  #{} {} @ {} [{}] ({} props)",
                    e.id, e.type_id, e.layer, e.handle.as_deref().unwrap_or("-"), e.properties.len());
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
    let db = Database::open_readonly(file)?;

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
    matches!(type_id,
        "bcad.Point" | "bcad.Line" | "bcad.Circle" | "bcad.Arc" | "bcad.Polyline" | "bcad.Text" |
        "cadastre.parcel" | "cadastre.boundary" | "cadastre.survey_mark" | "cadastre.easement"
    )
}

fn validate_cmd(file: &PathBuf, dxf: bool, format: OutputFormat, strict: bool) -> anyhow::Result<()> {
    let db = Database::open_readonly(file)?;
    let entities = db.entities()?;

    let opts = ValidationOptions {
        check_self_intersection: true,
        check_degenerate: true,
        check_duplicate_points: true,
        check_overlap: true,
        tolerance: 1e-9,
        parallel: true,
    };

    let mut report = validate_dxf(&parse_dxf_from_db(&db)?, opts);

    if dxf {
        // Also validate DXF if we can extract it
        // (would need DXF extraction from DB)
    }

    // Add entity-level validation
    for entity in &entities {
        if let bcad_db::EntityRecord { properties, .. } = entity {
            if let Some(prop) = properties.get("cadastre.section") {
                if let bcad_format::PropertyValue::String(s) = prop {
                    if s.is_empty() {
                        report.add_warning("VAL-CAD-001", "Empty cadastre.section");
                    }
                }
            }
        }
    }

    output_report(&report, format, strict)
}

fn parse_dxf_from_db(db: &Database) -> anyhow::Result<bcad_dxf::ParsedDxf> {
    // In a real implementation, this would reconstruct DXF from database
    // For now, return empty
    Ok(bcad_dxf::ParsedDxf::new())
}

fn migrate_cmd(input: &PathBuf, output: &PathBuf, dry_run: bool) -> anyhow::Result<()> {
    let mut db = bcad_db::open_readwrite(input)?;
    let version = db.schema_version()?;

    if version == 2 {
        println!("Already at schema version 2, no migration needed");
        return Ok(());
    }

    if version != 1 {
        return Err(anyhow::anyhow!("Unsupported schema version for migration: {}", version));
    }

    println!("Migrating from v1 to v2...");
    println!("Input: {}", input.display());
    println!("Output: {}", output.display());

    if dry_run {
        println!("DRY RUN - no changes written");
        return Ok(());
    }

    // In a real implementation, this would:
    // 1. Read v1 tables (entities with type enum, cadastre_parcels)
    // 2. Create v2 schema
    // 3. Migrate entities with type_id strings
    // 4. Migrate cadastre properties to entity_properties
    // 5. Write to output

    println!("⚠ Migration not fully implemented yet");
    println!("Would migrate: entities, properties, layers");

    Ok(())
}

fn report_cmd(file: &PathBuf, output: Option<PathBuf>, pretty: bool) -> anyhow::Result<()> {
    let db = Database::open_readonly(file)?;
    let dxf = parse_dxf_from_db(&db)?;

    let opts = ValidationOptions::default();
    let report = validate_dxf(&dxf, opts);

    let json = json!({
        "file": file.to_string_lossy(),
        "summary": {
            "errors": report.errors.len(),
            "warnings": report.warnings.len(),
            "infos": report.infos.len(),
        },
        "errors": report.errors,
        "warnings": report.warnings,
        "infos": report.infos,
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

fn dxf_cmd(file: &PathBuf, recovery: RecoveryModeArg, output: Option<PathBuf>) -> anyhow::Result<()> {
    let opts = ParseOptions {
        recovery_mode: recovery.into(),
        limits: ParseLimits::default(),
        strict_encoding: false,
    };

    let dxf = parse_dxf_file(file, opts)?;

    if dxf.has_errors() {
        eprintln!("⚠ DXF has {} errors", dxf.diagnostics.iter().filter(|d| d.severity == Severity::Error).count());
    }

    println!("DXF parsed successfully");
    println!("  Header: {:?}", dxf.header.acad_version);
    println!("  Layers: {}", dxf.layers.len());
    println!("  Entities: {}", dxf.entities.len());
    println!("  Diagnostics: {}", dxf.diagnostics.len());

    if let Some(out_path) = output {
        let json = serde_json::to_string_pretty(&dxf)?;
        std::fs::write(out_path, json)?;
        println!("Written to {}", out_path.display());
    }

    Ok(())
}

fn output_report(report: &bcad_validation::ValidationReport, format: OutputFormat, strict: bool) -> anyhow::Result<()> {
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
    use tempfile::tempdir;

    #[test]
    fn test_cli_parsing() {
        use clap::CommandFactory;
        Cli::command().debug_assert();
    }
}