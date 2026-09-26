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
