#![no_main]

use bcad_dxf::{parse_dxf, ParseLimits, ParseOptions, RecoveryMode};
use libfuzzer_sys::fuzz_target;

// Le parseur ne doit jamais paniquer : mode strict ET mode récupération, sur
// la même entrée. Les limites par défaut bornent mémoire et profondeur, comme
// en production.
fuzz_target!(|data: &[u8]| {
    let input = String::from_utf8_lossy(data);
    for recovery in [RecoveryMode::Strict, RecoveryMode::Recover] {
        let options = ParseOptions {
            recovery,
            limits: ParseLimits::default(),
            strict_encoding: false,
        };
        let _ = parse_dxf(&input, &options);
    }
});
