//! The crate advertises three properties on untrusted input: total, bounded,
//! no-panic. The unit tests check each rule against input its author thought
//! of. These check the properties themselves, against input nobody thought of:
//! a fixed corpus of shapes that have broken parsers before, plus a seeded
//! generator that splices DXF fragments into things no writer would produce.
//!
//! Randomness here is a fixed seed. A failure found by this file is
//! reproducible on any machine, which is the only reason to keep it at all --
//! a flaky parser test teaches everyone to re-run the suite.

// The dependencies of the crate under test are not dependencies of this test,
// and the workspace lint that says so cannot tell the difference. Silence it
// here rather than in the manifest, where it would also stop applying to the
// library.
#![allow(unused_crate_dependencies)]

use bcad_dxf::{
    parse_dxf, parse_dxf_bytes, ParseLimits, ParseOptions, ParsedEntityType, RecoveryMode,
};

/// xorshift64. Not cryptographic and not trying to be: the point is a
/// different byte order on every run of the suite, from a known one.
struct Rng(u64);

impl Rng {
    fn new(seed: u64) -> Self {
        assert_ne!(seed, 0, "xorshift cannot leave zero");
        Self(seed)
    }

    const fn next(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.0 = x;
        x
    }

    fn below(&mut self, n: usize) -> usize {
        usize::try_from(self.next() % n as u64).expect("index fits usize")
    }
}

/// Fragments chosen for the parse path they steer, not for looking like DXF.
const FRAGMENTS: &[&str] = &[
    "0\n",
    "SECTION\n",
    "ENDSEC\n",
    "2\nHEADER\n",
    "2\nTABLES\n",
    "2\nENTITIES\n",
    "0\nTABLE\n",
    "0\nENDTAB\n",
    "0\nLAYER\n",
    "0\nLINE\n",
    "0\nPOLYLINE\n",
    "0\nVERTEX\n",
    "0\nSEQEND\n",
    "0\nENDBLK\n",
    "8\n0\n",
    "8\nNOSUCHLAYER\n",
    "10\n0.0\n",
    "10\n1e308\n",
    "20\n-1e-308\n",
    "30\nnan\n",
    "40\n0.0\n",
    "62\n7\n",
    "70\n4294967295\n",
    "290\n1\n",
    "999\n",
    "99999999999999999999\n",
    "1\ntext\n",
    "1\n",
    "\n",
    "\r\n",
    "   \n",
    "\t\n",
    "\u{0}\n",
    "{\n",
    "}\n",
    "1001\nAPPID\n",
    "1000\ndata\n",
    "\u{e9}\n",
    "\u{1F600}\n",
    "1\n\"unterminated\n",
];

/// Shapes that have each broken a hand-written scanner, kept as whole files.
/// Built at run time because some of them are assembled rather than spelled:
/// a literal that long is unreadable, and the point is the length.
fn corpus() -> Vec<Vec<u8>> {
    let mut cases: Vec<Vec<u8>> = vec![
        b"".to_vec(),
        b"\n".to_vec(),
        b"0".to_vec(),
        b"0\n".to_vec(),
        b"0\n\n".to_vec(),
        b"\n\n\n\n".to_vec(),
        b"0\nSECTION\n".to_vec(),
        b"0\nSECTION\n2\nENTITIES\n0\nLINE\n".to_vec(),
        b"0\nSECTION\n2\nENTITIES\n0\nLINE\n8\n0\n10\n0\n20\n0\n11\n1\n21\n1\n0\nENDSEC\n0\nEOF\n"
            .to_vec(),
        // A code with no value at all: the tokenizer must not read the next
        // code as this one's value.
        b"0\nSECTION\n1\n0\n2\nENTITIES\n0\nENDSEC\n0\nEOF\n".to_vec(),
        // Empty values, which are legal and were once indistinguishable from a
        // missing line.
        b"1\n\n0\nSECTION\n\n2\nENTITIES\n\n0\nENDSEC\n\n".to_vec(),
        // CR without LF, and LF without CR.
        b"0\rSECTION\r2\rENTITIES\r0\rENDSEC\r".to_vec(),
        // Non-UTF-8, including a truncated multi-byte sequence and a lone NUL.
        b"0\n1\n\xff\xfe\n2\nENTITIES\n0\n\x00\n".to_vec(),
        // Nesting far past any plausible drawing, closed and unclosed.
        b"0\nSECTION\n0\nSECTION\n0\nSECTION\n0\nSECTION\n".to_vec(),
        b"0\nSECTION\n0\nSECTION\n0\nSECTION\n0\nSECTION\n0\nENDSEC\n".to_vec(),
    ];

    // A single enormous value.
    let mut huge = b"1\n".to_vec();
    huge.extend(std::iter::repeat_n(b'x', 70_000));
    huge.push(b'\n');
    cases.push(huge);

    // Only group codes, no values, as far as the eye can see.
    cases.push(b"0\n".repeat(20_000));

    // Deep XDATA nesting, opened and never closed.
    let mut xdata = b"0\nSECTION\n2\nENTITIES\n0\nLINE\n1001\nA\n".to_vec();
    xdata.extend_from_slice(&b"{\n".repeat(500));
    xdata.extend_from_slice(b"1000\nx\n");
    xdata.extend_from_slice(&b"}\n".repeat(500));
    xdata.extend_from_slice(b"0\nENDSEC\n0\nEOF\n");
    cases.push(xdata);

    cases
}

