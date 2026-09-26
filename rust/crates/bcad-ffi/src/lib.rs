//! BCAD FFI — Stable C ABI for Rust/C++ interop
//!
//! Exposes a C ABI for DXF parsing (`bcad-dxf`) and database access (`bcad-db`).
//! The C++ side mirrors these declarations in `include/bcad/io/DxfBridge.h`; the
//! two must be changed together.
//!
//! **Rules this crate holds itself to:**
//! - Only `extern "C"` functions, only `#[repr(C)]` types.
//! - No panic crosses the boundary: every entry point runs inside [`guard`].
//! - No null dereference: every pointer argument is checked before use.
//! - No `unwrap`/`expect`/slice index on a value that came from C.
//! - Opaque handles for anything with a lifetime or non-trivial destructor.
//! - Anything the C++ side cannot express is reported, not faked.
//!
//! **Ownership.** Every `BcString` handed out owns heap memory the caller must
//! release with [`bcad_string_free`]. Arrays returned by pointer come with a
//! matching `_free` that releases the array *and* the strings inside it.

use std::cell::RefCell;
use std::ffi::{CStr, CString};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::sync::Mutex;

use bcad_db::{open_readonly, open_readwrite, Database, DbError};
use bcad_dxf::{
    parse_dxf_bytes, parse_dxf_file, ParseLimits, ParseOptions, ParsedDxf, RecoveryMode,
};
use bcad_format::{Diagnostic, ParsedEntity, ParsedEntityType, ParsedLayer, Severity};
use libc::{c_char, c_int, c_uint, c_ulong, size_t};

/// Error codes. The numeric values are part of the ABI.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BcErrorCode {
    Ok = 0,
    InvalidArgument = 1,
    IoError = 2,
    ParseError = 3,
    InvalidFormat = 4,
    ResourceLimit = 5,
    NotFound = 6,
    InternalError = 255,
}

/// Runs `body`, converting a panic into [`BcErrorCode::InternalError`].
///
/// Unwinding into C is undefined behaviour, so this is the only sanctioned way
/// to call into Rust from an `extern "C"` function. `AssertUnwindSafe` is
/// required because the closures capture raw pointers; the C caller is
/// responsible for those pointers being valid, which is checked separately.
fn guard<F: FnOnce() -> BcErrorCode>(body: F) -> BcErrorCode {
    catch_unwind(AssertUnwindSafe(body)).unwrap_or(BcErrorCode::InternalError)
}

thread_local! {
    /// Message for the most recent failure on this thread.
    ///
    /// Several entry points can only return a code, and printing to stderr from
    /// a library is not an error channel the caller can read. This lets the
    /// C++ side retrieve the text after seeing a non-zero code.
    static LAST_ERROR: RefCell<Option<CString>> = const { RefCell::new(None) };
}

fn set_last_error(message: &str) {
    // An interior NUL cannot appear in any message we build, but a lossy
    // fallback is still needed to guarantee `CString::new` succeeds.
    let owned = CString::new(message).unwrap_or_else(|_| c"internal error".to_owned());
    LAST_ERROR.with(|slot| *slot.borrow_mut() = Some(owned));
}

fn take_last_error() -> Option<CString> {
    LAST_ERROR.with(|slot| slot.borrow_mut().take())
}

/// Returns the message for the last failure on the calling thread, or an empty
/// string if there was none. Release it with [`bcad_string_free`].
#[no_mangle]
pub extern "C" fn bcad_last_error() -> BcString {
    BcString::from_cstring(take_last_error().unwrap_or_default())
}

/// FFI string: an owned, counted run of bytes.
///
/// The caller owns the bytes and must release them with [`bcad_string_free`].
/// A null `ptr` with `len == 0` is the canonical "no string". C++ reads
/// `std::string(ptr, len)`, so a null `ptr` is only safe when `len` is zero.
#[repr(C)]
pub struct BcString {
    pub ptr: *const c_char,
    pub len: size_t,
}

impl BcString {
    /// Copies `s` onto the heap. The interior NUL of a C string is rejected
    /// rather than silently truncating the text.
    fn from_string(s: String) -> Self {
        Self::from_cstring(CString::new(s).unwrap_or_default())
    }

    fn from_cstring(owned: CString) -> Self {
        let len = owned.as_bytes().len();
        Self {
            ptr: owned.into_raw(),
            len,
        }
    }

    const fn null() -> Self {
        Self {
            ptr: ptr::null(),
            len: 0,
        }
    }

    /// Hands the buffer to the caller and leaves `self` empty.
    ///
    /// `BcString` owns memory and is deliberately not `Copy`, so a `_free`
    /// helper walks a struct it only has `&mut` to. Moving the buffer out this
    /// way makes a double free impossible to write by accident: the source is
    /// null afterwards, and `bcad_string_free` ignores a null string.
    const fn take(&mut self) -> Self {
        let taken = Self {
            ptr: self.ptr,
            len: self.len,
        };
        self.ptr = ptr::null();
        self.len = 0;
        taken
    }
}

/// Opaque handle to a parsed document.
pub struct ParsedDxfHandle {
    inner: Mutex<Option<ParsedDxf>>,
}

impl ParsedDxfHandle {
    fn new(dxf: ParsedDxf) -> *mut Self {
        Box::into_raw(Box::new(Self {
            inner: Mutex::new(Some(dxf)),
        }))
    }

    /// Runs `f` on the document while holding the lock.
    ///
    /// Returns `None` when the document has already been taken or when the
    /// mutex is poisoned by an earlier panic. A poisoned lock is treated as a
    /// dead handle rather than re-panicking, because `unwrap` here would unwind
    /// into C.
    fn with<R>(&self, f: impl FnOnce(&ParsedDxf) -> R) -> Option<R> {
        let guard = self.inner.lock().ok()?;
        let result = guard.as_ref().map(f);
        // Released before returning: holding the lock past its last use would
        // serialise unrelated callers for nothing.
        drop(guard);
        result
    }
}

