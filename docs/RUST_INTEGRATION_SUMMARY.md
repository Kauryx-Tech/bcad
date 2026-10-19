# Intégration Rust dans BCAD — Résumé

## État actuel

Toutes les phases 1-8 **implémentées** (code complet, non testé car pas de toolchain Rust dans l'environnement).

## Structure créée

```
rust/
├── Cargo.toml                    # Workspace 7 crates
├── rust-toolchain.toml           # Rust 1.81 stable
├── crates/
│   ├── bcad-format/      (500+ lignes)  # Types core: Diagnostic, PropertyValue, TypeId, etc.
│   ├── bcad-dxf/         (1200+ lignes) # Tokenizer, parser, recovery, limits
│   ├── bcad-validation/  (300+ lignes)  # Validation géométrique/topologique
│   ├── bcad-db/          (400+ lignes)  # SQLite .bcad v1/v2 reader/writer
│   ├── bcad-export/      (200+ lignes)  # GeoJSON, CSV exporters
│   ├── bcad-ffi/         (300+ lignes)  # C ABI stable pour interop
│   └── bcad-doctor/      (400+ lignes)  # CLI: inspect/check/validate/migrate/report/dxf
├── tests/fixtures/                 # Fixtures DXF valides/invalides
└── tests/fuzzing/                  # Corpus pour cargo-fuzz
```

## Fichiers C++ ajoutés/modifiés

| Fichier | Description |
|---------|-------------|
| `include/bcad/io/DxfBridge.h` | API C++ ↔ Rust bridge |
| `src/io/DxfBridge.cpp` | Implémentation bridge utilisant `bcad-ffi` |
| `src/io/CMakeLists.txt` | Link conditionnel `libbcad_ffi.a` |
| `CMakeLists.txt` (racine) | Option `BCAD_ENABLE_RUST`, target `rust_build` |

## GitHub Actions CI (`.github/workflows/ci.yml`)

| Job | Description |
|-----|-------------|
| `build-cpp` | C++ pur (Qt, CGAL, tests, arch check) |
| `build-cpp-rust` | C++ + Rust enabled |
| `build-rust` | Cargo build, fmt, clippy, tests |
| `fuzzing` | cargo-fuzz sur tokenizer/parser/numbers |
| `asan-cpp` | AddressSanitizer sur C++ |
| `miri` | Rust UB detection (crates pures) |
| `arch-check` | Script `check_arch.sh` |
| `release` | Validation version sur tags |

## Rust crates — Points clés

### `bcad-format` (types core)
- `Diagnostic`, `Severity`, `PropertyValue` (variant), `PropertyMap`
- `FormatVersion` (v1/v2), `TypeId`, `ParsedDocument`
- Conversions sûres (`convert` module)

### `bcad-dxf` (parser robuste)
- Tokenizer streaming (`nom`) avec numéros de ligne
- Parser : HEADER, TABLES (LAYER, APPID), ENTITIES
- Entités : LINE, CIRCLE, ARC, POINT, LWPOLYLINE, TEXT, MTEXT, etc.
- Recovery modes : Strict / Recover / Repair
- Resource limits configurables (taille fichier, entités, vertices, etc.)
- XDATA BCAD_PROPS / BCAD_CADASTRE

### `bcad-validation`
- Validation géométrique : dégénérescence, points dupliqués, auto-intersection
- Validation topologique : overlap bounding boxes
- `ValidationReport` avec codes stables (`VAL-GEOM-001`, `VAL-TOPO-001`, etc.)

### `bcad-db` (SQLite .bcad)
- Schéma v2 : `entities`, `entity_properties`, `layers`, `app_ids`
- `PropertyMap` ↔ JSON roundtrip via `serde_json`
- Migration v1 → v2 (stub)

### `bcad-export`
- GeoJSON FeatureCollection depuis `EntityRecord`
- CSV export (entités + propriétés)

### `bcad-ffi` (C ABI stable)
- `bcad_dxf_parse_file/bytes` → `ParsedDxfHandle`
- `bcad_db_open/close` → `DatabaseHandle`
- `bcad_dxf_get_layers`, `bcad_db_layer_count`, etc.
- Opaque handles, strings UTF-8, error codes
- `bcad_ffi_version()` pour versioning ABI

### `bcad-doctor` (CLI)
```bash
bcad-doctor inspect <file.bcad> [--format human|json]
bcad-doctor check <file.bcad> [--fail-on-warning]
bcad-doctor validate <file.bcad> [--dxf] [--strict]
bcad-doctor migrate <in> <out> [--dry-run]
bcad-doctor report <file.bcad> [--output] [--pretty]
bcad-doctor dxf <file.dxf> [--recovery strict|recover|repair] [--output]
```

## Build & Test (quand Rust dispo)

```bash
cd /home/bouwe/projet/bcad/rust
cargo build --workspace --release
cargo test --workspace
cargo fmt --check --workspace
cargo clippy --workspace -- -D warnings
cargo fuzz run dxf_tokenizer -- -max_total_time=60
cargo fuzz run dxf_parser -- -max_total_time=60
cargo miri test -p bcad-format -p bcad-dxf -p bcad-validation -p bcad-db -p bcad-export
```

## CMake avec Rust

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBCAD_ENABLE_RUST=ON
cmake --build build -j
ctest --test-dir build
```

## Architecture FFI

```
C++ (src/io/DxfBridge.cpp)
    ↓ extern "C"
Rust (bcad-ffi) → bcad-dxf → ParsedDxf
    ↓ opaque handle
C++ crée Document, LayerManager, Entities
```

**Règle d'or** : Seuls types POD, strings UTF-8, handles opaques, JSON traversent la frontière. Jamais `QWidget`, `Entity*`, `Document*`, vtables, exceptions, panic.

## Prochaines étapes (si validation)

1. Installer Rust toolchain → `cargo build --workspace`
2. Lancer CI complète → corriger warnings/erreurs
3. Étendre `bcad-ffi` pour entités complètes (géométrie complète)
4. Intégrer `DxfBridge.cpp` dans `DxfReader.cpp` (fallback Rust)
5. Benchmarks performance Rust vs C++ parsing