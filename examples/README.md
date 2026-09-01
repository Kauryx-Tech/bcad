# Exemples BCAD

> Exemples pratiques pour comprendre et contribuer à BCAD.

Ces exemples sont basés sur le code réel du projet.

## Liste des exemples

| Exemple | Description | Prérequis |
|---------|-------------|-----------|
| `01_add_test/` | Ajouter un test dans `smoke_test.cpp` | Aucun |
| `02_explore_document/` | Utiliser `Document`, calques | C++ |
| `03_reading_geometry/` | Types géométriques, transformations | C++ |
| `04_building_block/` | Ajouter une entité (architecture cible) | C++, architecture |

## Comment utiliser ces exemples

1. Lis le `README.md` de chaque exemple
2. Compile le projet principal : `cmake --build build`
3. Expérimente dans le code source

## Exemples non disponibles

Certains exemples ne peuvent pas être fournis car les fonctionnalités correspondantes n'existent pas encore :

| Exemple | Raison |
|---------|--------|
| Plugin complet | Système de plugins non implémenté (Phase 11) |
| Command personnalisée | Système de commandes non implémenté (Phase 6) |
| Serializer personnalisé | Système de serializers non implémenté (Phase 7) |
| Event Bus | Système d'événements non implémenté (Phase 8) |

Voir `docs/ARCHITECTURE_ROADMAP.md` pour les phases de migration.

## Contribuer un exemple

Si tu as créé un exemple utile, ouvre une PR avec :
- Un nouveau dossier dans `examples/`
- Un `README.md` avec explication
- Du code qui compile réellement

## Voir aussi

- `docs/GETTING_STARTED.md` — mise en route
- `docs/FIRST_CONTRIBUTION.md` — tutoriel de contribution
- `docs/GLOSSARY.md` — définitions des termes