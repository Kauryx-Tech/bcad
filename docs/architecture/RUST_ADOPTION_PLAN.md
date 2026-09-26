# Plan d'adoption Rust — BCAD

> Document généré après audit du codebase (Phase 0). Ce plan ne s'exécute pas avant validation.

---

## 1. Cartographie des composants actuels

| Module / Fichier | Responsabilité | Dépendances clés |
|------------------|----------------|------------------|
| **src/io/DxfReader.cpp** | Lecture DXF → entités C++ | `bcad/geometry/*`, `bcad/properties/PropertyMap`, `bcad/core/Document` |
| **src/io/DxfWriter.cpp** | Écriture DXF depuis entités C++ | `bcad/geometry/Entity`, `bcad/properties/PropertyMap` |
| **src/io/Database.cpp** | Persistance SQLite (.bcad v2) | `sqlite3`, `bcad/geometry/*`, `bcad/properties/*`, `bcad/serialization/Serializer` |
| **src/io/Exchange.cpp** | Export GeoJSON | `bcad/geometry/*`, `bcad/properties/PropertyMap` |
| **src/io/Exporters.cpp** | Exports CSV, etc. | `bcad/geometry/*`, `bcad/properties/*` |
| **src/io/ValueJson.h / JsonText.h** | Utilitaires JSON | Header-only, pas de Qt |
| **src/layout/PdfExport.cpp** | Export PDF vectoriel | `QPainter`, `QPrinter`, `QFont`, `QRectF`, `QPageLayout`, `bcad/layout/*`, `bcad/core/Document` |
| **src/core/Document.cpp** | Modèle en mémoire, index spatial | `bcad/index/ISpatialIndex`, `bcad/layers/LayerManager`, `bcad/events/EventBus` |
| **src/geometry/*** | Types géométriques de base | Header-only (CGAL isolé dans `detail/`) |
| **src/properties/PropertyMap.cpp** | Map de propriétés typées | `bcad/events/EventBus`, `bcad/properties/PropertyTypes` |
| **src/events/EventBus.h** | Bus d'événements typé (header-only) | Template, pas de Qt |
| **src/commands/ConcreteCommands.h** | Commandes pures C++ | `bcad/core/Document`, `bcad/geometry/*` |
| **src/plugin/PluginManager.cpp** | Chargement plugins (dlopen) | `dlopen`, `bcad/plugin/PluginRegistry` |
| **examples/cadastre_proof/plugin/cadastre_plugin.cpp** | Plugin métier cadastre | `bcad/plugin/PluginRegistry`, `bcad/geometry/*`, `bcad/commands/*` |
| **src/app/Viewport.cpp / MainWindow.cpp** | UI Qt (MainWindow, Viewport, ruban, palettes) | Qt6 Widgets, OpenGL, `bcad/app/*` |

---

## 2. Classification A / B / C / D

### A. Reste en C++ (Qt / UI / Core ABI / Plugin ABI)

| Composant | Justification |
|-----------|---------------|
| `src/app/MainWindow.cpp`, `src/app/Viewport.cpp` | UI Qt complète : MainWindow, ruban, palettes, Viewport OpenGL, interactions souris/clavier, QUndoStack |
| `src/app/PropertiesPanel.cpp` | Panneau Qt générique (QWidget, QFormLayout, QLineEdit, QComboBox, QSpinBox) |
| `src/app/QtCommandAdapter.h/.cpp` | Adaptateur QUndoCommand ↔ Command Core |
| `src/app/RibbonBar.cpp`, `src/app/LayerPanel.cpp` | Widgets Qt spécialisés |
| `src/layout/PdfExport.cpp` | Export PDF vectoriel : utilise `QPainter`, `QPrinter`, `QFont`, `QPageLayout`, `QPageSize` — API Qt d'impression |
| `src/core/Document.cpp` / `Document.h` | Modèle central, possède `Entity*`, `LayerManager`, `ISpatialIndex` — ABI stable pour plugins |
| `src/geometry/Entity.h` + classes concrètes | Hiérarchie `Entity` avec vtables, `clone()`, `tessellate()` — ABI plugin |
| `src/properties/PropertyMap.h/.cpp` | `PropertyMap` copiable, `Property` virtuel — utilisé par Core et plugins |
| `src/events/EventBus.h` | Bus d'événements header-only, types `EntityAdded`, `PropertyChanged`, etc. |
| `src/commands/Command.h` + `ConcreteCommands.h` | Interface `Command` pure C++, `Transaction`, `CommandStack` |
| `src/plugin/PluginManager.cpp` + `PluginRegistry.h` | `dlopen` / `LoadLibrary`, `bcad_plugin_init(PluginRegistry&)` — ABI ADR-005 |
| `src/registry/EntityRegistry.h/.cpp` | Enregistrement types d'entités par `TypeId` |
| `src/serialization/Serializer.h/.cpp` | `SerializerRegistry` pour plugins |
| `src/app/TessellationWorker.cpp` | Thread de tessellation (Qt `QThread`, `QTimer`) |

### B. Candidat Rust immédiat (parsing, validation, batch, CLI, data-only)

| Composant | Justification / Plan |
|-----------|----------------------|
| **DXF Parsing** (`src/io/DxfReader.cpp`) | Parsing pur texte → structure neutre. Aucun Qt, aucune Entity C++ créée directement. Devient `bcad-dxf` crate. |
| **DXF Writing** (`src/io/DxfWriter.cpp`) | Sérialisation pure depuis structures neutres. Devient partie de `bcad-dxf`. |
| **SQLite Persistence** (`src/io/Database.cpp`) | Lecture/écriture `.bcad` (SQLite). Peut utiliser `rusqlite`. Devient `bcad-db` ou partie de `bcad-format`. |
| **GeoJSON Export** (`src/io/Exchange.cpp`) | Transformation pure données → GeoJSON. Devient partie de `bcad-format` ou `bcad-export`. |
| **CSV Export** (`src/io/Exporters.cpp`) | Écriture CSV pure. Devient partie de `bcad-format`. |
| **Topological Validation** (depuis `cadastre_plugin.cpp`) | Logique géométrique pure (aire, auto-intersection, segments dégénérés, chevauchement). Devient `bcad-validation`. |
| **CLI Tool** (`bcad-doctor`) | Outil autonome : inspect, check, validate, migrate, report — sans Qt. Devient `bcad-doctor` binary. |
| **JSON Utilities** (`ValueJson.h`, `JsonText.h`) | Déjà header-only C++, remplaçables par `serde_json` en Rust. |

### C. Candidat Rust futur (nécessite FFI stable / bridge)

| Composant | Condition / Plan |
|-----------|------------------|
| **PDF Export** (`src/layout/PdfExport.cpp`) | Dépend de `QPainter`, `QPrinter`, `QFont`, `QPageLayout`. Garde C++ pour l'instant. Peut être migré plus tard avec `printpdf` / `pdf-writer` Rust + bridge pour polices/système. |
| **Plugin Cadastre Commands** (`CreateParcel`, `SplitParcel`, etc.) | Logique métier pure, mais enregistrées via `PluginRegistry` C++. Pourraient être réimplémentées en Rust + exposées via FFI `bcad-ffi` plus tard. |
| **Import/Export Round-trip** | Nécessite bridge `ParsedDxf` Rust → `Entity` C++ via adaptateur C++ (Phase 4). |

### D. Ne doit JAMAIS traverser le FFI (types C++ / Qt / ABI)

| Type / Composant | Raison |
|------------------|--------|
| `QWidget`, `QObject`, `QMainWindow`, `QOpenGLWidget` | Modèle objet Qt, méta-objet, parent/child ownership |
| `QUndoStack`, `QUndoCommand` | Pile undo Qt, signaux/slots, ownership Qt |
| `MainWindow`, `Viewport`, `PropertiesPanel`, `RibbonBar`, `LayerPanel` | UI complète Qt, signaux/slots, `QAction`, `QDockWidget` |
| `Document`, `Entity*`, `LayerManager*` | Objets C++ avec vtables, ownership `unique_ptr`/`shared_ptr`, `Document::notifyEntityChanged` publie sur `EventBus` C++ |
| `Entity` vtables (`typeId()`, `clone()`, `tessellate()`, `applyTransform()`) | ABI plugin ADR-005 — plugins C++ attendent ces vtables |
| `PluginRegistry`, `bcad_plugin_init(PluginRegistry&)` | ABI ADR-005 : `dlopen`, factories `EntityFactory`/`CommandFactory` pointeurs de fonction bruts |
| `std::shared_ptr`, `std::unique_ptr` | Ownership C++, deleter spécifique |
| `QVariant`, `QModelIndex`, `QAbstractItemModel` | Modèles Qt |
| `QPainter`, `QPrinter`, `QFont`, `QPageLayout`, `QPageSize` | API d'impression/dessin Qt |
| Exceptions C++ traversant la frontière | Doivent être catchées côté C++ |
| Panic Rust traversant la frontière | Doivent être catchées côté Rust (`catch_unwind`) |

---

## 3. Architecture Rust Workspace (Phase 1)

```
rust/
├── Cargo.toml                    # Workspace root
├── crates/
│   ├── bcad-format/              # Structures neutres, types diagnostic, versions formats, conversions sûres
│   ├── bcad-dxf/                 # Parseur/écrivain DXF robuste, sans Qt, sans C++
│   ├── bcad-validation/          # Validation géométrie/topologie pure, sans UI
│   ├── bcad-db/                  # Persistance SQLite (.bcad), migrations, intégrité
│   ├── bcad-export/              # GeoJSON, CSV, autres formats d'export
│   ├── bcad-ffi/                 # Frontière stable Rust/C++ (C ABI)
│   └── bcad-doctor/              # Outil CLI : inspect, check, validate, migrate, report
├── tests/
│   ├── fixtures/                 # DXF valides/invalides, .bcad, GeoJSON
│   ├── integration/              # Tests C++ ↔ Rust bridge
│   └── fuzzing/                  # Corpus fuzzing DXF, GeoJSON, CSV
└── rust-toolchain.toml           # Version Rust verrouillée (ex: 1.78)
```

### Règles de compilation

- **Toute crate compile sans Qt** : pas de `qt` feature, pas de `moc`, pas de `Q_*` types.
- **Toute crate compile sans headers C++** : pas de `#include "bcad/..."`, pas de `bindgen` dans les crates pures.
- **Aucune crate ne dépend de MainWindow, Viewport, Plugin, Document C++**.
- **`unsafe` interdit** dans `bcad-format`, `bcad-dxf`, `bcad-validation`, `bcad-db`, `bcad-export`, `bcad-doctor`.
- **`unsafe` limité à `bcad-ffi`** si nécessaire, blocs minimaux, documentés, testés.

---

## 4. Phase 2 — Premier livrable : `bcad-doctor`

### Commandes minimales

```bash
bcad-doctor inspect <fichier.bcad>
bcad-doctor check <fichier.bcad>
bcad-doctor validate <fichier.bcad>
bcad-doctor migrate <fichier.bcad> [--output <fichier.bcad>]
bcad-doctor report <fichier.bcad> --json
```

### Fonctions minimales

- Ouvrir `.bcad` SQLite en lecture seule (par défaut)
- Identifier `PRAGMA user_version` (schéma v1/v2)
- Vérifier intégrité SQLite (`PRAGMA integrity_check`)
- Détecter tables/colonnes manquantes (`entities`, `entity_properties`, `layers`, `app_ids`)
- Lire entités et propriétés génériques (sans connaître les clés métier)
- Préserver et signaler `TypeId` de plugins inconnus
- Vérifier références de styles/gabarits
- Détecter champs de cartouche non résolus
- Diagnostics structurés + codes de sortie stables
- Jamais modifier sans commande explicite `migrate`
- Copie de sortie ou transaction atomique pour migration

### Formats de sortie

**Humain :**
```
bcad-doctor check plan.bcad
✓ SQLite integrity OK
✓ Schema version: 2 (supported)
✓ Tables: entities, entity_properties, layers, app_ids
⚠ Entity 42: unknown TypeId "custom.plugin/entity"
✓ 156 entities, 1248 properties
```

**JSON (stable pour CI) :**
```json
{
  "severity": "error",
  "code": "BCAD-SCHEMA-002",
  "message": "Version de schéma non supportée",
  "file": "plan.bcad",
  "context": {
    "foundVersion": 5,
    "maximumSupportedVersion": 2
  }
}
```

---

## 5. Phase 3 — Parseur DXF Rust (`bcad-dxf`)

### Contraintes

- Aucune dépendance Qt, aucune dépendance C++
- Ne crée aucune Entity C++, ne connaît pas cadastre/calques métiers
- Ne panic jamais sur DXF malformé → `Result<ParsedDxf, DxfError>`
- Diagnostics : ligne, groupe DXF, entité, valeur
- Mode strict + mode récupération
- Streaming si possible (tokenizer lazy)
- Protection : fichiers trop gros, lignes trop longues, imbrications profondes, nombres invalides
- Limites configurables (mémoire, profondeur, taille ligne)

### Modèle neutre

```rust
// bcad-format/src/dxf.rs
pub struct ParsedDxf {
    pub header: ParsedHeader,
    pub layers: Vec<ParsedLayer>,
    pub entities: Vec<ParsedEntity>,
    pub app_ids: Vec<String>,
    pub diagnostics: Vec<DxfDiagnostic>,
}

pub struct ParsedEntity {
    pub handle: Option<String>,
    pub layer: String,
    pub entity_type: ParsedEntityType,
    pub properties: HashMap<String, ParsedPropertyValue>,
    // XDATA BCAD_PROPS restauré
}

pub enum ParsedEntityType {
    Point(ParsedPoint),
    Line(ParsedLine),
    Polyline(ParsedPolyline),
    Circle(ParsedCircle),
    Arc(ParsedArc),
    Text(ParsedText),
    Unknown { type_name: String, raw_groups: Vec<Group> },
}
```

### Tests obligatoires

- Fixtures DXF valides (AutoCAD, LibreCAD, exports BCAD)
- Fixtures DXF invalides (groupes manquants, codes invalides, EOF tronqué)
- Fixtures numériques invalides (NaN, Inf, dépassement)
- Tests de non-crash (fuzzing tokenizer + parser)
- Tests de récupération (mode récupération après erreur)
- Tests de limites (fichier 100MB, ligne 100KB, imbrication 10000)
- Fuzzing continu (libfuzzing / cargo-fuzz)

---

## 6. Phase 4 — FFI Rust / C++ (`bcad-ffi`)

### Principe : frontière C ABI stable

**Ne traverse JAMAIS la frontière :**
- Voir section **D** ci-dessus

**Traverse UNIQUEMENT :**
- Chaînes UTF-8 (`*const c_char` + longueur, ou `RustStr` { ptr, len })
- Entiers (`int32_t`, `int64_t`, `uint32_t`, `uint64_t`)
- Flottants (`float`, `double`)
- Booléens (`bool` / `uint8_t`)
- Tableaux / slices (`*const T` + `len`)
- Bytes (`*const u8` + `len`)
- JSON (`*const c_char` UTF-8)
- Structures POD stables (`#[repr(C)]`)
- Handles opaques (`*mut c_void` + fonctions de destruction)

### Stratégie : Bridge C explicite

```rust
// bcad-ffi/src/bridge.rs
#[repr(C)]
pub struct ParsedDxfHandle(*mut c_void);

#[repr(C)]
pub struct DxfParseResult {
    pub handle: ParsedDxfHandle,
    pub error_code: i32,      // 0 = OK
    pub error_message: *const c_char, // NULL si OK
}

#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_parse_from_file(
    path: *const c_char,
    options: *const DxfParseOptions,
) -> DxfParseResult { ... }

#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_parse_from_bytes(
    data: *const u8,
    len: usize,
    options: *const DxfParseOptions,
) -> DxfParseResult { ... }

#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_free(handle: ParsedDxfHandle) { ... }

#[no_mangle]
pub unsafe extern "C" fn bcad_dxf_get_layers(
    handle: ParsedDxfHandle,
    out_layers: *mut *mut ParsedLayer,
    out_count: *mut usize,
) -> i32 { ... }
```

**Règles FFI :**
- Aucune exception C++ ne traverse → `catch(...)` côté C++
- Aucun panic Rust ne traverse → `catch_unwind` côté Rust
- Durée de vie explicite : `bcad_dxf_free` obligatoire
- Thread-safety : `Send + Sync` pour handles, ou documentation `!Send`
- Versioning : `bcad_ffi_version()` retourne `u32` (ex: `0x010000` = 1.0.0)

### Adaptateur C++ (côté `src/io/`)

```cpp
// src/io/DxfBridge.cpp — SEUL endroit où Rust ↔ C++ se rencontrent
#include "bcad_ffi.h"

struct RustParsedDxf { /* wrapper handle + RAII */ };

