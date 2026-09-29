#![no_main]

use bcad_dxf::{tokenize, ParseLimits};
use libfuzzer_sys::fuzz_target;

// Le tokenizer ne doit jamais paniquer, quelle que soit l'octet : une entrée
// externe invalide est une erreur bornée, pas un crash. Le résultat est jeté :
// ce qui compte est l'absence de panic, pas le contenu.
fuzz_target!(|data: &[u8]| {
    let input = String::from_utf8_lossy(data);
    let limits = ParseLimits::default();
    let _ = tokenize(&input, &limits);
});
