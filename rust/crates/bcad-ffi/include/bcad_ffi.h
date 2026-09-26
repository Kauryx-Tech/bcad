/* bcad_ffi.h -- C ABI of the bcad-ffi crate.
 *
 * This header is the contract. It is hand-written rather than generated, and
 * `rust/crates/bcad-ffi/src/lib.rs` holds a test (`the_header_declares_every_
 * exported_function`) that fails if the two drift apart. The reason for keeping
 * it hand-written: the header is installed with the SDK (ADR-006), so building
 * it must not require a code generator on the consumer's machine.
 *
 * What is contractual here is the *layout* of the `repr(C)` types and the
 * numeric values of `BcErrorCode`. The spelling of the names is not.
 *
 * Rules that hold for every entry point:
 *   - no panic unwinds into C; failures come back as a `BcErrorCode`
 *   - a null out-parameter is rejected with `BCAD_INVALID_ARGUMENT`
 *   - `options` may be null, which means "the Rust defaults"
 *
 * Ownership: every `BcString` handed out owns heap memory the caller releases
 * with `bcad_string_free`. Arrays come back as a pointer plus a count and are
 * released by the matching `_free`, which frees the array *and* the strings
 * inside it. Handles are released by their own `_free`/`_close`.
 */

#ifndef BCAD_FFI_H
#define BCAD_FFI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque: the layout is Rust's business, and a caller that guessed it would
 * break the moment a field is added. */
typedef struct ParsedDxfHandle ParsedDxfHandle;
typedef struct DatabaseHandle DatabaseHandle;

/* The numeric values are the ABI. `ResourceLimit` is reachable whenever a
 * ParseLimits ceiling is breached, in both recovery modes. */
typedef enum BcErrorCode {
    BCAD_OK = 0,
    BCAD_INVALID_ARGUMENT = 1,
    BCAD_IO_ERROR = 2,
    BCAD_PARSE_ERROR = 3,
    BCAD_INVALID_FORMAT = 4,
    BCAD_RESOURCE_LIMIT = 5,
    BCAD_NOT_FOUND = 6,
    BCAD_INTERNAL_ERROR = 255
} BcErrorCode;

/* An owned, counted run of bytes. Not necessarily NUL-terminated: read `len`
 * bytes, do not assume `ptr[0..len]` terminates. Release with
 * `bcad_string_free`. */
typedef struct BcString {
    const char *ptr;
    size_t len;
} BcString;

/* Parse tuning. Pass a null pointer for the defaults (Recover, 100 MiB,
 * 1,000,000 entities).
 *
 * `timeout_ms` is accepted for layout compatibility and ignored: bcad-dxf has
 * no timeout, and honouring one would mean guessing a deadline the caller never
 * agreed to. */
typedef struct BcParseOptions {
    int recovery_mode; /* 0 = Strict, 1 = Recover, 2 = Repair */
    unsigned long max_file_size;
    unsigned long max_entities;
    unsigned long timeout_ms; /* ignored */
} BcParseOptions;

/* Parse outcome. `handle` is null unless `error_code` is `BCAD_OK`; when it is
 * null, `error_message` owns the reason and must still be released. */
typedef struct BcDxfParseResult {
    ParsedDxfHandle *handle;
    BcErrorCode error_code;
    BcString error_message;
    unsigned long entity_count;
    unsigned long layer_count;
    unsigned long diagnostic_count;
} BcDxfParseResult;

/* Database open outcome, same handle/message convention. */
typedef struct BcDbOpenResult {
    DatabaseHandle *handle;
    BcErrorCode error_code;
    BcString error_message;
} BcDbOpenResult;

/* A layer as the C++ core already models it. Colours are 0.0..=1.0 because
 * that is what `geom::Color` stores; the AutoCAD Color Index is resolved on the
 * Rust side against a table mirrored from `include/bcad/io/DxfColor.h`.
 *
 * `line_type` is an index into BCAD_LINETYPE_*, not a DXF name. */
typedef struct BcLayer {
    BcString name;
    float color_r;
    float color_g;
    float color_b;
    double line_weight;
    int visible;
    int locked;
    int line_type;
} BcLayer;

/* `line_type` values. The order matches `layers::Layer::LineType`, but the
 * bridge maps them explicitly rather than casting, so a reordering on either
 * side becomes a compile error instead of silent corruption. */
#define BCAD_LINETYPE_CONTINUOUS 0
#define BCAD_LINETYPE_DASHED 1
#define BCAD_LINETYPE_DOTTED 2
#define BCAD_LINETYPE_DASHDOT 3

/* An entity *summary*, not the entity.
 *
 * This is the boundary's deliberate limit: crossing with real geometry would
 * need one `repr(C)` type per entity kind, a far larger ABI than this bridge is
 * meant to have. `property_count` is what the entity carries, not what has been
 * transferred -- the values themselves are not exposed yet. */