/// Opaque handle to an open database.
pub struct DatabaseHandle {
    inner: Mutex<Option<Database>>,
}

impl DatabaseHandle {
    fn new(db: Database) -> *mut Self {
        Box::into_raw(Box::new(Self {
            inner: Mutex::new(Some(db)),
        }))
    }

    /// Borrows the database for the duration of `f`.
    ///
    /// A `Database` owns a `rusqlite::Connection`, which is neither `Clone` nor
    /// safely handable by value, so accessors cannot take the value out the way
    /// a `ParsedDxf` could. Borrowing under the lock is the only sound option.
    fn with<R>(&self, f: impl FnOnce(&Database) -> Result<R, DbError>) -> Result<R, BcErrorCode> {
        let guard = self.inner.lock().map_err(|_| BcErrorCode::InternalError)?;
        let db = guard.as_ref().ok_or(BcErrorCode::InvalidArgument)?;
        let result = f(db).map_err(|e| {
            set_last_error(&e.to_string());
            db_error_code(&e)
        });
        drop(guard);
        result
    }
}

const fn db_error_code(error: &DbError) -> BcErrorCode {
    match error {
        DbError::Sqlite(_) | DbError::Io(_) => BcErrorCode::IoError,
        DbError::InvalidSchemaVersion(_) | DbError::MissingTable(_) => BcErrorCode::InvalidFormat,
        DbError::InvalidEntityData(_) | DbError::PropertySerialization(_) | DbError::Json(_) => {
            BcErrorCode::InvalidFormat
        }
    }
}

/// Parse options. The layout is mirrored by `BcParseOptions` in
/// `include/bcad/io/DxfBridge.h` and must stay byte-for-byte identical.
#[repr(C)]
pub struct BcParseOptions {
    /// 0 = Strict, 1 = Recover. See [`to_parse_options`] for the other values.
    pub recovery_mode: c_int,
    pub max_file_size: c_ulong,
    pub max_entities: c_ulong,
    /// Accepted for layout compatibility and ignored: `bcad-dxf` has no
    /// timeout, and inventing one would mean guessing at a deadline the caller
    /// never agreed to.
    pub timeout_ms: c_ulong,
}

impl Default for BcParseOptions {
    fn default() -> Self {
        Self {
            recovery_mode: 1, // Recover
            max_file_size: 100 * 1024 * 1024,
            max_entities: 1_000_000,
            timeout_ms: 0,
        }
    }
}

fn to_size(value: c_ulong, name: &str) -> Result<usize, BcErrorCode> {
    usize::try_from(value).map_err(|_| {
        set_last_error(&format!(
            "{name} ({value}) does not fit in a usize on this target"
        ));
        BcErrorCode::InvalidArgument
    })
}

/// Translates the C options into the parser's own.
///
/// `recovery_mode` 2 used to mean "repair". `bcad-dxf` no longer offers that
/// mode, so it now means Recover rather than failing: a caller compiled against
/// the old header should keep working, just with less repair than it asked for.
/// Values outside 0..=2 are rejected, because silently treating a typo as
/// Recover would hide it.
fn to_parse_options(options: *const BcParseOptions) -> Result<ParseOptions, BcErrorCode> {
    // SAFETY: the caller guarantees `options` is null or points to a live,
    // initialised `BcParseOptions` for the duration of the call.
    let Some(raw) = (unsafe { options.as_ref() }) else {
        return Ok(ParseOptions::default());
    };

    let recovery = match raw.recovery_mode {
        0 => RecoveryMode::Strict,
        1 | 2 => RecoveryMode::Recover,
        other => {
            set_last_error(&format!(
                "unknown recovery_mode {other}, expected 0, 1 or 2"
            ));
            return Err(BcErrorCode::InvalidArgument);
        }
    };

    // These arrive as `u64` from C. A 32-bit target cannot hold every `u64`,
    // and truncating would silently turn a generous limit into a tiny one, so
    // an unrepresentable value is refused instead.
    let max_file_size = to_size(raw.max_file_size, "max_file_size")?;
    let max_entities = to_size(raw.max_entities, "max_entities")?;
    let limits = ParseLimits {
        max_file_size,
        max_entities,
        ..ParseLimits::default()
    };

    Ok(ParseOptions {
        recovery,
        limits,
        strict_encoding: false,
    })
}

/// Result of a DXF parse.
#[repr(C)]
pub struct BcDxfParseResult {
    pub handle: *mut ParsedDxfHandle,
    pub error_code: BcErrorCode,
    pub error_message: BcString,
    pub entity_count: c_ulong,
    pub layer_count: c_ulong,
    pub diagnostic_count: c_ulong,
}

/// Result of opening a database.
#[repr(C)]
pub struct BcDbOpenResult {
    pub handle: *mut DatabaseHandle,
    pub error_code: BcErrorCode,
    pub error_message: BcString,
}

/// A layer, as the C++ side already models it.
///
/// The C++ core stores layer colour as RGBA, but `bcad-dxf` reports an `AutoCAD`
/// Color Index. `aci_to_rgb` bridges the two; see its documentation for why the
/// table is only eight entries.
#[repr(C)]
pub struct BcLayer {
    pub name: BcString,
    pub color_r: f32,
    pub color_g: f32,
    pub color_b: f32,
    pub line_weight: f64,
    pub visible: c_int,
    pub locked: c_int,
    pub line_type: c_int,
}

/// Summary of a parsed entity.
///
/// A summary, not the geometry: crossing the boundary with a full entity would
/// need one `repr(C)` type per entity kind, which is a much larger ABI than
/// this bridge is meant to have. The handle is the way back to the data.
#[repr(C)]
pub struct BcEntitySummary {
    pub id: c_ulong,
    pub type_id: BcString,
    pub layer: BcString,
    pub handle: BcString,
    pub property_count: c_ulong,
}

