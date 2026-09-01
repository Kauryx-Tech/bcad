# Documentation BCAD

> Point d'entrée de la documentation. Si tu découvres le projet, commence par ici.

## Qu'est-ce que BCAD ?

Application CAO 2D en C++20. Technologies : Qt6 (UI), OpenGL 3.3 (rendu), CGAL (géométrie), SQLite3 (format natif). Licence GPL-3.0-or-later.

## À qui s'adresse cette documentation ?

| Public | Parcours |
|--------|----------|
| **Étudiant / nouveau** | `GETTING_STARTED.md` → `FIRST_CONTRIBUTION.md` → `GLOSSARY.md` |
| **Dev C++exp** | `ARCHITECTURE.md` → `ARCHITECTURE_PRINCIPLES.md` → `ARCHITECTURE_DECISIONS.md` |
| **Contributeur Core** | `CONTRIBUTOR_GUIDE.md` → `TESTING_ARCHITECTURE.md` |
| **Auteur de plugins** | `EXTENDING_BCAD.md` → `PLUGIN_ARCHITECTURE.md` |
| **Agent IA** | `../AGENTS.md` → `ARCHITECTURE.md` → `ARCHITECTURE_PRINCIPLES.md` |

## Architecture actuelle vs cible

L'architecture **actuelle** (code) est en transition vers l'architecture **cible** (documentation).

| Aspect | Actuel | Cible |
|--------|--------|-------|
| EntityType | enum figé | TypeId + Registry |
| CGAL | exposé dans Types.h | confiné dans geometry/detail/ |
| Plugins | aucun | Plugin System + SDK |

Voir `ARCHITECTURE_ROADMAP.md` pour le plan de migration.

---

## Index des documents

| Document | Objectif | Consulter quand |
|----------|----------|-----------------|
| `GETTING_STARTED.md` | Mise en route | Tu découvres BCAD |
| `FIRST_CONTRIBUTION.md` | Tutoriel PR | Première contribution |
| `GLOSSARY.md` | Définitions | Terme inconnu |
| `ARCHITECTURE.md` | Vue d'ensemble | Débuter |
| `ARCHITECTURE_PRINCIPLES.md` | 15 principes | Avant modification |
| `ARCHITECTURE_DECISIONS.md` | 15 ADR | Contexte décision |
| `GEOMETRY_ARCHITECTURE.md` | Géométrie, CGAL | Modifier géométrie |
| `ENTITY_MODEL.md` | Modèle d'entité | Ajouter Entity |
| `DOCUMENT_MODEL.md` | Modèle de document | Modifier Document |
| `COMMAND_SYSTEM.md` | Commandes | Ajouter Command |
| `EVENT_SYSTEM.md` | Bus d'événements | Ajouter Event |
| `RENDERING_ARCHITECTURE.md` | Rendu | Modifier rendu |
| `SPATIAL_INDEX.md` | Index spatial | Modifier index |
| `PERSISTENCE_ARCHITECTURE.md` | Persistence | Modifier persistence |
| `PLUGIN_ARCHITECTURE.md` | Plugins | Créer plugin |
| `SDK_ARCHITECTURE.md` | SDK public | Créer plugin |
| `API_ABI_POLICY.md` | API/ABI | Modifier API |
| `BUILD_AND_PACKAGING.md` | CMake | Compiler |
| `TESTING_ARCHITECTURE.md` | Tests | Ajouter tests |
| `CONTRIBUTOR_GUIDE.md` | Standards C++ | Contribuer |
| `EXTENDING_BCAD.md` | Extension | Étendre BCAD |

---

## Matrice Tâche → Documentation

| Tâche | Documents |
|-------|-----------|
| Ajouter une Entity | `ENTITY_MODEL.md`, `GEOMETRY_ARCHITECTURE.md` |
| Ajouter une Command | `COMMAND_SYSTEM.md` |
| Ajouter un Plugin | `EXTENDING_BCAD.md`, `PLUGIN_ARCHITECTURE.md` |
| Modifier la géométrie | `GEOMETRY_ARCHITECTURE.md` |
| Modifier le Renderer | `RENDERING_ARCHITECTURE.md` |
| Modifier la persistence | `PERSISTENCE_ARCHITECTURE.md` |
| Ajouter des tests | `TESTING_ARCHITECTURE.md` |
| Modifier l'architecture | `ARCHITECTURE.md`, `ARCHITECTURE_PRINCIPLES.md`, `ARCHITECTURE_DECISIONS.md` |

---

## Thèmes transversaux

| Thème | Documents |
|-------|-----------|
| CGAL | `GEOMETRY_ARCHITECTURE.md`, `THIRD_PARTY_LICENSES.md` |
| Plugins | `PLUGIN_ARCHITECTURE.md`, `SDK_ARCHITECTURE.md`, `EXTENDING_BCAD.md` |
| API/ABI | `API_ABI_POLICY.md`, `SDK_ARCHITECTURE.md` |
| Licence | `LICENSING.md`, `THIRD_PARTY_LICENSES.md` |

## Documents de référence (racine)

- `../ARCHITECTURE.md` — architecture actuelle
- `../DESIGN_NOTES.md` — études comparatives
- `../CAHIER_DES_CHARGES.md` — fonctionnalités
- `../CONTRIBUTING.md` — comment contribuer
- `../AGENTS.md` — guide agents IA

## Schémas

Format Draw.io dans `docs/schemas/` :
- `architecture_overview.drawio`
- `core_dependencies.drawio`
- `plugin_lifecycle.drawio`
- `document_entity.drawio`

Mis à jour : 2026-09-01.
