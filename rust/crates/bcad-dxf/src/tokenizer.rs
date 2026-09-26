//! Turns DXF text into a flat `Vec<Token>`.
//!
//! The tokenizer is the only place that looks at raw text. It does no
//! interpretation: it validates that the stream is a well-formed alternation of
//! integer group codes and value lines, and it records the source line of each
//! group so every later stage can report a location.
//!
//! # Byte-safe iteration
//!
//! DXF files are frequently not UTF-8 (they predate it), and `&str` slicing on a
//! non-`char` boundary panics. This tokenizer therefore never indexes into the
//! input by raw byte offset: it walks forward with `char_indices` and only
//! slices at offsets the iterator itself produced.

use crate::error::{DxfError, DxfResult};
use crate::limits::ParseLimits;
use crate::token::Token;

/// Reads `input` completely into owned group tokens.
///
/// The whole file is tokenized up front. That costs memory proportional to the
/// group count, which is bounded by [`ParseLimits::max_tokens`], and it buys a
/// parser that can look ahead freely and backtrack without re-scanning text.
///
/// # Errors
///
/// Returns [`DxfError::ResourceLimit`] when the input exceeds the byte or group
/// budget, [`DxfError::InvalidGroupCode`] when a group code is not an integer,
/// and [`DxfError::MalformedStream`] when a group code is not followed by a
/// value line.
pub fn tokenize(input: &str, limits: &ParseLimits) -> DxfResult<Vec<Token>> {
    limits.check_file_size(input.len())?;

    let mut tokens = Vec::new();
    let mut iter = Lines::new(input);

    while let Some(code_line) = iter.next_group_code(limits)? {
        let value_line = iter
            .next_value(limits)?
            .ok_or_else(|| DxfError::unexpected_eof(code_line.number))?;

        limits.check_token_count(tokens.len() + 1)?;
        tokens.push(Token::new(code_line.code, value_line, code_line.number));
    }

    Ok(tokens)
}

/// A group code line: an integer, plus the line it was read from.
struct CodeLine {
    code: i32,
    number: usize,
}

/// Byte-safe line reader.
///
/// `str::split_inclusive` yields slices that always end on a `char` boundary, so
/// this reader cannot panic on multi-byte input no matter what the file
/// contains. Empty lines are skipped; the reported number is the 1-based source
/// line, counting every line including the skipped ones.
struct Lines<'a> {
    inner: std::str::SplitInclusive<'a, char>,
    number: usize,
}

impl<'a> Lines<'a> {
    fn new(input: &'a str) -> Self {
        Self {
            inner: input.split_inclusive('\n'),
            number: 0,
        }
    }

    /// Reads the next non-empty line, or `None` at end of input.
    ///
    /// Only *trailing* whitespace is trimmed. Leading whitespace is significant
    /// in a value, and trimming it here would silently alter text content.
    ///
    /// A line that is empty once trailing whitespace is removed is treated as a
    /// separator and skipped. The consequence is deliberate and worth stating:
    /// an empty group *value* cannot be represented, because a blank line and an
    /// empty value are indistinguishable in the source format. Real DXF writers
    /// do not emit empty values, and treating them as separators is what keeps
    /// hand-edited files with blank lines between groups parseable.
    fn next(&mut self) -> Option<(&'a str, usize)> {
        loop {
            let raw = self.inner.next()?;
            self.number += 1;

            // `trim_end` also removes the `\n`, a `\r` from CRLF, and any
            // trailing padding, so "  0  \r\n" yields "  0".
            let line = raw.trim_end_matches(['\n', '\r', ' ', '\t']);
            if !line.is_empty() {
                return Some((line, self.number));
            }
        }
    }

    /// Reads the next line and requires it to be an integer group code.
    ///
    /// Group codes are conventionally right-aligned in their field ("  0",
    /// "999"), so surrounding whitespace is removed before parsing. This is the
    /// one place leading whitespace is discarded.
    fn next_group_code(&mut self, limits: &ParseLimits) -> DxfResult<Option<CodeLine>> {
        let Some((line, number)) = self.next() else {
            return Ok(None);
        };

        limits.check_line_length(line.len())?;

        let code = line.trim().parse::<i32>().map_err(|_| {
            DxfError::malformed_stream(number, format!("expected a group code, found {line:?}"))
        })?;

        Ok(Some(CodeLine { code, number }))
    }