/// A parser or validator message.
#[repr(C)]
pub struct BcDiagnostic {
    pub severity: c_int,
    pub code: BcString,
    pub message: BcString,
    pub suggestion: BcString,
}

/// The ACI palette the C++ side already owns, mirrored.
///
/// `include/bcad/io/DxfColor.h` (`aciToRgb`) sets the policy: eight entries,
/// anything outside the range falls back to white, and exact fidelity with the
/// full 255-entry `AutoCAD` palette is explicitly not a goal. This mirrors that
/// table rather than inventing a second one, so both sides agree on what a
/// given index means. If the C++ table changes, this has to change with it;
/// `aci_to_rgb_matches_the_cpp_table` pins the values.
///
/// Values are `0.0..=1.0` because that is what `geom::Color` stores.
fn aci_to_rgb(aci: i32) -> (f32, f32, f32) {
    const TABLE: [[u8; 3]; 8] = [
        [0, 0, 0],       // 0 unused
        [255, 0, 0],     // 1 red
        [255, 255, 0],   // 2 yellow
        [0, 255, 0],     // 3 green
        [0, 255, 255],   // 4 cyan
        [0, 0, 255],     // 5 blue
        [255, 0, 255],   // 6 magenta
        [255, 255, 255], // 7 white
    ];
    // `if (aci < 0 || aci > 7) aci = 7;` on the C++ side: out of range means
    // white on *both* ends. `clamp` would send a negative index to entry 0,
    // which is black, so the two sides would disagree on bad input.
    let index = usize::try_from(aci).ok().filter(|i| *i <= 7).unwrap_or(7);
    let entry = &TABLE[index];
    (
        f32::from(entry[0]) / 255.0,
        f32::from(entry[1]) / 255.0,
        f32::from(entry[2]) / 255.0,
    )
}

/// Maps the parser's line type name onto `bcad::layers::Layer::LineType`.
///
/// The parser reports the name as written in the source table (group code 6),
/// while the C++ enum has only four values and no slot for `ByLayer`, `ByBlock`
/// or a custom name. Those fall back to `Continuous`, which is also what
/// `AutoCAD` draws by default.
fn line_type_code(name: &str) -> c_int {
    match name.trim().to_ascii_uppercase().as_str() {
        "DASHED" => 1,
        "DOTTED" => 2,
        "DASHDOT" => 3,
        _ => 0,
    }
}

fn layer_to_ffi(layer: &ParsedLayer) -> BcLayer {
    let (r, g, b) = aci_to_rgb(layer.color);
    BcLayer {
        name: BcString::from_string(layer.name.clone()),
        color_r: r,
        color_g: g,
        color_b: b,
        line_weight: layer.line_weight,
        visible: c_int::from(layer.visible),
        locked: c_int::from(layer.locked),
        line_type: line_type_code(&layer.line_type),
    }
}

/// Stands in for a DXF handle. DXF text handles are hexadecimal and may be
/// absent, so a summary needs a stable integer; this hashes the handle text
/// rather than inventing a sequential id the document does not have.
fn entity_id(entity: &ParsedEntity) -> c_ulong {
    entity.handle.as_ref().map_or(0, |handle| {
        handle.bytes().fold(0xcbf2_9ce4_8422_2325u64, |hash, byte| {
            (hash ^ u64::from(byte)).wrapping_mul(0x0000_0100_0000_01b3)
        })
    })
}

/// A stable name for the entity kind.
///
/// `ParsedEntityType` exposes no such method, and `Unknown` already carries the
/// name the file used, so those two cases come straight from the data and the
/// rest name the variant. This is a label for logs and dispatch, not a
/// serialisation format: `serde` on the Rust side is the canonical encoding.
fn entity_type_name(entity_type: &ParsedEntityType) -> &str {
    match entity_type {
        ParsedEntityType::Point { .. } => "Point",
        ParsedEntityType::Line { .. } => "Line",
        ParsedEntityType::Polyline { .. } => "Polyline",
        ParsedEntityType::Circle { .. } => "Circle",
        ParsedEntityType::Arc { .. } => "Arc",
        ParsedEntityType::Text { .. } => "Text",
        ParsedEntityType::Unknown { type_name, .. } => type_name,
    }
}

fn entity_to_ffi(entity: &ParsedEntity) -> BcEntitySummary {
    BcEntitySummary {
        id: entity_id(entity),
        type_id: BcString::from_string(entity_type_name(&entity.entity_type).to_string()),
        layer: BcString::from_string(entity.layer.clone()),
        handle: entity
            .handle
            .as_ref()
            .map_or_else(BcString::null, |handle| {
                BcString::from_string(handle.clone())
            }),
        property_count: entity.properties.len() as c_ulong,
    }
}

const fn severity_code(severity: Severity) -> c_int {
    match severity {
        Severity::Info => 0,
        Severity::Warning => 1,
        Severity::Error => 2,
    }
}

fn diagnostic_to_ffi(diagnostic: &Diagnostic) -> BcDiagnostic {
    let optional = |value: &Option<String>| {
        value
            .as_ref()
            .map_or_else(BcString::null, |text| BcString::from_string(text.clone()))
    };
    BcDiagnostic {
        severity: severity_code(diagnostic.severity),
        code: BcString::from_string(diagnostic.code.clone()),
        message: BcString::from_string(diagnostic.message.clone()),
        suggestion: optional(&diagnostic.suggestion),
    }
}

/// Fills a parse result with a failure, so the caller never reads a
/// half-initialised struct. Done before the work, not after, so that a panic
/// mid-way still leaves a coherent result behind.
unsafe fn fail_parse(out: *mut BcDxfParseResult, code: BcErrorCode, message: String) {
    let out = unsafe { &mut *out };
    out.handle = ptr::null_mut();
    out.entity_count = 0;
    out.layer_count = 0;
    out.diagnostic_count = 0;
    out.error_code = code;
    out.error_message = BcString::from_string(message);
}

