# Contributing to bcad

## Building

See [README.md](README.md#building) for dependency setup (either system
packages via `apt-get`, or a [vcpkg](vcpkg.json)-managed toolchain via
`CMakePresets.json`).

```bash
cmake --preset vcpkg      # or the manual apt-get + cmake -S . -B build flow
cmake --build --preset vcpkg
ctest --preset vcpkg --output-on-failure
```

## Project layout

See the "Layout" section of [README.md](README.md#layout) and
[ARCHITECTURE.md](ARCHITECTURE.md) for how modules are split and why.
[CAHIER_DES_CHARGES.md](CAHIER_DES_CHARGES.md) §3 is the up-to-date,
prioritized feature/roadmap list — check it before starting work to avoid
duplicating something already in flight or planned differently.

## Local hooks

This repo ships a `pre-commit` hook in [`.githooks/`](.githooks) (merge-conflict
markers, accidentally force-added ignored files, and clang-format if you have
both a `.clang-format` and the tool installed). It's not enabled by default —
turn it on once per clone:

```bash
git config core.hooksPath .githooks
```

CI still re-checks the real build and tests; this hook only catches the cheap
mistakes before they're committed.

## Before opening a PR

- Keep each module's dependency direction one-way (`geometry` → `layers` →
  `render` → `core` → `io` → `app`); don't add a back-edge to make one patch
  easier.
- Add or extend a check in `tests/smoke_test.cpp` for new `core`/`geometry`/
  `io` behavior — there's no framework, just one function per module, so
  follow the existing pattern rather than introducing a new one.
- Run `ctest --test-dir build --output-on-failure` locally before pushing;
  CI runs the same command.
- Keep commits scoped to one logical change; describe the *why* in the
  commit message, not just the *what*.

## Maintainer: branch protection

`scripts/setup-branch-protection.sh` enables branch protection on `main`
(CI must pass before merge) via `gh api`. One-off: run it once, after the
repo is pushed, once `gh auth login` is done and the CI workflow has run at
least once.

## Reporting bugs / proposing features

Use the issue templates under "New Issue" — they ask for the information
needed to reproduce a bug or evaluate a feature request without a
back-and-forth.
