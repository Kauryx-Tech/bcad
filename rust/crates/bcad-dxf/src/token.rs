//! One DXF group, as produced by the tokenizer.
//!
//! A DXF file is a flat stream of `(group code, value)` pairs. This type is the
//! only representation of that stream inside the crate: the parser owns a
//! `Vec<Token>` and never re-reads the raw text.

/// A single group: an integer `code` and the raw textual `value` that follows it.
///
/// The `value` is kept exactly as it appeared in the source (only the trailing
/// line terminator is removed). No interpretation happens here, so a value that
/// fails to parse later can still be reported verbatim.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Token {
    /// DXF group code.
    pub code: i32,
    /// Raw value text following the group code.
    pub value: String,
    /// 1-based source line on which the group *code* appears.
    ///
    /// This is deliberately the code's line, not the value's, so that
    /// `(line, code)` identifies the group unambiguously.
    pub line: usize,
}

impl Token {
    /// Builds a token. `line` is the 1-based line of the group code.
    pub fn new(code: i32, value: impl Into<String>, line: usize) -> Self {
        Self {
            code,
            value: value.into(),
            line,
        }
    }

    /// Returns `true` when this token is the structural marker `0/<marker>`.
    ///
    /// Section, table and entity boundaries are all encoded as group code 0
    /// whose value is the marker name; this is the single place that
    /// interpretation is defined.
    #[must_use]
    pub fn is_marker(&self, marker: &str) -> bool {
        self.code == CODE_MARKER && self.value == marker
    }
}

/// Group code used for every structural marker and entity type name.
pub const CODE_MARKER: i32 = 0;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn new_keeps_value_verbatim() {
        let t = Token::new(10, "  1.5  ", 7);
        assert_eq!(t.value, "  1.5  ");
        assert_eq!(t.line, 7);
    }

    #[test]
    fn is_marker_requires_code_zero() {
        assert!(Token::new(0, "SECTION", 1).is_marker("SECTION"));
        assert!(!Token::new(2, "SECTION", 1).is_marker("SECTION"));
        assert!(!Token::new(0, "EOF", 1).is_marker("SECTION"));
    }
}