std::unique_ptr<Document> importDxfFromRust(const std::string& path) {
    RustParsedDxf parsed = bcad_dxf_parse_from_file(path.c_str(), nullptr);
    if (!parsed.ok()) throw DxfError(parsed.error_message());

    Document doc;
    for (auto& layer : parsed.layers()) {
        doc.layerManager().createLayer(layer.name, layer.color);
    }
    for (auto& entity : parsed.entities()) {
        auto e = convertToCppEntity(entity); // C++ uniquement
        doc.addEntity(std::move(e));
    }
    // XDATA BCAD_PROPS → PropertyMap
    return doc;
}
```

---

## 7. Phase 5 — Validation Topologique Rust (`bcad-validation`)

### Contraintes

- Structures géométriques immuables
- Aucune dépendance Qt, aucun accès `Document` C++
- Calculs déterministes
- Résultats structurés (`ValidationReport`)
- Parallélisable (`rayon`)
- Règles cadastrales locales laissées aux plugins
- Noyau Rust = primitives génériques seulement

### API

```rust
// bcad-validation/src/lib.rs
pub struct ValidationReport {
    pub errors: Vec<ValidationIssue>,
    pub warnings: Vec<ValidationIssue>,
}

pub struct ValidationIssue {
    pub severity: Severity,        // Error / Warning / Info
    pub code: &'static str,        // "VAL-GEOM-001", "VAL-TOPO-003"
    pub message: String,
    pub entity_id: Option<u64>,    // ID dans le fichier source
    pub geometry: Option<GeometryRef>, // coordonnées concernées
    pub suggestion: Option<String>,
}

