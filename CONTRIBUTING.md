# Contribuer à bcad

## Compilation

Voir [README.md](README.md#compilation) pour la mise en place des
dépendances (paquets système via `apt-get`, ou une chaîne d'outils gérée
par [vcpkg](vcpkg.json) via `CMakePresets.json`).

```bash
cmake --preset vcpkg      # ou le chemin manuel apt-get + cmake -S . -B build
cmake --build --preset vcpkg
ctest --preset vcpkg --output-on-failure
```

## Organisation du projet

Voir la section « Organisation » du [README.md](README.md#organisation) et
[ARCHITECTURE.md](ARCHITECTURE.md) pour la répartition des modules et sa
justification. [CAHIER_DES_CHARGES.md](CAHIER_DES_CHARGES.md) §3 est la
liste priorisée et à jour des fonctionnalités/de la feuille de route —
à consulter avant de commencer pour éviter de dupliquer quelque chose déjà
en cours ou prévu différemment.

## Hooks locaux

Ce dépôt fournit un hook `pre-commit` dans [`.githooks/`](.githooks)
(marqueurs de conflit de fusion, fichiers ignorés accidentellement
force-ajoutés, et clang-format si `.clang-format` et l'outil sont tous
deux présents). Il n'est pas activé par défaut — active-le une fois par
clone :

```bash
git config core.hooksPath .githooks
```

La CI revérifie toujours le vrai build et les tests ; ce hook ne fait que
rattraper les erreurs bon marché avant qu'elles ne soient commitées.

## Avant d'ouvrir une PR

- Garde le sens de dépendance de chaque module à sens unique (`geometry` →
  `layers` → `render` → `core` → `io` → `app`) ; n'ajoute pas d'arête
  retour pour te faciliter un patch.
- Ajoute ou étends une vérification dans `tests/smoke_test.cpp` pour tout
  nouveau comportement dans `core`/`geometry`/`io` — il n'y a pas de
  framework, juste une fonction par module, donc suis le modèle existant
  plutôt que d'en introduire un nouveau.
- Lance `ctest --test-dir build --output-on-failure` en local avant de
  pousser ; la CI lance la même commande.
- Garde des commits circonscrits à un seul changement logique ; décris le
  *pourquoi* dans le message de commit, pas seulement le *quoi*.

## Mainteneur : protection de branche

`scripts/setup-branch-protection.sh` active la protection de branche sur
`main` (la CI doit passer avant merge) via `gh api`. Ponctuel : à lancer
une fois, après que le dépôt a été poussé, une fois `gh auth login` fait
et le workflow CI exécuté au moins une fois.

## Signaler des bugs / proposer des fonctionnalités

Utilise les modèles d'issue sous « New Issue » — ils demandent les
informations nécessaires pour reproduire un bug ou évaluer une demande de
fonctionnalité sans aller-retour.