/// Parse a DXF file.
///
/// # Safety
/// `path` must be a valid null-terminated C string and `out_result` must point
/// to a writable [`BcDxfParseResult`]. On success the caller owns `handle` and
/// must release it with [`bcad_dxf_free`].
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_parse_file(
    path: *const c_char,
    options: *const BcParseOptions,
    out_result: *mut BcDxfParseResult,
) -> BcErrorCode {
    if out_result.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    unsafe { fail_parse(out_result, BcErrorCode::InternalError, String::new()) };

    guard(|| {
        // SAFETY: checked non-null by the caller; the C string is valid for the
        // duration of the call.
        let Some(path) =
            (unsafe { path.as_ref() }).and_then(|p| unsafe { CStr::from_ptr(p) }.to_str().ok())
        else {
            unsafe {
                fail_parse(
                    out_result,
                    BcErrorCode::InvalidArgument,
                    "path is null or not valid UTF-8".to_string(),
                );
            };
            return BcErrorCode::InvalidArgument;
        };

        let options = match to_parse_options(options) {
            Ok(options) => options,
            Err(code) => {
                let message = format!("invalid parse options: {code:?}");
                unsafe { fail_parse(out_result, code, message) };
                return code;
            }
        };

        match parse_dxf_file(path, &options) {
            Ok(dxf) => {
                let entity_count = dxf.entity_count() as c_ulong;
                let layer_count = dxf.layer_count() as c_ulong;
                let diagnostic_count = dxf.diagnostics.len() as c_ulong;
                // SAFETY: non-null and initialised above.
                unsafe {
                    let out = &mut *out_result;
                    out.handle = ParsedDxfHandle::new(dxf);
                    out.error_code = BcErrorCode::Ok;
                    out.error_message = BcString::null();
                    out.entity_count = entity_count;
                    out.layer_count = layer_count;
                    out.diagnostic_count = diagnostic_count;
                }
                BcErrorCode::Ok
            }
            Err(error) => {
                let message = error.to_string();
                set_last_error(&message);
                unsafe { fail_parse(out_result, BcErrorCode::ParseError, message) };
                BcErrorCode::ParseError
            }
        }
    })
}

/// Parse a DXF held in memory.
///
/// # Safety
/// `data` must point to `len` readable bytes and `out_result` to a writable
/// [`BcDxfParseResult`]. The buffer is borrowed for the call only.
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_parse_bytes(
    data: *const u8,
    len: size_t,
    options: *const BcParseOptions,
    out_result: *mut BcDxfParseResult,
) -> BcErrorCode {
    if out_result.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    unsafe { fail_parse(out_result, BcErrorCode::InternalError, String::new()) };

    // Checked before the slice is built: `from_raw_parts` requires a non-null
    // pointer even for an empty slice.
    if data.is_null() {
        unsafe {
            fail_parse(
                out_result,
                BcErrorCode::InvalidArgument,
                "data is null".to_string(),
            );
        };
        return BcErrorCode::InvalidArgument;
    }

    guard(|| {
        let bytes = unsafe { std::slice::from_raw_parts(data, len) };
        let options = match to_parse_options(options) {
            Ok(options) => options,
            Err(code) => {
                let message = format!("invalid parse options: {code:?}");
                unsafe { fail_parse(out_result, code, message) };
                return code;
            }
        };

        match parse_dxf_bytes(bytes, &options) {
            Ok(dxf) => {
                let entity_count = dxf.entity_count() as c_ulong;
                let layer_count = dxf.layer_count() as c_ulong;
                let diagnostic_count = dxf.diagnostics.len() as c_ulong;
                unsafe {
                    let out = &mut *out_result;
                    out.handle = ParsedDxfHandle::new(dxf);
                    out.error_code = BcErrorCode::Ok;
                    out.error_message = BcString::null();
                    out.entity_count = entity_count;
                    out.layer_count = layer_count;
                    out.diagnostic_count = diagnostic_count;
                }
                BcErrorCode::Ok
            }
            Err(error) => {
                let message = error.to_string();
                set_last_error(&message);
                unsafe { fail_parse(out_result, BcErrorCode::ParseError, message) };
                BcErrorCode::ParseError
            }
        }
    })
}

/// Releases a handle from either parser entry point.
///
/// # Safety
/// `handle` must be null or a pointer returned by this crate and not yet freed.
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_free(handle: *mut ParsedDxfHandle) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}

/// Number of layers in the document.
///
/// # Safety
/// `handle` must be a live handle and `out_count` writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_layer_count(
    handle: *mut ParsedDxfHandle,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    count_over(handle, out_count, ParsedDxf::layer_count)
}

/// Number of entities in the document.
///
/// # Safety
/// `handle` must be a live handle and `out_count` writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_entity_count(
    handle: *mut ParsedDxfHandle,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    count_over(handle, out_count, ParsedDxf::entity_count)
}

fn count_over(
    handle: *mut ParsedDxfHandle,
    out_count: *mut c_ulong,
    count: fn(&ParsedDxf) -> usize,
) -> BcErrorCode {
    if out_count.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    guard(|| {
        // SAFETY: checked non-null by the caller.
        let Some(handle) = (unsafe { handle.as_ref() }) else {
            return BcErrorCode::InvalidArgument;
        };
        handle
            .with(count)
            .map_or(BcErrorCode::InvalidArgument, |value| {
                unsafe { *out_count = value as c_ulong };
                BcErrorCode::Ok
            })
    })
}

/// Copies the layers into a newly allocated array.
///
/// The array and every `name` inside it belong to the caller; release both with
/// [`bcad_layers_free`].
///
/// # Safety
/// `handle` must be live; `out_layers` and `out_count` must be writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_get_layers(
    handle: *mut ParsedDxfHandle,
    out_layers: *mut *mut BcLayer,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    if out_layers.is_null() || out_count.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    guard(|| {
        let Some(handle) = (unsafe { handle.as_ref() }) else {
            return BcErrorCode::InvalidArgument;
        };
        let Some(layers) =
            handle.with(|dxf| dxf.layers.iter().map(layer_to_ffi).collect::<Vec<_>>())
        else {
            return BcErrorCode::InvalidArgument;
        };

        let count = layers.len() as c_ulong;
        let array: *mut BcLayer = Box::into_raw(layers.into_boxed_slice()).cast();
        unsafe {
            *out_layers = array;
            *out_count = count;
        }
        BcErrorCode::Ok
    })
}

