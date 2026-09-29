# État d'avancement — intégration Rust

> **Fichier vivant** : chaque tâche menée par un agent met à jour cette feuille
> (voir `AGENTS.md`, workflow). Les références `#<sha>` pointent les commits.

Le projet comporte sept crates Rust sous `rust/`. Le pont entre eux et le code
C++ n'existait pas encore : c'est l'objet de ce document.

## 1. Gates

Les quatre gates se lancent **un par un**. Une chaîne `&& … || echo OK` a déjà
masqué un échec de clippy sur un commit déjà poussé (`0794ac6` → `a25525b`) : ne
plus jamais les enchaîner.

| Gate | Commande (depuis `rust/`) |
|------|---------------------------|
| Format | `cargo fmt --all --check` |
| Compilation | `cargo check --workspace --all-targets` |
| Lints | `cargo clippy --workspace --all-targets --all-features -- -D warnings -A clippy::cargo` |
| Tests | `cargo test --workspace --all-features` |

Volumétrie au dernier passage : **162 tests verts**.

| Crate | Tests |
|-------|-------|
| `bcad-dxf` | 123 |
| `bcad-ffi` | 17 |
| `bcad-format` | 8 |
| `bcad-validation` | 8 |
| `bcad-db` | 4 |
| `bcad-export` | 1 |
| `bcad-doctor` | 1 |

## 2. Crates

| Crate | Rôle | Statut |
|-------|------|--------|
| `bcad-format` | Types neutres partagés, `PropertyMap`, géométrie canonique 3D | FAIT |
| `bcad-dxf` | Lecteur DXF total, borné, sans panique | FAIT |
| `bcad-validation` | Règles de validation géométrique | FAIT |
| `bcad-db` | Accès SQLite | FAIT |
| `bcad-export` | Export de formats | FAIT |
| `bcad-ffi` | ABI C pour l'appel depuis C++ | FAIT |
| `bcad-doctor` | Diagnostic d'installation | FAIT |

Le `Cargo.toml` racine ne listait que cinq crates sur sept et pointait vers des
chemins inexistants : les crates ne compilaient pas du tout. Corrigé, `10c69a4`.
La licence des crates suit celle de BCAD, GPL-3.0-or-later : `b9f7092`.

## 3. Pont C++ ↔ Rust (ADR-006)

`a25525b`. Le pont ne pouvait pas compiler : `DxfBridge.cpp` inclue
`<bcad_ffi.h>`, fichier absent de l'arbre.

| Tâche | Statut | Trace |
|-------|--------|-------|
| `include/bcad_ffi.h` énonçant l'ABI réelle (19 points d'entrée, 6 types `repr(C)`) + `static_assert` de tailles | FAIT | `a25525b` |
| Test de dérive header ↔ crate : déclaration manquante, fonction fantôme, code erreur renuméroté | FAIT | `a25525b` |
| `DxfBridge.h` purgé de `BcColor`, `BcPropertyValue` et `BcErrorCode`-en-`struct`, qui n'ont jamais existé côté Rust | FAIT | `a25525b` |
| Double free : les `_free` libèrent déjà les chaînes du tableau | FAIT | `a25525b` |
| `libbcad_ffi.a` lié dans une cible **non exportée** : le SDK installé est identique que Rust soit ON ou OFF | FAIT | `a25525b` |
| `bcad_ffi.h` non installé ; aucune référence Cargo dans `BCADTargets.cmake` | FAIT | `a25525b` |
| `sdk_proof` se configure, compile et lie contre le préfixe installé avec `BCAD_ENABLE_RUST=ON` | FAIT | `a25525b` |

## 4. Défauts corrigés

| Défaut | Effet | Trace |
|--------|-------|-------|
| Groupe DXF 290 lu comme un booléen : `visible = v != 0` | Tout calque masqué était dessiné | `c1fc23f` |
| Fixture `a_hidden_layer_is_marked_invisible` en `290\n0` | Le test contredisait son propre nom, et validait le bug | `c1fc23f` |
| `$LIMMIN`/`$LIMMAX` 2D refusés | En-tête Autodesk minimal rejeté | `0794ac6` |
| `ResourceLimit` non fatal en `Recover` | Un dessin tronqué passait pour complet | `0794ac6` |
| Tokenizer consommant la ligne suivante comme valeur | Désynchronisation du flux sur valeur vide | `0794ac6` |
| Clippy `question_mark` sur du code déjà poussé | Gate masqué par un `&& … || echo OK` | `a25525b` |

