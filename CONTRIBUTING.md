# Contribuer à bcad

Ce fichier est la porte d'entrée ; chaque sujet a **une** référence, citée ici
plutôt que recopiée.

## Lire d'abord

| Pour | Lire |
|---|---|
| Installer, compiler, tester, normes de code, code commun, tests, PR | [`docs/CONTRIBUTOR_GUIDE.md`](docs/CONTRIBUTOR_GUIDE.md) |
| Interface : jetons visuels, composants, comportement des outils, recettes | [`docs/DESIGN_SYSTEM.md`](docs/DESIGN_SYSTEM.md) |
| Où va un outil (ruban, menus, cycle d'une commande) | [`docs/UI_CONVENTIONS.md`](docs/UI_CONVENTIONS.md) |
| Règles d'architecture non négociables | [`AGENTS.md`](AGENTS.md), [`docs/ARCHITECTURE_DECISIONS.md`](docs/ARCHITECTURE_DECISIONS.md) |
| Ce qui reste à faire, par priorité | [`TODO_OUTILS.md`](TODO_OUTILS.md) |
| Premier pas guidé, par niveau | [`docs/FIRST_CONTRIBUTION.md`](docs/FIRST_CONTRIBUTION.md) |

## En bref

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j2
ctest --test-dir build --output-on-failure
scripts/check_arch.sh                # frontieres, design system, code commun
git config core.hooksPath .githooks  # une fois par clone : verifications avant commit
```

Une PR = un changement logique, avec son test, `check_arch.sh` au vert, et le
message de commit qui dit **pourquoi**. La liste complète est dans le guide
(« Checklist PR »).

## Mainteneur : protection de branche

`scripts/setup-branch-protection.sh` active la protection de `main` (la CI doit
passer avant merge) via `gh api`, une fois `gh auth login` fait.

## Signaler un bug, proposer une fonctionnalité

Modèles d'issue sous « New Issue ».