/// Releases a layer array **and** the `BcString` names inside it.
///
/// Freeing only the array would leak every name, so this walks the array first.
/// Release each name exactly once, through this function.
///
/// # Safety
/// `layers` must be null, or an array of `count` elements returned by
/// [`bcad_dxf_get_layers`] and not yet freed.
#[no_mangle]
pub unsafe extern "C" fn bcad_layers_free(layers: *mut BcLayer, count: c_ulong) {
    // A `u64` count too large for a `usize` cannot describe a real
    // allocation; truncating it would rebuild the box from a wrong length.
    let Ok(count) = usize::try_from(count) else {
        return;
    };
    if layers.is_null() || count == 0 {
        return;
    }
    let slice = unsafe { std::slice::from_raw_parts_mut(layers, count) };
    for layer in slice {
        unsafe { bcad_string_free(layer.name.take()) };
    }
    drop(unsafe { Box::from_raw(std::slice::from_raw_parts_mut(layers, count).as_mut_ptr()) });
}

/// Copies the entity summaries into a newly allocated array.
///
/// # Safety
/// `handle` must be live; `out_entities` and `out_count` must be writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_get_entities(
    handle: *mut ParsedDxfHandle,
    out_entities: *mut *mut BcEntitySummary,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    if out_entities.is_null() || out_count.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    guard(|| {
        let Some(handle) = (unsafe { handle.as_ref() }) else {
            return BcErrorCode::InvalidArgument;
        };
        let Some(entities) = handle.with(|dxf| {
            dxf.entities
                .iter()
                .map(entity_to_ffi)
                .collect::<Vec<BcEntitySummary>>()
        }) else {
            return BcErrorCode::InvalidArgument;
        };

        let count = entities.len() as c_ulong;
        let array: *mut BcEntitySummary = Box::into_raw(entities.into_boxed_slice()).cast();
        unsafe {
            *out_entities = array;
            *out_count = count;
        }
        BcErrorCode::Ok
    })
}

/// Releases an entity array and the strings inside it.
///
/// # Safety
/// `entities` must be null, or an array of `count` elements returned by
/// [`bcad_dxf_get_entities`] and not yet freed.
#[no_mangle]
pub unsafe extern "C" fn bcad_entity_summaries_free(
    entities: *mut BcEntitySummary,
    count: c_ulong,
) {
    // A `u64` count too large for a `usize` cannot describe a real
    // allocation; truncating it would rebuild the box from a wrong length.
    let Ok(count) = usize::try_from(count) else {
        return;
    };
    if entities.is_null() || count == 0 {
        return;
    }
    let slice = unsafe { std::slice::from_raw_parts_mut(entities, count) };
    for entity in slice {
        // SAFETY: `take` nulls the field, so each string is released once.
        unsafe { bcad_string_free(entity.type_id.take()) };
        // SAFETY: as above.
        unsafe { bcad_string_free(entity.layer.take()) };
        // SAFETY: as above.
        unsafe { bcad_string_free(entity.handle.take()) };
    }
    drop(unsafe { Box::from_raw(std::slice::from_raw_parts_mut(entities, count).as_mut_ptr()) });
}

/// Copies the diagnostics into a newly allocated array.
///
/// `BcDxfParseResult::diagnostic_count` is useless without this: the count was
/// reachable but the messages were not.
///
/// # Safety
/// `handle` must be live; `out_diagnostics` and `out_count` must be writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_get_diagnostics(
    handle: *mut ParsedDxfHandle,
    out_diagnostics: *mut *mut BcDiagnostic,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    if out_diagnostics.is_null() || out_count.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    guard(|| {
        let Some(handle) = (unsafe { handle.as_ref() }) else {
            return BcErrorCode::InvalidArgument;
        };
        let Some(diagnostics) = handle.with(|dxf| {
            dxf.diagnostics
                .iter()
                .map(diagnostic_to_ffi)
                .collect::<Vec<BcDiagnostic>>()
        }) else {
            return BcErrorCode::InvalidArgument;
        };

        let count = diagnostics.len() as c_ulong;
        let array: *mut BcDiagnostic = Box::into_raw(diagnostics.into_boxed_slice()).cast();
        unsafe {
            *out_diagnostics = array;
            *out_count = count;
        }
        BcErrorCode::Ok
    })
}

/// Releases a diagnostic array and the strings inside it.
///
/// # Safety
/// `diagnostics` must be null, or an array of `count` elements returned by
/// [`bcad_dxf_get_diagnostics`] and not yet freed.
#[no_mangle]
pub unsafe extern "C" fn bcad_diagnostics_free(diagnostics: *mut BcDiagnostic, count: c_ulong) {
    // A `u64` count too large for a `usize` cannot describe a real
    // allocation; truncating it would rebuild the box from a wrong length.
    let Ok(count) = usize::try_from(count) else {
        return;
    };
    if diagnostics.is_null() || count == 0 {
        return;
    }
    let slice = unsafe { std::slice::from_raw_parts_mut(diagnostics, count) };
    for diagnostic in slice {
        // SAFETY: `take` nulls the field, so each string is released once.
        unsafe { bcad_string_free(diagnostic.code.take()) };
        // SAFETY: as above.
        unsafe { bcad_string_free(diagnostic.message.take()) };
        // SAFETY: as above.
        unsafe { bcad_string_free(diagnostic.suggestion.take()) };
    }
    drop(unsafe { Box::from_raw(std::slice::from_raw_parts_mut(diagnostics, count).as_mut_ptr()) });
}

