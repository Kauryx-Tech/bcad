//! Errors raised while reading a DXF file.
//!
//! A `DxfError` always names *where* the problem is: the source line, the group
//! code, and — where relevant — the entity it was reading. This is what lets a
//! caller point at a specific line of a multi-megabyte drawing instead of
//! reporting "parse failed".

use bcad_format::ParseError;
use thiserror::Error;

/// Failure encountered while reading untrusted DXF input.
#[derive(Debug, Error)]
pub enum DxfError {
    /// The group stream is not well formed: a group code was not an integer, or
    /// a code was present with no value line after it.
    #[error("malformed group stream at line {line}: {message}")]
    MalformedStream {
        /// Source line the problem was seen on.
        line: usize,
        /// What was expected and what was found.
        message: String,
    },

    /// Input ended while a value was still expected.
    #[error("unexpected end of input at line {line}")]
    UnexpectedEof {
        /// Last source line that was successfully read.
        line: usize,
    },

    /// A group code was required at this position but another was found.
    #[error("expected group code {expected}, found {found} at line {line}")]
    UnexpectedGroupCode {
        /// Group code the reader required.
        expected: i32,
        /// Group code actually present.
        found: i32,
        /// Source line of the offending group.
        line: usize,
    },

    /// A structural marker was required at this position but another was found.
    #[error("expected marker {expected}, found {found} at line {line}")]
    UnexpectedMarker {
        /// Marker the reader required.
        expected: &'static str,
        /// Marker value actually present.
        found: String,
        /// Source line of the offending marker.
        line: usize,
    },

    /// A group value could not be converted to a number.
    #[error("group {code} on line {line} is not a valid number: {value:?} (entity {entity_type})")]
    InvalidNumeric {
        /// The value as written in the file.
        value: String,
        /// Group code the value belongs to.
        code: i32,
        /// Source line of the group code.
        line: usize,
        /// Entity the group was read for.
        entity_type: String,
    },

    /// A group value parsed as a number but is not finite.
    #[error(
        "group {code} on line {line} must be a finite number, got {value:?} (entity {entity_type})"
    )]
    NonFinite {
        /// The value as written in the file.
        value: String,
        /// Group code the value belongs to.
        code: i32,
        /// Source line of the group code.
        line: usize,
        /// Entity the group was read for.
        entity_type: String,
    },

    /// A group required by this entity type was absent.
    #[error("entity {entity_type} is missing required group {code} (near line {line})")]
    MissingGroup {
        /// Group code the entity requires.
        code: i32,
        /// Entity that requires it.
        entity_type: String,
        /// Source line used as an anchor for the entity.
        line: usize,
    },

    /// An XDATA block was not terminated by its closing brace.
    #[error("unterminated XDATA for application {appid} at line {line}")]
    UnterminatedXData {
        /// Application identifier of the unterminated block.
        appid: String,
        /// Source line where the block started.
        line: usize,
    },

    /// A configured resource limit was reached.
    #[error("resource limit exceeded: {0}")]
    ResourceLimit(
        /// Which limit was reached, and by how much.
        String,
    ),

    /// The bytes could not be decoded as text.
    #[error("encoding error: {0}")]
    Encoding(
        /// What could not be decoded, or read.
        String,
    ),

    /// A neutral format-contract error.
    #[error(transparent)]
    Format(#[from] ParseError),
}

impl DxfError {
    /// Source line the error refers to, when one is known.
    #[must_use]
    pub const fn line(&self) -> Option<usize> {
        match self {
            Self::MalformedStream { line, .. }
            | Self::UnexpectedEof { line }
            | Self::UnexpectedGroupCode { line, .. }
            | Self::UnexpectedMarker { line, .. }
            | Self::InvalidNumeric { line, .. }
            | Self::NonFinite { line, .. }
            | Self::MissingGroup { line, .. }
            | Self::UnterminatedXData { line, .. } => Some(*line),
            Self::ResourceLimit(_) | Self::Encoding(_) | Self::Format(_) => None,
        }
    }

    /// `true` when a configured resource ceiling was reached.
    ///
    /// This is the one error that [`RecoveryMode::Recover`](crate::recovery::RecoveryMode)
    /// must **not** downgrade. Every other error describes a defect in the file,
    /// and a document that records the defect is still a faithful account of what
    /// the file said. A breached limit is not a defect: it is the reader refusing
    /// to go further. The document built so far is truncated, and returning it as
    /// `Ok` would hand back a partial drawing that looks complete — which is the
    /// silently-clamped geometry this crate refuses to produce.
    #[must_use]
    pub const fn is_resource_limit(&self) -> bool {
        matches!(self, Self::ResourceLimit(_))
    }

    /// Builds a stream-level error.
    #[must_use]
    pub fn malformed_stream(line: usize, message: impl Into<String>) -> Self {
        Self::MalformedStream {
            line,
            message: message.into(),
        }
    }

    /// Builds an unexpected-end-of-input error.
    #[must_use]
    pub const fn unexpected_eof(line: usize) -> Self {
        Self::UnexpectedEof { line }
    }

    /// Builds a group-code mismatch error.
    #[must_use]
    pub const fn unexpected_group_code(expected: i32, found: i32, line: usize) -> Self {
        Self::UnexpectedGroupCode {
            expected,
            found,
            line,
        }
    }

    /// Builds a structural-marker mismatch error.
    pub fn unexpected_marker(
        expected: &'static str,
        found: impl Into<String>,
        line: usize,
    ) -> Self {
        Self::UnexpectedMarker {
            expected,
            found: found.into(),
            line,
        }
    }

    /// Builds a numeric conversion error.
    pub fn invalid_numeric(
        value: impl Into<String>,
        code: i32,
        line: usize,
        entity_type: impl Into<String>,
    ) -> Self {
        Self::InvalidNumeric {
            value: value.into(),
            code,
            line,
            entity_type: entity_type.into(),
        }
    }

    /// Builds a non-finite value error.
    pub fn non_finite(
        value: impl Into<String>,
        code: i32,
        line: usize,
        entity_type: impl Into<String>,
    ) -> Self {
        Self::NonFinite {
            value: value.into(),
            code,
            line,
            entity_type: entity_type.into(),
        }
    }

    /// Builds a missing-required-group error.
    pub fn missing_group(code: i32, entity_type: impl Into<String>, line: usize) -> Self {
        Self::MissingGroup {
            code,
            entity_type: entity_type.into(),
            line,
        }
    }

    /// Builds a resource-limit error.
    #[must_use]
    pub fn resource_limit(message: impl Into<String>) -> Self {
        Self::ResourceLimit(message.into())
    }
}

/// Result alias for DXF reading.
pub type DxfResult<T> = Result<T, DxfError>;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn numeric_error_mentions_line_value_and_entity() {
        let err = DxfError::invalid_numeric("NaN", 10, 42, "LINE");
        let text = err.to_string();
        assert!(text.contains("10"), "{text}");
        assert!(text.contains("42"), "{text}");
        assert!(text.contains("LINE"), "{text}");
        assert_eq!(err.line(), Some(42));
    }

    #[test]
    fn missing_group_reports_entity() {
        let err = DxfError::missing_group(40, "CIRCLE", 7);
        assert!(err.to_string().contains("CIRCLE"));
        assert!(err.to_string().contains("40"));
        assert_eq!(err.line(), Some(7));
    }

    #[test]
    fn resource_limit_has_no_line() {
        let err = DxfError::resource_limit("too big");
        assert_eq!(err.line(), None);
    }
}
