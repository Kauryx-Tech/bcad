//! BCAD FFI — Stable C ABI for Rust/C++ interop
//!
//! This crate exposes a minimal, stable C ABI for:
//! - DXF parsing (bcad-dxf)
//! - Database operations (bcad-db)
//! - Validation (bcad-validation)
//!
//! **Rules:**
//! - Only `extern "C"` functions
//! - Only `#[repr(C)]` structs
//! - No panic across boundary (`catch_unwind`)
//! - No exceptions across boundary
//! - Opaque handles for complex objects

use bcad_format::*;
use bcad_dxf::*;
use bcad_db::*;
use libc::{c_char, c_int, c_uint, c_ulong, c_void, size_t};
use std::ffi::{CStr, CString};
use std::ptr;
use std::sync::Mutex;

/// Opaque handle for parsed DXF
pub struct ParsedDxfHandle {
    inner: Mutex<Option<ParsedDxf>>,
}

impl ParsedDxfHandle {
    fn new(dxf: ParsedDxf) -> *mut Self {
        Box::into_raw(Box::new(ParsedDxfHandle {
            inner: Mutex::new(Some(dxf)),
        }))
    }

    fn take(&self) -> Option<ParsedDxf> {
        self.inner.lock().unwrap().take()
    }
}

/// Opaque handle for database
pub struct DatabaseHandle {
    inner: Mutex<Option<Database>>,
}

impl DatabaseHandle {
    fn new(db: Database) -> *mut Self {
        Box::into_raw(Box::new(DatabaseHandle {
            inner: Mutex::new(Some(db)),
        }))
    }

    fn get(&self) -> Option<Database> {
        self.inner.lock().unwrap().clone()
    }
}

/// Error codes
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

/// FFI string (owned by caller, must be freed with `bcad_string_free`)
#[repr(C)]
pub struct BcString {
    pub ptr: *const c_char,
    pub len: size_t,
}

impl BcString {
    fn from_string(s: String) -> Self {
        let cstring = CString::new(s).unwrap_or_else(|_| CString::new("").unwrap());
        let ptr = cstring.into_raw();
        Self { ptr, len: unsafe { CStr::from_ptr(ptr).to_bytes().len() } }
    }

    fn from_str(s: &str) -> Self {
        Self::from_string(s.to_string())
    }

    fn null() -> Self {
        Self { ptr: ptr::null(), len: 0 }
    }
}