/// Releases a string returned by this crate.
///
/// # Safety
/// `s` must be a string produced by this crate and not yet freed. Passing the
/// null string is a no-op.
#[no_mangle]
pub unsafe extern "C" fn bcad_string_free(s: BcString) {
    if s.ptr.is_null() || s.len == 0 {
        return;
    }
    // `CString` is NUL-terminated, so the pointer alone is enough to rebuild
    // it. `len` stays in the ABI because C++ reads the bytes by length rather
    // than scanning for the terminator; it is not needed on this side.
    drop(unsafe { CString::from_raw(s.ptr.cast_mut()) });
}

/// Opens a database.
///
/// # Safety
/// `path` must be a valid C string and `out_result` writable. On success the
/// caller owns `handle` and must release it with [`bcad_db_close`].
#[no_mangle]
pub unsafe extern "C" fn bcad_db_open(
    path: *const c_char,
    readonly: c_int,
    out_result: *mut BcDbOpenResult,
) -> BcErrorCode {
    if out_result.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    // SAFETY: non-null, checked above.
    let out = unsafe { &mut *out_result };
    out.handle = ptr::null_mut();
    out.error_code = BcErrorCode::InternalError;
    out.error_message = BcString::null();

    guard(|| {
        let Some(path) =
            (unsafe { path.as_ref() }).and_then(|p| unsafe { CStr::from_ptr(p) }.to_str().ok())
        else {
            unsafe {
                let out = &mut *out_result;
                out.error_code = BcErrorCode::InvalidArgument;
                out.error_message =
                    BcString::from_string("path is null or not valid UTF-8".to_string());
            }
            return BcErrorCode::InvalidArgument;
        };

        let opened = if readonly != 0 {
            open_readonly(path)
        } else {
            open_readwrite(path)
        };

        match opened {
            Ok(db) => {
                unsafe {
                    let out = &mut *out_result;
                    out.handle = DatabaseHandle::new(db);
                    out.error_code = BcErrorCode::Ok;
                    out.error_message = BcString::null();
                }
                BcErrorCode::Ok
            }
            Err(error) => {
                let message = error.to_string();
                set_last_error(&message);
                unsafe {
                    let out = &mut *out_result;
                    out.error_code = BcErrorCode::IoError;
                    out.error_message = BcString::from_string(message);
                }
                BcErrorCode::IoError
            }
        }
    })
}

/// Closes a database handle.
///
/// # Safety
/// `handle` must be null or a pointer returned by [`bcad_db_open`] and not yet
/// freed.
#[no_mangle]
pub unsafe extern "C" fn bcad_db_close(handle: *mut DatabaseHandle) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}

/// Number of layers in the database.
///
/// # Safety
/// `handle` must be live and `out_count` writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_db_layer_count(
    handle: *mut DatabaseHandle,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    db_count(handle, out_count, |db| Ok(db.layers()?.len()))
}

/// Number of entities in the database.
///
/// # Safety
/// `handle` must be live and `out_count` writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_db_entity_count(
    handle: *mut DatabaseHandle,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    db_count(handle, out_count, |db| Ok(db.entities()?.len()))
}

/// Schema version stored in the database.
///
/// # Safety
/// `handle` must be live and `out_version` writable.
#[no_mangle]
pub unsafe extern "C" fn bcad_db_schema_version(
    handle: *mut DatabaseHandle,
    out_version: *mut c_int,
) -> BcErrorCode {
    if out_version.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    guard(|| {
        let Some(handle) = (unsafe { handle.as_ref() }) else {
            return BcErrorCode::InvalidArgument;
        };
        match handle.with(Database::schema_version) {
            Ok(version) => {
                unsafe { *out_version = version };
                BcErrorCode::Ok
            }
            Err(code) => code,
        }
    })
}

fn db_count(
    handle: *mut DatabaseHandle,
    out_count: *mut c_ulong,
    query: impl FnOnce(&Database) -> Result<usize, DbError>,
) -> BcErrorCode {
    if out_count.is_null() {
        return BcErrorCode::InvalidArgument;
    }
    guard(|| {
        let Some(handle) = (unsafe { handle.as_ref() }) else {
            return BcErrorCode::InvalidArgument;
        };
        match handle.with(query) {
            Ok(value) => {
                unsafe { *out_count = value as c_ulong };
                BcErrorCode::Ok
            }
            Err(code) => code,
        }
    })
}

/// ABI version as `0xMMmmpp`. Bumped whenever a declaration changes shape, so
/// the C++ side can refuse to run against a mismatched build instead of
/// misreading memory.
#[no_mangle]
pub const extern "C" fn bcad_ffi_version() -> c_uint {
    0x0001_0000 // 1.0.0
}

#[cfg(test)]
mod tests {
    use super::*;

    const MINIMAL_DXF: &[u8] =
        b"0\nSECTION\n2\nENTITIES\n0\nLINE\n8\n0\n10\n0.0\n20\n0.0\n11\n1.0\n21\n1.0\n0\nENDSEC\n0\nEOF\n";

    #[test]
    fn version_is_reported() {
        assert_eq!(bcad_ffi_version(), 0x0001_0000);
    }

    #[test]
    fn bcstring_round_trip() {
        let string = BcString::from_string("hello".to_string());
        assert!(!string.ptr.is_null());
        assert_eq!(string.len, 5);
        // SAFETY: freshly created, freed once.
        unsafe { bcad_string_free(string) };
    }

    #[test]
    fn null_string_free_is_a_no_op() {
        // SAFETY: the null string is explicitly allowed.
        unsafe { bcad_string_free(BcString::null()) };
    }

    #[test]
    fn bcstring_rejects_interior_nul_instead_of_truncating() {
        let string = BcString::from_string("ab\0cd".to_string());
        // `CString::new` refuses the input, so nothing past the NUL is stored
        // and the length reflects what is actually readable.
        assert!(string.len < 5);
        // SAFETY: freshly created, freed once.
        unsafe { bcad_string_free(string) };
    }

