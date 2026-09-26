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
    /// A line that is empty once trailing whitespace is removed is skipped. This
    /// is only ever used when looking for a *group code*, where a blank line can
    /// only be a separator: that tolerance is what keeps hand-edited files with
    /// blank lines between groups parseable. It is never used to look for a
    /// value — see [`Lines::next_value`], where skipping would desynchronise the
    /// whole stream.
    fn next(&mut self) -> Option<(&'a str, usize)> {
        loop {
            let Some((raw, number)) = self.next_verbatim() else {
                return None;
            };
            if !raw.is_empty() {
                return Some((raw, number));
            }
        }
    }

    /// Reads the next line exactly as written, blank lines included.
    fn next_verbatim(&mut self) -> Option<(&'a str, usize)> {
        let raw = self.inner.next()?;
        self.number += 1;

        // `trim_end` also removes the `\n`, a `\r` from CRLF, and any
        // trailing padding, so "  0  \r\n" yields "  0".
        Some((raw.trim_end_matches(['\n', '\r', ' ', '\t']), self.number))
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

    /// Reads the line following a group code as that code's value.
    ///
    /// The line is taken verbatim, **including a blank one**. The DXF format is
    /// a strict alternation of code line and value line: every code is followed
    /// by exactly one value, and an empty value is a legal empty string. A file
    /// may put blank lines *between* groups, and that tolerance lives in
    /// [`Lines::next`]; it must not apply here.
    ///
    /// Skipping a blank at this point used to shift every following pair by one,
    /// so an entity with an empty group (say `8` with no layer name) reported
    /// `expected a group code, found "0.0"` several lines later, naming neither
    /// the offending code nor its position. Consuming exactly one line per code
    /// makes desynchronisation impossible.
    fn next_value(&mut self, limits: &ParseLimits) -> DxfResult<Option<String>> {
        let Some((line, _)) = self.next_verbatim() else {
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

    /// An empty group value is a legal empty string, not a separator.
    #[test]
    fn an_empty_group_value_is_an_empty_string() {
        let tokens = toks("  8\n\n 10\n0.0\n");
        assert_eq!(tokens.len(), 2);
        assert_eq!(tokens[0].code, 8);
        assert_eq!(tokens[0].value, "", "a blank value line is a value");
        assert_eq!(tokens[1].code, 10, "the pair after it stays aligned");
        assert_eq!(tokens[1].value, "0.0");
    }

    /// Regression: an empty value used to consume the *next* group code, shifting
    /// every following pair by one and reporting a bogus error far from the cause.
    #[test]
    fn an_empty_value_does_not_desynchronise_the_stream() {
        let tokens = toks("  0\nLINE\n  8\n\n 10\n0.0\n 20\n0.0\n");
        let pairs: Vec<(i32, &str)> = tokens.iter().map(|t| (t.code, t.value.as_str())).collect();
        assert_eq!(
            pairs,
            vec![(0, "LINE"), (8, ""), (10, "0.0"), (20, "0.0")],
            "every code must keep its own value"
        );
    }

    /// The tolerance is about blank lines *between* groups and must not become a
    /// licence to pair a code with a later line.
    #[test]
    fn a_code_with_no_value_at_all_is_rejected() {
        // A trailing code with nothing after it: the value is missing, not blank.
        let err = tokenize("  0\nLINE\n  8\n", &ParseLimits::default()).unwrap_err();
        assert!(
            matches!(err, DxfError::UnexpectedEof { line: 3 }),
            "the report must name the dangling code: {err:?}"
        );
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