/// Parse options
#[repr(C)]
pub struct BcParseOptions {
    pub recovery_mode: c_int,  // 0=Strict, 1=Recover, 2=Repair
    pub max_file_size: c_ulong,
    pub max_entities: c_ulong,
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

/// DXF parse result
#[repr(C)]
pub struct BcDxfParseResult {
    pub handle: *mut ParsedDxfHandle,
    pub error_code: BcErrorCode,
    pub error_message: BcString,
    pub entity_count: c_ulong,
    pub layer_count: c_ulong,
    pub diagnostic_count: c_ulong,
}

/// Database open result
#[repr(C)]
pub struct BcDbOpenResult {
    pub handle: *mut DatabaseHandle,
    pub error_code: BcErrorCode,
    pub error_message: BcString,
}

/// Layer info for FFI
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

/// Entity summary for FFI
#[repr(C)]
pub struct BcEntitySummary {
    pub id: c_ulong,
    pub type_id: BcString,
    pub layer: BcString,
    pub handle: BcString,
    pub property_count: c_ulong,
}

/// Validation report for FFI
#[repr(C)]
pub struct BcValidationReport {
    pub error_count: c_ulong,
    pub warning_count: c_ulong,
    pub info_count: c_ulong,
}

/// Parse DXF from file
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_parse_file(
    path: *const c_char,
    options: *const BcParseOptions,
    out_result: *mut BcDxfParseResult,
) -> BcErrorCode {
    let result = std::panic::catch_unwind(|| {
        let path = CStr::from_ptr(path).to_str().unwrap_or("");
        let opts = if options.is_null() {
            ParseOptions::default()
        } else {
            let opts_ref = &*options;
            ParseOptions {
                recovery_mode: match opts_ref.recovery_mode {
                    0 => RecoveryMode::Strict,
                    1 => RecoveryMode::Recover,
                    2 => RecoveryMode::Repair,
                    _ => RecoveryMode::Recover,
                },
                limits: ParseLimits {
                    max_file_size: opts_ref.max_file_size as usize,
                    max_entities: opts_ref.max_entities as usize,
                    timeout_ms: opts_ref.timeout_ms as u64,
                    ..Default::default()
                },
                strict_encoding: false,
            }
        };

        match parse_dxf_file(path, opts) {
            Ok(dxf) => {
                let handle = ParsedDxfHandle::new(dxf);
                let entity_count = dxf.entity_count() as c_ulong;
                let layer_count = dxf.layer_count() as c_ulong;
                let diagnostic_count = dxf.diagnostics.len() as c_ulong;
                (*out_result).handle = handle;
                (*out_result).error_code = BcErrorCode::Ok;
                (*out_result).error_message = BcString::null();
                (*out_result).entity_count = entity_count;
                (*out_result).layer_count = layer_count;
                (*out_result).diagnostic_count = diagnostic_count;
                BcErrorCode::Ok
            }
            Err(e) => {
                let msg = CString::new(e.to_string()).unwrap_or_default();
                (*out_result).handle = ptr::null_mut();
                (*out_result).error_code = BcErrorCode::ParseError;
                (*out_result).error_message = BcString::from_string(msg.to_string_lossy().to_string());
                (*out_result).entity_count = 0;
                (*out_result).layer_count = 0;
                (*out_result).diagnostic_count = 0;
                BcErrorCode::ParseError
            }
        })
        .unwrap_or(BcErrorCode::InternalError)
}

/// Parse DXF from bytes
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_parse_bytes(
    data: *const u8,
    len: size_t,
    options: *const BcParseOptions,
    out_result: *mut BcDxfParseResult,
) -> BcErrorCode {
    let result = std::panic::catch_unwind(|| {
        let slice = std::slice::from_raw_parts(data, len);
        let opts = if options.is_null() {
            ParseOptions::default()
        } else {
            let opts_ref = &*options;
            ParseOptions {
                recovery_mode: match opts_ref.recovery_mode {
                    0 => RecoveryMode::Strict,
                    1 => RecoveryMode::Recover,
                    2 => RecoveryMode::Repair,
                    _ => RecoveryMode::Recover,
                },
                limits: ParseLimits {
                    max_file_size: opts_ref.max_file_size as usize,
                    max_entities: opts_ref.max_entities as usize,
                    timeout_ms: opts_ref.timeout_ms as u64,
                    ..Default::default()
                },
                strict_encoding: false,
            }
        };

        match parse_dxf_bytes(slice, opts) {
            Ok(dxf) => {
                let handle = ParsedDxfHandle::new(dxf);
                let entity_count = dxf.entity_count() as c_ulong;
                let layer_count = dxf.layer_count() as c_ulong;
                let diagnostic_count = dxf.diagnostics.len() as c_ulong;
                (*out_result).handle = handle;
                (*out_result).error_code = BcErrorCode::Ok;
                (*out_result).error_message = BcString::null();
                (*out_result).entity_count = entity_count;
                (*out_result).layer_count = layer_count;
                (*out_result).diagnostic_count = diagnostic_count;
                BcErrorCode::Ok
            }
            Err(e) => {
                let msg = CString::new(e.to_string()).unwrap_or_default();
                (*out_result).handle = ptr::null_mut();
                (*out_result).error_code = BcErrorCode::ParseError;
                (*out_result).error_message = BcString::from_string(msg.to_string_lossy().to_string());
                (*out_result).entity_count = 0;
                (*out_result).layer_count = 0;
                (*out_result).diagnostic_count = 0;
                BcErrorCode::ParseError
            }
        })
        .unwrap_or(BcErrorCode::InternalError)
}

/// Free DXF handle
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_free(handle: *mut ParsedDxfHandle) {
    if !handle.is_null() {
        let _ = Box::from_raw(handle);
    }
}

/// Get layer count
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_layer_count(handle: *mut ParsedDxfHandle, out_count: *mut c_ulong) -> BcErrorCode {
    if handle.is_null() { return BcErrorCode::InvalidArgument; }
    if let Some(h) = handle.as_ref() {
        if let Some(dxf) = h.inner.lock().unwrap().as_ref() {
            *out_count = dxf.layer_count() as c_ulong;
            BcErrorCode::Ok
        } else {
            BcErrorCode::InvalidArgument
        }
    } else {
        BcErrorCode::InvalidArgument
    }
}

/// Get entity count
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_entity_count(handle: *mut ParsedDxfHandle, out_count: *mut c_ulong) -> BcErrorCode {
    if handle.is_null() { return BcErrorCode::InvalidArgument; }
    if let Some(h) = handle.as_ref() {
        if let Some(dxf) = h.inner.lock().unwrap().as_ref() {
            *out_count = dxf.entity_count() as c_ulong;
            BcErrorCode::Ok
        } else {
            BcErrorCode::InvalidArgument
        }
    } else {
        BcErrorCode::InvalidArgument
    }
}

/// Get layers array (caller must free with bcad_layers_free)
#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_get_layers(
    handle: *mut ParsedDxfHandle,
    out_layers: *mut *mut BcLayer,
    out_count: *mut c_ulong,
) -> BcErrorCode {
    if handle.is_null() { return BcErrorCode::InvalidArgument; }
    if let Some(h) = handle.as_ref() {
        if let Some(dxf) = h.inner.lock().unwrap().as_ref() {
            let layers: Vec<BcLayer> = dxf.layers.iter().map(|l| BcLayer {
                name: BcString::from_string(l.name.clone()),
                color_r: l.color.r,
                color_g: l.color.g,
                color_b: l.color.b,
                line_weight: l.line_weight,
                visible: if l.visible { 1 } else { 0 },
                locked: if l.locked { 1 } else { 0 },
                line_type: l.line_type,
            }).collect();

            let count = layers.len() as c_ulong;
            let ptr = layers.into_boxed_slice().into_raw() as *mut BcLayer;
            *out_layers = ptr;
            *out_count = count;
            BcErrorCode::Ok
        } else {
            BcErrorCode::InvalidArgument
        }
    } else {
        BcErrorCode::InvalidArgument
    }
}

/// Free layers array
#[no_mangle]
pub unsafe extern "C" fn bcad_layers_free(layers: *mut BcLayer, count: c_ulong) {
    if !layers.is_null() && count > 0 {
        let _ = Box::from_raw(std::slice::from_raw_parts_mut(layers, count as usize).as_mut_ptr());
    }
}

/// Free string
#[no_mangle]
pub unsafe extern "C" fn bcad_string_free(s: BcString) {
    if !s.ptr.is_null() {
        let _ = CString::from_raw(s.ptr as *mut c_char);
    }
}

/// Open database
#[no_mangle]
pub unsafe extern "C" fn bcad_db_open(
    path: *const c_char,
    readonly: c_int,
    out_result: *mut BcDbOpenResult,
) -> BcErrorCode {
    let result = std::panic::catch_unwind(|| {
        let path = CStr::from_ptr(path).to_str().unwrap_or("");
        let result = if readonly != 0 {
            open_readonly(path)
        } else {
            open_readwrite(path)
        };

        match result {
            Ok(db) => {
                let handle = DatabaseHandle::new(db);
                (*out_result).handle = handle;
                (*out_result).error_code = BcErrorCode::Ok;
                (*out_result).error_message = BcString::null();
                BcErrorCode::Ok
            }
            Err(e) => {
                let msg = CString::new(e.to_string()).unwrap_or_default();
                (*out_result).handle = ptr::null_mut();
                (*out_result).error_code = BcErrorCode::IoError;
                (*out_result).error_message = BcString::from_string(msg.to_string_lossy().to_string());
                BcErrorCode::IoError
            }
        })
        .unwrap_or(BcErrorCode::InternalError)
}

/// Close database
#[no_mangle]
pub unsafe extern "C" fn bcad_db_close(handle: *mut DatabaseHandle) {
    if !handle.is_null() {
        let _ = Box::from_raw(handle);
    }
}

/// Get layer count from database
#[no_mangle]
pub unsafe extern "C" fn bcad_db_layer_count(handle: *mut DatabaseHandle, out_count: *mut c_ulong) -> BcErrorCode {
    if handle.is_null() { return BcErrorCode::InvalidArgument; }
    if let Some(h) = handle.as_ref() {
        if let Some(db) = h.get() {
            match db.layers() {
                Ok(layers) => {
                    *out_count = layers.len() as c_ulong;
                    BcErrorCode::Ok
                }
                Err(e) => {
                    eprintln!("DB layer error: {}", e);
                    BcErrorCode::InternalError
                }
            }
        } else {
            BcErrorCode::InvalidArgument
        }
    } else {
        BcErrorCode::InvalidArgument
    }
}

/// Get entity count from database
#[no_mangle]
pub unsafe extern "C" fn bcad_db_entity_count(handle: *mut DatabaseHandle, out_count: *mut c_ulong) -> BcErrorCode {
    if handle.is_null() { return BcErrorCode::InvalidArgument; }
    if let Some(h) = handle.as_ref() {
        if let Some(db) = h.get() {
            match db.entities() {
                Ok(entities) => {
                    *out_count = entities.len() as c_ulong;
                    BcErrorCode::Ok
                }
                Err(e) => {
                    eprintln!("DB entity error: {}", e);
                    BcErrorCode::InternalError
                }
            }
        } else {
            BcErrorCode::InvalidArgument
        }
    } else {
        BcErrorCode::InvalidArgument
    }
}

/// Get schema version
#[no_mangle]
pub unsafe extern "C" fn bcad_db_schema_version(handle: *mut DatabaseHandle, out_version: *mut c_int) -> BcErrorCode {
    if handle.is_null() { return BcErrorCode::InvalidArgument; }
    if let Some(h) = handle.as_ref() {
        if let Some(db) = h.get() {
            match db.schema_version() {
                Ok(v) => {
                    *out_version = v;
                    BcErrorCode::Ok
                }
                Err(e) => {
                    eprintln!("DB schema error: {}", e);
                    BcErrorCode::InternalError
                }
            }
        } else {
            BcErrorCode::InvalidArgument
        }
    } else {
        BcErrorCode::InvalidArgument
    }
}

/// FFI version
#[no_mangle]
pub extern "C" fn bcad_ffi_version() -> c_uint {
    0x010000 // 1.0.0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ffi_version() {
        assert_eq!(bcad_ffi_version(), 0x010000);
    }

    #[test]
    fn test_bcstring() {
        let s = BcString::from_string("hello".to_string());
        assert!(!s.ptr.is_null());
        assert_eq!(s.len, 5);
        unsafe { bcad_string_free(s); }
    }
}