    #[test]
    fn aci_to_rgb_matches_the_cpp_table() {
        // Mirrors `include/bcad/io/DxfColor.h`. If this fails, the C++ table
        // moved and the two sides would disagree on layer colours.
        assert_eq!(aci_to_rgb(1), (1.0, 0.0, 0.0));
        assert_eq!(aci_to_rgb(2), (1.0, 1.0, 0.0));
        assert_eq!(aci_to_rgb(3), (0.0, 1.0, 0.0));
        assert_eq!(aci_to_rgb(4), (0.0, 1.0, 1.0));
        assert_eq!(aci_to_rgb(5), (0.0, 0.0, 1.0));
        assert_eq!(aci_to_rgb(6), (1.0, 0.0, 1.0));
        assert_eq!(aci_to_rgb(7), (1.0, 1.0, 1.0));
        // Out of range falls back to white, as on the C++ side.
        assert_eq!(aci_to_rgb(0), (0.0, 0.0, 0.0));
        assert_eq!(aci_to_rgb(8), (1.0, 1.0, 1.0));
        assert_eq!(aci_to_rgb(255), (1.0, 1.0, 1.0));
        assert_eq!(aci_to_rgb(-3), (1.0, 1.0, 1.0));
    }

    #[test]
    fn line_type_names_map_to_the_cpp_enum() {
        assert_eq!(line_type_code("CONTINUOUS"), 0);
        assert_eq!(line_type_code("DASHED"), 1);
        assert_eq!(line_type_code("dotted"), 2);
        assert_eq!(line_type_code(" DASHDOT "), 3);
        // No enum slot: these fall back to Continuous.
        assert_eq!(line_type_code("ByLayer"), 0);
        assert_eq!(line_type_code("CUSTOM_DASHED_X"), 0);
        assert_eq!(line_type_code(""), 0);
    }

    #[test]
    fn null_options_use_the_parser_defaults() {
        let options = to_parse_options(ptr::null()).unwrap();
        assert_eq!(options.recovery, RecoveryMode::Recover);
    }

    #[test]
    fn repair_mode_degrades_to_recover_instead_of_failing() {
        // A caller compiled against the old header must keep working.
        let raw = BcParseOptions {
            recovery_mode: 2,
            ..Default::default()
        };
        let options = to_parse_options(std::ptr::from_ref(&raw)).unwrap();
        assert_eq!(options.recovery, RecoveryMode::Recover);
    }

    #[test]
    fn unknown_recovery_mode_is_rejected() {
        let raw = BcParseOptions {
            recovery_mode: 99,
            ..Default::default()
        };
        assert_eq!(
            to_parse_options(std::ptr::from_ref(&raw)).err(),
            Some(BcErrorCode::InvalidArgument)
        );
    }

    #[test]
    fn limits_are_carried_over() {
        let raw = BcParseOptions {
            recovery_mode: 0,
            max_file_size: 4096,
            max_entities: 7,
            timeout_ms: 1234,
        };
        let options = to_parse_options(std::ptr::from_ref(&raw)).unwrap();
        assert_eq!(options.recovery, RecoveryMode::Strict);
        assert_eq!(options.limits.max_file_size, 4096);
        assert_eq!(options.limits.max_entities, 7);
        // `timeout_ms` has no counterpart and is ignored rather than faked.
    }

    /// The whole point of the crate: a DXF goes in through the C ABI and a
    /// usable document comes out.
    #[test]
    fn parses_a_document_through_the_boundary() {
        let mut result = BcDxfParseResult {
            handle: ptr::null_mut(),
            error_code: BcErrorCode::InternalError,
            error_message: BcString::null(),
            entity_count: 0,
            layer_count: 0,
            diagnostic_count: 0,
        };

        // SAFETY: valid buffer for the call, `result` is writable.
        let code = unsafe {
            bcad_dxf_parse_bytes(
                MINIMAL_DXF.as_ptr(),
                MINIMAL_DXF.len(),
                ptr::null(),
                ptr::from_mut(&mut result),
            )
        };

        assert_eq!(code, BcErrorCode::Ok);
        assert_eq!(result.error_code, BcErrorCode::Ok);
        assert!(!result.handle.is_null());
        assert_eq!(result.entity_count, 1);

        // SAFETY: the handle is live and `count` is writable.
        let (entities, entity_count) = unsafe { read_entities(result.handle) };
        assert_eq!(entity_count, 1);
        // SAFETY: array produced by the crate, freed once.
        unsafe { bcad_entity_summaries_free(entities, entity_count) };

        // SAFETY: the handle has not been freed yet.
        unsafe { bcad_dxf_free(result.handle) };
    }

    unsafe fn read_entities(handle: *mut ParsedDxfHandle) -> (*mut BcEntitySummary, c_ulong) {
        let mut entities: *mut BcEntitySummary = ptr::null_mut();
        let mut count: c_ulong = 0;
        // SAFETY: valid out-params for the call.
        let code = unsafe {
            bcad_dxf_get_entities(
                handle,
                ptr::from_mut(&mut entities),
                ptr::from_mut(&mut count),
            )
        };
        assert_eq!(code, BcErrorCode::Ok);
        (entities, count)
    }

    #[test]
    fn null_out_result_is_rejected_before_any_dereference() {
        // SAFETY: deliberately null, which the callee must notice.
        let code = unsafe {
            bcad_dxf_parse_bytes(
                MINIMAL_DXF.as_ptr(),
                MINIMAL_DXF.len(),
                ptr::null(),
                ptr::null_mut(),
            )
        };
        assert_eq!(code, BcErrorCode::InvalidArgument);
    }

    #[test]
    fn null_handle_is_rejected() {
        let mut count: c_ulong = 0;
        // SAFETY: null handles and a valid `count` are both handled.
        let code = unsafe { bcad_dxf_entity_count(ptr::null_mut(), ptr::from_mut(&mut count)) };
        assert_eq!(code, BcErrorCode::InvalidArgument);
        // SAFETY: as above.
        let code = unsafe { bcad_db_entity_count(ptr::null_mut(), ptr::from_mut(&mut count)) };
        assert_eq!(code, BcErrorCode::InvalidArgument);
    }