pub fn validate_dxf(parsed: &ParsedDxf, opts: ValidationOptions) -> ValidationReport;
pub fn validate_geometry(entities: &[ParsedEntity], opts: ValidationOptions) -> ValidationReport;
pub fn validate_topology(entities: &[ParsedEntity]) -> ValidationReport;
```

### Règles noyau (génériques)

| Code | Règle |
|------|-------|
| `VAL-GEOM-001` | Contour fermé requis pour polygone |
| `VAL-GEOM-002` | Aire > epsilon (pas colinéaire) |
| `VAL-GEOM-003` | Pas d'auto-intersection |
| `VAL-GEOM-004` | Pas de segments dégénérés (longueur < epsilon) |
| `VAL-GEOM-005` | Pas de points dupliqués consécutifs |
| `VAL-TOPO-001` | Chevauchement polygones |
| `VAL-TOPO-002` | Intersection limites non gérée |
| `VAL-PREC-001` | Précision numérique insuffisante |

---

## 8. Phase 6 — Parseurs GeoJSON / CSV

### GeoJSON (`bcad-export` ou crate dédiée)

- Validation RFC 7946
- Détection colonnes/propriétés manquantes
- Diagnostics structurés (ligne, propriété, valeur)
- Streaming (`serde_json::Deserializer` + `StreamDeserializer`)
- Tailles fichiers contrôlées (limite configurable)
- Conservation propriétés génériques (`properties` object → `PropertyMap`)
- Aucune clé métier cadastrale dans crate générique

### CSV

- Détection encodage (UTF-8, Latin1, etc.)
- Détection délimiteur (`,`, `;`, `\t`)
- Détection colonnes manquantes / en trop
- Types inférés + override possible
- Streaming (`csv::Reader` + `deserialize`)

---

## 9. Phase 7 — Tests de résilience

| Niveau | Outils / Exigences |
|--------|---------------------|
| Unit Rust | `cargo test` — chaque crate |
| Intégration Rust | `cargo test --test integration` — fixtures → `ParsedDxf` → validation |
| Bridge C++ ↔ Rust | `ctest` — `DxfBridge` tests round-trip |
| Import DXF → Document BCAD | `ctest` — `dxf_cadastre_test`, `dxf_roundtrip_test` |
| Round-trip DXF | C++ : `readDxf` → `writeDxf` → `readDxf` == original |
| Non-régression C++ | `ctest` — 45 tests existants passent |
| **Fuzzing tokenizer DXF** | `cargo fuzz run dxf_tokenizer` (libfuzzer) |
| **Fuzzing parser DXF** | `cargo fuzz run dxf_parser` |
| **Fuzzing conversions numériques** | `cargo fuzz run dxf_numbers` |
| Fuzzing GeoJSON/CSV | Quand crates existent |
| AddressSanitizer C++ | `cmake -DCMAKE_CXX_FLAGS="-fsanitize=address"`, `ctest` |
| Miri Rust | `cargo miri test` (quand faisable, pas de FFI) |
| Concurrence | `rayon` parallel validation, stress test multi-thread |

### Critère de succès (non-négociable)

Une entrée externe invalide ne doit **jamais** provoquer :
- ✗ crash
- ✗ panic traversant le FFI
- ✗ exception C++ traversant le FFI
- ✗ corruption mémoire
- ✗ écriture partielle du fichier source
- ✗ perte silencieuse de données
- ✗ blocage de l'UI

---

## 10. Phase 8 — Intégration CMake + CI

### Option CMake

```cmake
# CMakeLists.txt racine
option(BCAD_ENABLE_RUST "Enable Rust components (requires Rust toolchain)" OFF)