    /// Reads the next line as a raw value, or `None` at end of input.
    fn next_value(&mut self, limits: &ParseLimits) -> DxfResult<Option<String>> {
        let Some((line, _)) = self.next() else {
            return Ok(None);
        };

        limits.check_line_length(line.len())?;
        Ok(Some(line.to_string()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn toks(input: &str) -> Vec<Token> {
        tokenize(input, &ParseLimits::default()).expect("tokenizes")
    }

    #[test]
    fn reads_code_value_pairs() {
        let tokens = toks("  0\nLINE\n  8\n0\n");
        assert_eq!(tokens.len(), 2);
        assert_eq!(tokens[0].code, 0);
        assert_eq!(tokens[0].value, "LINE");
        assert_eq!(tokens[1].code, 8);
        assert_eq!(tokens[1].value, "0");
    }

    #[test]
    fn records_line_of_the_group_code_not_the_value() {
        // The value of group 0 sits on line 2; the group code is on line 1.
        let tokens = toks("  0\nSECTION\n  2\nHEADER\n");
        assert_eq!(tokens[0].line, 1);
        assert_eq!(tokens[1].line, 3);
    }

    #[test]
    fn skips_blank_lines() {
        let tokens = toks("  0\nLINE\n\n\n  8\n0\n");
        assert_eq!(tokens.len(), 2);
        assert_eq!(tokens[1].code, 8);
    }

    #[test]
    fn tolerates_crlf_and_trailing_space() {
        let tokens = toks("  0  \r\nSECTION\r\n  2\r\nHEADER\r\n");
        assert_eq!(tokens[0].value, "SECTION");
        assert_eq!(tokens[1].value, "HEADER");
    }

    #[test]
    fn rejects_non_integer_group_code() {
        let err = tokenize("  ABC\nLINE\n", &ParseLimits::default()).expect_err("must fail");
        assert!(err.to_string().contains("ABC"), "{err}");
    }

    #[test]
    fn rejects_code_without_value() {
        let err = tokenize("  0\n", &ParseLimits::default()).expect_err("must fail");
        assert!(matches!(err, DxfError::UnexpectedEof { .. }), "{err}");
    }

    #[test]
    fn rejects_value_without_code() {
        // A leading bare word is read as a group code and must fail.
        assert!(tokenize("SECTION\n", &ParseLimits::default()).is_err());
    }

    #[test]
    fn empty_input_yields_no_tokens() {
        assert!(toks("").is_empty());
        assert!(toks("\n\n  \n").is_empty());
    }

    #[test]
    fn multi_byte_characters_do_not_panic() {
        // "Café" and a 4-byte emoji exercise char-boundary safety.
        let input = "  1\nCafé \u{1F600}\n  0\nEOF\n";
        let tokens = toks(input);
        assert_eq!(tokens[0].value, "Café \u{1F600}");
        assert_eq!(tokens[1].value, "EOF");
    }

    #[test]
    fn multi_byte_on_the_code_line_does_not_panic() {
        let input = "  é\nvalue\n";
        assert!(tokenize(input, &ParseLimits::default()).is_err());
    }

    #[test]
    fn enforces_token_count_limit() {
        let limits = ParseLimits {
            max_tokens: 2,
            ..ParseLimits::default()
        };
        let err = tokenize("  0\nA\n  1\nB\n  2\nC\n", &limits).expect_err("must fail");
        assert!(err.to_string().contains("group limit"), "{err}");
    }

    #[test]
    fn enforces_file_size_limit() {
        let limits = ParseLimits {
            max_file_size: 2,
            ..ParseLimits::default()
        };
        let err = tokenize("  0\nLINE\n", &limits).expect_err("must fail");
        assert!(err.to_string().contains("byte limit"), "{err}");
    }
}