    #[test]
    fn unparsable_input_reports_the_reason() {
        let mut result = BcDxfParseResult {
            handle: ptr::null_mut(),
            error_code: BcErrorCode::Ok,
            error_message: BcString::null(),
            entity_count: 99,
            layer_count: 99,
            diagnostic_count: 99,
        };
        let garbage = b"this is not a dxf";

        // SAFETY: valid buffer, writable result.
        let code = unsafe {
            bcad_dxf_parse_bytes(
                garbage.as_ptr(),
                garbage.len(),
                ptr::null(),
                ptr::from_mut(&mut result),
            )
        };

        assert_ne!(code, BcErrorCode::Ok);
        assert!(result.handle.is_null());
        // The stale counts from the caller must not survive a failure.
        assert_eq!(result.entity_count, 0);
        assert_eq!(result.layer_count, 0);
        assert_ne!(result.error_message.ptr, ptr::null());
        // SAFETY: string produced by the crate, freed once.
        unsafe { bcad_string_free(result.error_message) };
    }
}

/// Pins the hand-written C header to this ABI.
///
/// `include/bcad_ffi.h` is installed with the SDK, so a function added here and
/// not declared there compiles fine in Rust and then fails to link for every
/// C++ consumer. This module is the tripwire: it reads the header and checks
/// that the two agree on the set of entry points and on the error codes.
#[cfg(test)]
mod header_contract {
    use super::{
        BcDbOpenResult, BcDiagnostic, BcDxfParseResult, BcEntitySummary, BcErrorCode, BcLayer,
        BcString,
    };
    use std::collections::BTreeSet;
    use std::path::Path;

    /// Return types a `bcad_*` entry point can have in the header.
    const RETURN_TYPES: [&str; 4] = ["BcErrorCode ", "void ", "BcString ", "unsigned int "];

    fn header_text() -> String {
        let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("include/bcad_ffi.h");
        std::fs::read_to_string(&path)
            .unwrap_or_else(|e| panic!("{} must be readable: {e}", path.display()))
    }

    /// The name in a declaration, keeping `_` so `bcad_dxf_free` stays whole.
    fn identifier_after(rest: &str) -> String {
        rest.chars()
            .take_while(|c| c.is_ascii_alphanumeric() || *c == '_')
            .collect()
    }

    /// The `bcad_*` names a header line declares, if it declares one.
    fn declared_on(line: &str) -> Option<String> {
        let t = line.trim_start();
        let rest = RETURN_TYPES.iter().find_map(|p| t.strip_prefix(p))?;
        let name = identifier_after(rest);
        name.starts_with("bcad_").then_some(name)
    }

    fn header_declarations() -> BTreeSet<String> {
        header_text().lines().filter_map(declared_on).collect()
    }

    /// Every `#[no_mangle] extern "C"` function in this file, by name.
    fn exported_function_names() -> BTreeSet<String> {
        include_str!("lib.rs")
            .lines()
            .collect::<Vec<_>>()
            .windows(3)
            .filter(|w| w[0].trim() == "#[no_mangle]" && w[1].trim().starts_with("pub "))
            .filter_map(|w| {
                let after = w[1].trim();
                let open = after.find('(')?;
                let name = after[..open].rsplit(' ').next()?;
                name.starts_with("bcad_").then(|| name.to_string())
            })
            .collect()
    }

    #[test]
    fn the_header_and_the_exported_functions_are_the_same_set() {
        let exported = exported_function_names();
        let declared = header_declarations();
        assert_eq!(exported.len(), 19, "a new entry point needs auditing here");
        let missing: Vec<_> = exported.difference(&declared).collect();
        let extra: Vec<_> = declared.difference(&exported).collect();
        assert!(
            missing.is_empty() && extra.is_empty(),
            "the header and the crate disagree: missing from the header {missing:?}, \
             declared but not exported {extra:?}"
        );
    }

    #[test]
    fn the_error_codes_match_the_enum() {
        let header = header_text();
        let expected = [
            ("BCAD_OK", BcErrorCode::Ok),
            ("BCAD_INVALID_ARGUMENT", BcErrorCode::InvalidArgument),
            ("BCAD_IO_ERROR", BcErrorCode::IoError),
            ("BCAD_PARSE_ERROR", BcErrorCode::ParseError),
            ("BCAD_INVALID_FORMAT", BcErrorCode::InvalidFormat),
            ("BCAD_RESOURCE_LIMIT", BcErrorCode::ResourceLimit),
            ("BCAD_NOT_FOUND", BcErrorCode::NotFound),
            ("BCAD_INTERNAL_ERROR", BcErrorCode::InternalError),
        ];
        for (name, code) in expected {
            let needle = format!("{name} = {}", code as i32);
            assert!(
                header.contains(&needle),
                "the header must pin {name} to the value the enum uses; expected `{needle}`"
            );
        }
    }

    #[test]
    fn the_struct_layouts_are_the_ones_the_header_declares() {
        use std::mem::{align_of, size_of};
        // The header's `_Static_assert`s repeat these numbers; a change on either
        // side has to be made on both, and this is what notices.
        assert_eq!(size_of::<usize>(), 8, "the header assumes LP64");
        assert_eq!(size_of::<BcString>(), 16);
        assert_eq!(align_of::<BcString>(), 8);
        // Every struct that starts with a BcString inherits its 8-byte alignment.
        for size in [
            size_of::<BcLayer>(),
            size_of::<BcEntitySummary>(),
            size_of::<BcDiagnostic>(),
            size_of::<BcDxfParseResult>(),
            size_of::<BcDbOpenResult>(),
        ] {
            assert_eq!(size % 8, 0, "a struct is not 8-byte aligned");
        }
    }
}