if(BCAD_ENABLE_RUST)
    find_program(CARGO cargo REQUIRED)
    add_custom_target(rust_build
        COMMAND ${CARGO} build --workspace --release
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/rust
        COMMENT "Building Rust workspace"
    )
    add_dependencies(bcad_cadastre_plugin rust_build) # si plugin utilise FFI
    # Installation des .so/.dll Rust produits
endif()
```

### Comportement

| `BCAD_ENABLE_RUST` | Comportement |
|--------------------|--------------|
| `ON` | Construit crates nécessaires, lie `bcad-ffi`, active imports Rust |
| `OFF` | Projet C++ compilable seul ; fonctionnalités Rust → erreur explicite ou fallback C++ |

### CI (GitHub Actions / GitLab CI)

```yaml
jobs:
  build-cpp:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure CMake
        run: cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
      - name: Build
        run: cmake --build build -j
      - name: Test C++
        run: ctest --test-dir build --output-on-failure

  build-rust:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable
      - name: Build Rust
        run: cargo build --workspace --release
      - name: Test Rust
        run: cargo test --workspace
      - name: Format check
        run: cargo fmt --check --workspace
      - name: Clippy
        run: cargo clippy --workspace -- -D warnings
      - name: Fuzzing (short)
        run: cargo fuzz run dxf_tokenizer -- -max_total_time=60

  bridge-test:
    needs: [build-cpp, build-rust]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build with Rust
        run: cmake -S . -B build -DBCAD_ENABLE_RUST=ON && cmake --build build -j
      - name: Test Bridge
        run: ctest --test-dir build -R "dxf|bridge" --output-on-failure