typedef struct BcEntitySummary {
    unsigned long id;
    BcString type_id;
    BcString layer;
    BcString handle;
    unsigned long property_count;
} BcEntitySummary;

/* `severity`: 0 = info, 1 = warning, 2 = error, matching bcad-format. */
typedef struct BcDiagnostic {
    int severity;
    BcString code;
    BcString message;
    BcString suggestion;
} BcDiagnostic;

/* --- error reporting ---------------------------------------------------- */

/* Message for the last failure on this thread, or a null `BcString` when there
 * is none. The caller owns the result. */
BcString bcad_last_error(void);

/* Releases a `BcString` obtained from this API. Safe on a null `ptr`. */
void bcad_string_free(BcString s);

/* --- DXF import --------------------------------------------------------- */

/* Parses a file. `path` must be a valid NUL-terminated C string. */
BcErrorCode bcad_dxf_parse_file(const char *path,
                                const BcParseOptions *options,
                                BcDxfParseResult *out_result);

/* Parses a buffer. `data`/`len` are borrowed for the duration of the call. */
BcErrorCode bcad_dxf_parse_bytes(const unsigned char *data,
                                 size_t len,
                                 const BcParseOptions *options,
                                 BcDxfParseResult *out_result);

/* Releases a parse handle and everything it owns. Safe on null. */
void bcad_dxf_free(ParsedDxfHandle *handle);

BcErrorCode bcad_dxf_layer_count(ParsedDxfHandle *handle, unsigned long *out_count);

BcErrorCode bcad_dxf_entity_count(ParsedDxfHandle *handle, unsigned long *out_count);

/* On success `*out_layers` owns `*out_count` elements; release with
 * `bcad_layers_free`. */
BcErrorCode bcad_dxf_get_layers(ParsedDxfHandle *handle,
                                BcLayer **out_layers,
                                unsigned long *out_count);

/* Releases a layer array and the `BcString` names in it. */
void bcad_layers_free(BcLayer *layers, unsigned long count);

/* On success `*out_entities` owns `*out_count` elements; release with
 * `bcad_entity_summaries_free`. */
BcErrorCode bcad_dxf_get_entities(ParsedDxfHandle *handle,
                                 BcEntitySummary **out_entities,
                                 unsigned long *out_count);

/* Releases a summary array and the strings in it. */
void bcad_entity_summaries_free(BcEntitySummary *entities, unsigned long count);

/* On success `*out_diagnostics` owns `*out_count` elements; release with
 * `bcad_diagnostics_free`. */
BcErrorCode bcad_dxf_get_diagnostics(ParsedDxfHandle *handle,
                                     BcDiagnostic **out_diagnostics,
                                     unsigned long *out_count);

/* Releases a diagnostic array and the strings in it. */
void bcad_diagnostics_free(BcDiagnostic *diagnostics, unsigned long count);

/* --- database ----------------------------------------------------------- */

/* `readonly` is 0 for read-write, non-zero for read-only. */
BcErrorCode bcad_db_open(const char *path, int readonly, BcDbOpenResult *out_result);

void bcad_db_close(DatabaseHandle *handle);

BcErrorCode bcad_db_layer_count(DatabaseHandle *handle, unsigned long *out_count);

BcErrorCode bcad_db_entity_count(DatabaseHandle *handle, unsigned long *out_count);

BcErrorCode bcad_db_schema_version(DatabaseHandle *handle, int *out_version);

/* --- ABI ---------------------------------------------------------------- */

/* Packed as 0xMMmmpp00. Bump the low byte for a compatible addition, the minor
 * for a backwards-compatible change, the major for a break. */
unsigned int bcad_ffi_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* Layout, checked by the compiler rather than by a comment. The values mirror
 * the `repr(C)` definitions in `rust/crates/bcad-ffi/src/lib.rs`; a field added
 * on one side without the other fails here at build time. `bcad-ffi`'s
 * `the_struct_layouts_are_the_ones_the_header_declares` test checks the same
 * numbers from the Rust side. */
#if defined(__cplusplus)
#  define BCAD_FFI_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define BCAD_FFI_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#  define BCAD_FFI_ASSERT(cond, msg)
#endif

BCAD_FFI_ASSERT(sizeof(void *) == 8, "bcad-ffi targets LP64");
BCAD_FFI_ASSERT(sizeof(BcString) == 16, "BcString is a pointer and a length");
BCAD_FFI_ASSERT(sizeof(float) == 4 && sizeof(double) == 8, "IEEE types");
BCAD_FFI_ASSERT(sizeof(BcErrorCode) == 4, "a C enum, as repr(C) assumes");

#endif /* BCAD_FFI_H */