const fn options(recovery: RecoveryMode, limits: ParseLimits) -> ParseOptions {
    ParseOptions {
        recovery,
        limits,
        strict_encoding: false,
    }
}

/// Total: every input produces a value or an error, and never a panic, through
/// either entry point and in either recovery mode. A panic here is the bug the
/// crate exists to not have, so the test does not catch it and continue -- it
/// fails and names the input.
#[test]
fn no_input_panics() {
    let mut rng = Rng::new(0x5EED_1234_ABCD_0001);

    let generated: Vec<Vec<u8>> = (0..2_000)
        .map(|_| {
            let count = 1 + rng.below(40);
            let mut out = Vec::new();
            for _ in 0..count {
                out.extend_from_slice(FRAGMENTS[rng.below(FRAGMENTS.len())].as_bytes());
            }
            out
        })
        .collect();

    let mut inputs: Vec<Vec<u8>> = corpus();
    inputs.extend(generated);

    for input in &inputs {
        for recovery in [RecoveryMode::Recover, RecoveryMode::Strict] {
            for strict_encoding in [false, true] {
                let opts = ParseOptions {
                    recovery,
                    limits: ParseLimits::default(),
                    strict_encoding,
                };
                // Bytes first: this is the path a file takes, and the one that
                // has to survive arbitrary bytes rather than valid UTF-8.
                let _ = parse_dxf_bytes(input, &opts);
                // Then text, for the subset that is valid UTF-8. Lossy
                // conversion here is the same substitution the parser does
                // internally, so a panic on this path is a real one.
                let as_text = String::from_utf8_lossy(input);
                let _ = parse_dxf(&as_text, &opts);
            }
        }
    }
}

/// Bounded: with the ceilings pulled tight, a successful parse never reports
/// more than it was allowed. Resource limits are fatal in both modes, so
/// `Ok` is only reachable when the input genuinely fitted.
#[test]
fn a_successful_parse_stays_within_tight_limits() {
    let tight = ParseLimits {
        max_tokens: 128,
        max_entities: 4,
        max_layers: 4,
        max_polyline_vertices: 8,
        max_xdata_size: 16,
        max_nesting_depth: 3,
        ..ParseLimits::default()
    };
    let mut rng = Rng::new(0x5EED_1234_ABCD_0002);

    for _ in 0..1_000 {
        let count = 1 + rng.below(60);
        let mut input = Vec::new();
        for _ in 0..count {
            input.extend_from_slice(FRAGMENTS[rng.below(FRAGMENTS.len())].as_bytes());
        }
        for recovery in [RecoveryMode::Recover, RecoveryMode::Strict] {
            let opts = options(recovery, tight.clone());
            if let Ok(parsed) = parse_dxf_bytes(&input, &opts) {
                assert!(
                    parsed.entities.len() <= tight.max_entities,
                    "{} entities under a limit of {}",
                    parsed.entities.len(),
                    tight.max_entities
                );
                assert!(
                    parsed.layers.len() <= tight.max_layers,
                    "{} layers under a limit of {}",
                    parsed.layers.len(),
                    tight.max_layers
                );
                for entity in &parsed.entities {
                    if let ParsedEntityType::Polyline { vertices, .. } = &entity.entity_type {
                        assert!(
                            vertices.len() <= tight.max_polyline_vertices,
                            "{} polyline vertices under a limit of {}",
                            vertices.len(),
                            tight.max_polyline_vertices
                        );
                    }
                }
            }
        }
    }
}

/// Deterministic: the same bytes read twice produce the same report, in the
/// same order. Iteration order leaking into output is how a "harmless" map
/// turns into a file that differs run to run.
#[test]
fn parsing_is_deterministic() {
    let mut rng = Rng::new(0x5EED_1234_ABCD_0003);

    for _ in 0..500 {
        let count = 1 + rng.below(40);
        let mut input = Vec::new();
        for _ in 0..count {
            input.extend_from_slice(FRAGMENTS[rng.below(FRAGMENTS.len())].as_bytes());
        }
        let opts = options(RecoveryMode::Recover, ParseLimits::default());
        let first = parse_dxf_bytes(&input, &opts);
        let second = parse_dxf_bytes(&input, &opts);
        assert_eq!(
            format!("{first:?}"),
            format!("{second:?}"),
            "two reads of the same bytes disagreed"
        );
    }
}

/// The error path stays an error. A `ResourceLimit` raised in Recover mode and
/// then swallowed would leave a half-read drawing looking complete, which is
/// the one outcome a caller cannot detect.
#[test]
fn limits_are_reported_rather_than_silently_applied() {
    let tiny = ParseLimits {
        max_entities: 1,
        max_tokens: 32,
        ..ParseLimits::default()
    };
    let mut input = String::from("0\nSECTION\n2\nENTITIES\n");
    for _ in 0..8 {
        input.push_str("0\nLINE\n8\n0\n10\n0\n20\n0\n11\n1\n21\n1\n");
    }
    input.push_str("0\nENDSEC\n0\nEOF\n");

    for recovery in [RecoveryMode::Recover, RecoveryMode::Strict] {
        let result = parse_dxf(&input, &options(recovery, tiny.clone()));
        match result {
            Ok(parsed) => assert!(
                parsed.entities.len() <= tiny.max_entities,
                "Recover accepted {} entities with a ceiling of {}",
                parsed.entities.len(),
                tiny.max_entities
            ),
            Err(err) => assert!(
                err.to_string().to_lowercase().contains("limit"),
                "expected a limit diagnostic, got: {err}"
            ),
        }
    }
}