Les propriétés « total, borné, déterministe » que le crate revendique dans sa
description n'étaient vérifiées que sur des cas pensés par l'auteur du test.
Elles le sont désormais contre un corpus de formes qui cassent les scanners
manuscrits et un générateur à graine fixe : `1b6f509`. Ce n'est pas un
remplaçant du fuzzing, et ne dispense pas des cibles de fuzz de la §7.

## 5. Décision d'architecture à arbitrer

**Ne pas étendre l'ABI à la géométrie.** Le lecteur C++ natif importe déjà la
géométrie et il est couvert par les tests de round-trip. Le faire aussi en Rust
imposerait deux importateurs à garder synchronisés, une surface `repr(C)` bien
plus large, et aucun gain visible.

Répartition proposée : Rust pour les limites, les diagnostics et la validation ;
C++ pour la géométrie. Conséquence assumée : l'ABI ne transporte que des
métadonnées — calques, résumés d'entités, diagnostics — d'où les noms
`readDxfMetadata*` et la séparation entre `parsed_entity_count` et ce qui
atterrit réellement dans le `Document`.

## 6. Reste à faire

| Tâche | Blocage | Décideur |
|-------|---------|----------|
| Créer les trois cibles `dxf_tokenizer`, `dxf_parser`, `dxf_numbers` | `cargo fuzz` exige nightly ; non installé. `rustup` est présent (`~/.cargo/bin/rustup`), l'installation est possible | Mainteneur |
| Lancer réellement ces cibles au moins une fois | Idem | Mainteneur |
| Corriger le job `fuzzing` de la CI : il installe `stable` et appelle `cargo fuzz`, qui ne peut pas fonctionner, et nomme trois cibles inexistantes | Fichier non commité, travail du mainteneur | Mainteneur |
| `scripts/check_no_panic.sh` référencé par les métadonnées Cargo | Fichier absent | Mainteneur |
| Protection de `main` sur GitHub | `scripts/setup-branch-protection.sh` jamais exécuté | Mainteneur |
| Brancher le pont sur l'UI (lecture d'un DXF, affichage du rapport) | Décision §5 non prise | Mainteneur |

## 7. Blocage courant : le WIP application ne compile pas

Ces fichiers sont modifiés **dans l'arbre de travail, non commités**, et
n'appartiennent pas aux travaux Rust :

```
include/bcad/properties/PropertyMap.h
src/app/PropertiesPanel.cpp
src/app/Commands.{h,cpp}
src/core/Document.cpp
include/bcad/core/Document.h
include/bcad/commands/ConcreteCommands.h
include/bcad/plugin/PluginRegistry.h
src/properties/PropertyMap.cpp
docs/{COMMAND_PATTERN,PROPERTY_SYSTEM}.md
docs/interface_amelioration.txt
AGENTS.md
.github/workflows/ci.yml
RUST_INTEGRATION_SUMMARY.md   (non suivi)
```

`PropertiesPanel.cpp` ne compile pas :

- `QDoubleSpinBox` et `QSpinBox` non déclarés (includes Qt manquants) ;
- `Property::hasRange()` inexistant, `min_` et `max_` inaccessibles.

Conséquences mesurées, toutes **`pre_existantes` et sans rapport avec Rust** :

- la cible `bcad` ne se construit pas ;
- `ctest` plafonne à **44/46** : `sdk_external_test` et `cadastre_external_test`
  échouent tous deux à l'étape d'installation, qui exige le binaire `bcad`
  absent ;
- `cmake --install` s'arrête sur le même fichier.

Le SDK s'installe et `sdk_proof` se lie si l'on saute la seule cible `bcad`,
ce qui isole le défaut.

## 8. Vérifier avant de croire un vert

`ctest` affiche 96 % et passe pour un motif : deux échecs sont réels, mais la
cause est dans des fichiers que la tâche en cours ne touche pas. Vérifier que
les fichiers en cause sont disjoints du jeu de modifications avant de conclure
qu'une régression est préexistante — c'est ce qui a été fait ici, par
`git status --porcelain` sur les deux côtés.

De même, `cargo test` peut être vert et clippy rouge : ce sont deux commandes
indépendantes, pas deux facons de lire un même verdict.

## 9. Alignement v3 réel + gates verts (2026-09-29)

Constat d'audit : `bcad-doctor` ne savait lire aucun vrai fichier (schéma
inventé `handle`/`props_json`, `MAX_SUPPORTED = V2`), et les gates `fmt` /
`clippy` étaient rouges (27 diffs, 17 erreurs `bcad-ffi` + 1 `bcad-export`).

