# Exemples BCAD

> Exemples pratiques pour comprendre et contribuer à BCAD.

Ces exemples sont basés sur le code réel du projet.

## Liste des exemples

| Exemple | Description | Prérequis |
|---------|-------------|-----------|
| `01_add_test/` | Ajouter un test dans `smoke_test.cpp` | Aucun |
| `02_explore_document/` | Utiliser `Document`, calques | C++ |
| `03_reading_geometry/` | Types géométriques, transformations | C++ |
| `04_building_block/` | Ajouter une entité (motif d'extension) | C++, architecture |
| `sdk_proof/` | **Preuve SDK installable + plugin externe** (ADR-005/006) | CMake ≥ 3.20, test `sdk_external_test` |

## Comment utiliser ces exemples

1. Lis le `README.md` de chaque exemple
2. Compile le projet principal : `cmake --build build`
3. Expérimente dans le code source

`examples/sdk_proof` est la référence **exécutable** courante : consommateur
`find_package(BCAD)`, plugin externe minimal (entité + commande +
serializer) et chargeur qui vérifie le partage des types hôte↔plugin.

## Contribuer un exemple

Si tu as créé un exemple utile, ouvre une PR avec :
- Un nouveau dossier dans `examples/`
- Un `README.md` avec explication
- Du code qui compile réellement

## Voir aussi

- `docs/GETTING_STARTED.md` — mise en route
- `docs/FIRST_CONTRIBUTION.md` — tutoriel de contribution
- `docs/GLOSSARY.md` — définitions des termes