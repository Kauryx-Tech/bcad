#pragma once

// Bridge C++ <-> Rust (bcad-ffi) for reading DXF.
//
// # Boundary
//
// The Rust ABI is deliberately *not* part of this header. `bcad_ffi.h` and its
// `Bc*` types stay inside `DxfBridge.cpp`: they are an implementation detail of
// the bridge. Re-declaring them here would publish a second, hand-written copy
// of a contract that has exactly one authority, `rust/crates/bcad-ffi`, and the
// two would drift. A header here that declares a type the crate does not have
// compiles until the first call, and then fails to link.
//
// # What actually crosses
//
// The ABI now carries a *layer table*, entity *summaries*, entity *geometries*
// and parser diagnostics. The geometry is exposed via tagged union
// `BcEntityGeometry`. This allows the Rust parser to be the single source of
// truth for DXF import.

#include "bcad/core/Document.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bcad::io {

/// How much the reader forgives a malformed file.
///
/// `Repair` is accepted for compatibility with the FFI's numbering but is
/// treated as `Recover`: there is no third strategy to fall back on, and
/// pretending otherwise would be a silent difference in behaviour.
enum class DxfRecoveryMode : int {
    Strict = 0,  ///< first defect aborts the parse
    Recover = 1, ///< defects become diagnostics, the parse continues
    Repair = 2,  ///< degrades to Recover
};

struct DxfBridgeOptions {
    DxfRecoveryMode recovery_mode = DxfRecoveryMode::Recover;
    std::uint64_t max_file_size = 100ULL * 1024 * 1024;
    std::uint64_t max_entities = 1'000'000;
    /// Accepted and ignored. `bcad-dxf` has no timeout, and honouring one would
    /// mean guessing a deadline the caller never agreed to.
    std::uint64_t timeout_ms = 0;
};

/// A parser or validator message, copied out of the Rust-owned memory.
struct DxfBridgeDiagnostic {
    /// 0 = info, 1 = warning, 2 = error, matching `bcad-format`.
    int severity = 0;
    std::string code;
    std::string message;
    std::string suggestion;
};

struct DxfBridgeResult {
    /// The parse itself succeeded. Says nothing about the drawing: see the note
    /// on `parsed_entity_count`.
    bool success = false;

    /// Set when `success` is false, and when the bridge refuses to continue for
    /// a reason that is not an error (see `imported_layer_count`).
    std::string error_message;

    /// Entities the parser found.
    std::uint64_t parsed_entity_count = 0;

    /// Layers the parser found.
    std::uint64_t parsed_layer_count = 0;

    /// Layers actually created in `document`. A layer the bridge had to skip
    /// makes this smaller than `parsed_layer_count`, and the reason is in
    /// `diagnostics`.
    std::uint64_t imported_layer_count = 0;

    /// Entities actually created in `document`.
    std::uint64_t imported_entity_count = 0;

    /// Everything the parser reported, carried over rather than dropped. A parse
    /// can succeed and still fill this with errors.
    std::vector<DxfBridgeDiagnostic> diagnostics;

    /// Never null when `success` is true. Holds the imported layer table and entities.
    std::unique_ptr<bcad::core::Document> document;
};

/// Reads a DXF file through bcad-ffi, importing layers AND geometry.
DxfBridgeResult readDxfFromFile(const std::string& path,
                                DxfBridgeOptions options = {});

/// As `readDxfFromFile`, from a buffer already in memory.
DxfBridgeResult readDxfFromBytes(const std::vector<std::uint8_t>& data,
                                 DxfBridgeOptions options = {});

/// Reads a DXF file through bcad-ffi. See the note at the top of this file: the
/// result carries the layer table, the diagnostics and the counts, not geometry.
/// @deprecated Use readDxfFromFile instead.
DxfBridgeResult readDxfMetadataFromFile(const std::string& path,
                                        DxfBridgeOptions options = {});

/// As `readDxfMetadataFromFile`, from a buffer already in memory.
/// @deprecated Use readDxfFromBytes instead.
DxfBridgeResult readDxfMetadataFromBytes(const std::vector<std::uint8_t>& data,
                                         DxfBridgeOptions options = {});

/// `true` when any diagnostic is an error, i.e. the parse reported a defect even
/// though it returned a result.
[[nodiscard]] bool hasErrors(const DxfBridgeResult& result);

/// Writes a DXF file from a Document using the Rust exporter.
/// Returns true on success, false on failure with error_message set.
struct DxfWriteResult {
    bool success = false;
    std::string error_message;
};

/// Writes a Document to a DXF file through bcad-ffi.
DxfWriteResult writeDxfToFile(const core::Document& doc, const std::string& path);

/// Writes a Document to DXF bytes through bcad-ffi.
DxfWriteResult writeDxfToBytes(const core::Document& doc, std::vector<std::uint8_t>& outData);

/// Validation severity levels (matching Rust ValidationSeverity).
enum class ValidationSeverity : int {
    Info = 0,
    Warning = 1,
    Error = 2,
};

/// Validation options passed to Rust validator.
struct ValidationOptions {
    bool check_self_intersection = true;
    bool check_degenerate = true;
    bool check_duplicate_points = true;
    bool check_overlap = true;
    double tolerance = 1e-9;
    bool parallel = true;
};

/// A validation issue (error, warning, or info).
struct ValidationIssue {
    ValidationSeverity severity;
    std::string code;
    std::string message;
    std::uint64_t entity_id = 0;
    bool has_entity_id = false;
};

/// Validation report containing all issues.
struct ValidationReport {
    bool success = false;
    std::string error_message;
    std::vector<ValidationIssue> issues;
};

/// Validates a Document using the Rust validator.
/// The Document must have been created by readDxfFromFile/Bytes (which creates the parse handle).
ValidationReport validateDocument(const core::Document& doc, const ValidationOptions& options = {});

// --- Database bridge ------------------------------------------------------
//
// These map one-to-one onto real `bcad_db_*` entry points and need no caveat.

struct DatabaseBridgeResult {
    bool success = false;
    std::string error_message;
    /// Pass to `closeDatabase`. Owned; null unless `success`.
    void* handle = nullptr;
};

/// Opens the SQLite database at `path`.
DatabaseBridgeResult openDatabase(const std::string& path, bool readonly = false);

/// Releases a handle from `openDatabase`. Safe on null.
void closeDatabase(void* handle);

[[nodiscard]] std::size_t databaseLayerCount(void* handle);

[[nodiscard]] std::size_t databaseEntityCount(void* handle);

/// The schema version, or -1 if the handle is null or the call failed.
[[nodiscard]] int databaseSchemaVersion(void* handle);

}  // namespace bcad::io
