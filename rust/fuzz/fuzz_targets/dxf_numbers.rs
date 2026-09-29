#![no_main]

use bcad_dxf::numeric;
use bcad_format::RawGroup;
use libfuzzer_sys::fuzz_target;

// Les conversions numériques (NaN, Inf, dépassements, chaînes vides) rendent
// des erreurs, jamais de panic — y compris sur des codes de groupe absurdes.
fuzz_target!(|data: &[u8]| {
    let text = String::from_utf8_lossy(data);
    let mut parts = text.split('\n');
    let code: i32 = parts
        .next()
        .and_then(|s| s.trim().parse().ok())
        .unwrap_or(0);
    let value = parts.next().unwrap_or("").to_string();
    let group = RawGroup {
        code,
        value,
        line: 1,
    };
    let _ = numeric::f64_field(&group, "FUZZ");
    let _ = numeric::i32_field(&group, "FUZZ");
    let _ = numeric::integerish_field(&group, "FUZZ");
});