```

**Règles CI :**
- `cargo fmt --check` obligatoire
- `cargo clippy -- -D warnings` obligatoire
- Pas de `#[allow(...)]` pour faire passer CI sans justification documentée
- Fuzzing court en CI, complet en nightly/scheduled

---

## 11. Rapport final attendu (à produire à la fin)

| Section | Contenu |
|---------|---------|
| 1. Composants gardés en C++ | Liste + justification |
| 2. Composants introduits en Rust | Liste + justification |
| 3. Contrat FFI exact | Signatures `extern "C"`, structures `#[repr(C)]`, codes erreur |
| 4. Fonctions `unsafe` | Liste + justification par bloc |
| 5. Garanties obtenues | Memory safety parsing, pas de crash sur entrée invalide, fuzzing coverage |
| 6. Limitations C++ restantes | PDF export, UI, plugin ABI, Document/Entity ownership |
| 7. Tests ajoutés | Unit, integration, bridge, fuzzing, ASan/Miri |
| 8. Résultats builds | `cmake --build build` + `cargo build --workspace --release` |
| 9. Résultats fuzzing | Corpus size, crashes found/fixed, coverage |
| 10. Risques déploiement + rollback | Plan si Rust crate casse build C++ |
| 11. Étapes suivantes | Sans élargir périmètre sans accord |

---

## 12. Principes directeurs (rappel)

> **Rust protège les frontières de données et les calculs purs.**
> **C++/Qt garde l'interface, le Document existant et l'écosystème plugin.**
> **Aucun langage ne doit connaître les détails internes de l'autre.**

---

## 13. Prochaines étapes immédiates

1. ✅ Phase 0 : Audit + ce document (`RUST_ADOPTION_PLAN.md`)
2. ⏳ Phase 1 : Créer `rust/` workspace + `Cargo.toml` + crates vides
3. ⏳ Phase 2 : `bcad-doctor` MVP (inspect/check sur `.bcad` v2)
4. ⏳ Phase 3 : `bcad-dxf` tokenizer + parser + tests fuzzing
5. ⏳ Phase 4 : `bcad-ffi` bridge C + adaptateur C++ `DxfBridge.cpp`
6. ⏳ Phase 5 : `bcad-validation` règles noyau
7. ⏳ Phase 6 : GeoJSON/CSV
8. ⏳ Phase 7 : CI + fuzzing + ASan/Miri
9. ⏳ Phase 8 : Intégration CMake + `BCAD_ENABLE_RUST`

---

*Document créé après audit Phase 0. Ne pas exécuter les phases 1+ sans validation.*