- Gates : `fmt` appliqué, lints corrigés (`replace` chaîné, imports morts,
  casts 32 bits, `too_many_lines` découpé, sections `# Safety`), doc-lints.
  Les 4 gates passent : fmt OK, `check` sans warning, `clippy -D warnings`
  propre, **170 tests Rust à 0 échec**.
- `bcad-format` : `FormatVersion::V3` (`CURRENT`/`MAX_SUPPORTED`), codec
  `value_json` (grammaire exacte C++, domaine d'enum conservé), décodeur
  `native_params` (6 grammaires natives, texte à virgules inclus), table
  `legacy_v1` (entier → `type_id`, règle `|`).
- `bcad-db` réécrit sur le schéma réel : `entities` (`type_id` + `params`
  opaque), `entity_properties` en `value_json`, lecture v1 (entiers +
  `cadastre_parcels`), `document_properties` + `sheets`/`sheet_views`/
  `furniture`/`furniture_fields`, `create_new` en vrai v3, `migrate_to_v3`
  (palier v1 porté du SQL C++, tables v3 `IF NOT EXISTS`, atomique,
  idempotent). Règle `UnknownEntity` : type inconnu conservé + signalé.
- `bcad-doctor` : `check`/`inspect`/`validate`/`report` sur fichiers réels
  (v1 : 5 entités, v2 : 3, v3 : dossier + feuilles), `migrate` v1/v2→v3 réel
  (copie d'abord, jamais sur place). Preuve croisée : un v2→v3 migré par Rust
  se charge en C++ (3 entités).
- `bcad-export` : GeoJSON depuis `params` natifs (arcs échantillonnés,
  cercles en point + `bcad_radius`, polygones fermés) ; skip compté, jamais
  inventé. CSV sans colonne `handle` morte.
- Anti-dérive : `bcad-db` embarque les fixtures C++ (`reference_v2`,
  `legacy_v1`, `reference_v3` — nouveau jeu v3 avec feuille) en tests ; une
  dérive de schéma cassera ici, pas chez un opérateur.

Reste ouvert, hors de ce lot : `export_*` sans appelant (pas de commande
`doctor export`), validation FFI non prouvée de bout en bout, pont non exercé
en CI (`BCAD_ENABLE_RUST=OFF` dans ce build), écriture Rust→C++ relue par
personne d'autre que les tests Rust.

## 10. Fuzzing réel + position papier honorée (2026-09-29)

Constat d'audit : le job CI `fuzzing` lançait trois cibles qui n'existaient
pas (`rust/fuzz/` absent), avec `continue-on-error: true` — un vert qui
mentait, exactement l'anti-motif déjà consigné au §1.

- `rust/fuzz/` créé (convention `cargo-fuzz` : `Cargo.toml` + `[[bin]]`,
  workspace propre) : `dxf_tokenizer`, `dxf_parser`, `dxf_numbers`. Graines
  versionnées (`fuzz/corpus/`), artefacts ignorés (`.gitignore`).
- CI réparé : nightly installée (libfuzzer l'exige, stable ne peut pas),
  `continue-on-error` retiré avec le motif écrit en commentaire.
- Prouvé localement (nightly 1.101) : les trois cibles compilent
  (`cargo +nightly fuzz check`), `dxf_numbers` 4,3 M runs et `dxf_parser`
  494 k runs en 60 s, **0 crash**. Le test déterministe `robustness.rs`
  (splicing seedé) reste la garde stable.

## 11. Rapport final d'intégration Rust (phase 11 du plan)

### 1. Composants gardés en C++
UI Qt complète (`MainWindow`, `Viewport`, panneaux, ruban), `Document` et
hiérarchie `Entity` (vtables ABI plugin ADR-005), registries
(`EntityRegistry`, `SerializerRegistry`, `ValidatorRegistry`,
`DocumentValidatorRegistry`), `EventBus`, commandes, `PluginManager`
(`dlopen`), `TessellationWorker`, export PDF (`QPainter`). Justification
inchangée : ABI stable, ownership C++, modèle objet Qt — rien de cela ne
traverse un FFI (plan §D, respecté : aucun type Qt/C++ dans les crates).

### 2. Composants introduits en Rust
| Crate | Rôle | État |
|-------|------|------|
| `bcad-format` | types neutres, versions (V1–V3), codec `value_json`, grammaires natives, table v1 | fait, testé |
| `bcad-dxf` | parseur DXF (tokenizer, sections, entités, limites, recovery) | fait, fuzzé |
| `bcad-validation` | primitives génériques (dégénérés, doublons, chevauchement, aire) | fait, branché au FFI |
| `bcad-db` | lecture v1/v2/v3 sur schéma réel, `migrate_to_v3` | fait, prouvé sur fixtures C++ |
| `bcad-export` | GeoJSON/CSV/DXF depuis `params` natifs | fait (skip compté, jamais inventé) |
| `bcad-ffi` | frontière C ABI, 26 fonctions `extern "C"` | fait, pont C++ `DxfBridge.cpp` |
| `bcad-doctor` | CLI `inspect/check/validate/migrate/report/dxf` | fait, lit les vrais fichiers |

### 3. Contrat FFI exact
`rust/crates/bcad-ffi/include/bcad_ffi.h` côté C++, `bcad-ffi/src/lib.rs` côté
Rust. `BcErrorCode` : 0 Ok, 1 InvalidArgument, 2 IoError, 3 ParseError,
4 InvalidFormat, 5 ResourceLimit, 6 NotFound, 255 InternalError (valeurs
gelées, partie de l'ABI). `BcString` = `{ptr, len}` UTF-8 possédé, libéré par
`bcad_string_free` ; durées de vie explicites (`*_free` obligatoire) ;
structures `#[repr(C)]`. Tout appel traverse `guard()` :
`catch_unwind` → `InternalError`, jamais de panic vers le C, jamais
d'exception vers Rust.

### 4. Fonctions `unsafe`
Tout le `unsafe` vit dans `bcad-ffi` : 22 `extern "C"` (contrats `# Safety`
documentés) + blocs minimaux (slices FFI, `CString::from_raw`,
`Box::from_raw`, handles opaques sous `Mutex`). Zéro `unsafe` dans les six
autres crates (vérifié par inspection ; `bcad-dxf` le déclare
`#![forbid(unsafe_code)]`).

### 5. Garanties obtenues
Entrée externe invalide → erreur bornée, jamais de crash : parseur sous
limites configurables (`ParseLimits`), fuzzing sans crash ( §9–10),
`robustness.rs` (splicing seedé, reproductible), migration jamais sur place
(copie d'abord), règle `UnknownEntity` des deux côtés (conservé + signalé).

### 6. Limitations C++ restantes
PDF, UI, ABI plugin, ownership `Document`/`Entity`. Pont désactivé dans ce
build (`BCAD_ENABLE_RUST=OFF`) mais exercé par le job CI `build-cpp-rust`.
Écriture Rust→C++ relue uniquement par les tests Rust.

### 7. Tests ajoutés
~170 tests Rust (`cargo test --workspace`) : codec `value_json` aller-retour
grammaire C++, 6 grammaires natives, table v1, fixtures C++ embarquées
(`reference_v2`, `legacy_v1`, `reference_v3`), migration idempotente,
`splicing` déterministe. Côté C++ : `dxf_bridge_test`,
`dxf_roundtrip_rust_test` (job CI Rust). Fuzz : 3 cibles, graines
versionnées, 4,3 M + 494 k runs, 0 crash.

### 8. Résultats builds
`cmake --build build` OK ; `cargo build/check/clippy/test --workspace`
verts (fmt + `clippy -D warnings` propres) ; `cargo +nightly fuzz check`
propre sur les 3 cibles. C++ : 48/48, `check_arch.sh` PASSED.

### 9. Résultats fuzzing
Voir §10 : `dxf_numbers` 4,3 M runs, `dxf_parser` 494 k runs en 60 s,
0 crash (nightly 1.101.0, 2026-09-28). Hebdo en CI.

### 10. Risques déploiement + rollback
`BCAD_ENABLE_RUST=OFF` = C++ seul, erreur explicite (pas de fallback
silencieux). Le `.a` Cargo n'entre jamais dans le SDK installé (chemin absolu
interdit par ADR-006 : cible `bcad_io_ffi` interne). Rollback = OFF + rebuild.

### 11. Étapes suivantes (sans élargir sans accord)
Miri sur crates sans FFI ; preuve FFI bout en bout (`bcad_validate_dxf`
sous `ctest`) ; appelants `export_*` (commande `doctor export` ?) ;
parallélisme `rayon` (retiré délibérément, à re-trancher) ; phases 3D hors
périmètre.
