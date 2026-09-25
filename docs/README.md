# Documentation BCAD

> Point d'entrée de la documentation. Si tu découvres le projet, commence par ici.

## Qu'est-ce que BCAD ?

Application CAO 2D en C++20. Technologies : Qt6 (UI), OpenGL 3.3 (rendu), CGAL (géométrie), SQLite3 (format natif). Licence GPL-3.0-or-later.

## À qui s'adresse cette documentation ?

| Public | Parcours |
|--------|----------|
| **Étudiant / nouveau** | `QUICK_START.md` → `GETTING_STARTED.md` → `VISUAL_ARCHITECTURE.md` → `GLOSSARY.md` |
| **Dev C++exp** | `ARCHITECTURE.md` → `ARCHITECTURE_PRINCIPLES.md` → `ARCHITECTURE_DECISIONS.md` → `WALKTHROUGH.md` |
| **Contributeur Core** | `CONTRIBUTOR_GUIDE.md` → `FIRST_CONTRIBUTION.md` → `TESTING_ARCHITECTURE.md` |
| **Auteur de plugins** | `EXTENDING_BCAD.md` → `PLUGIN_ARCHITECTURE.md` → `SDK_ARCHITECTURE.md` |
| **Agent IA** | `../AGENTS.md` → `ARCHITECTURE.md` → `ARCHITECTURE_PRINCIPLES.md` |
| **Quelqu'un qui cherche** | `DECISION_TREE.md` — "Je suis X, je veux Y" |
| **Quelqu'un qui a un bug** | `TROUBLESHOOTING.md` |

## Architecture actuelle vs cible

Les trois transitions annoncées ci-dessous sont **terminées dans le code** : le
tableau décrit l'architecture en place, ce n'est plus un état d'avancement.

| Aspect | Avant | Actuel |
|--------|-------|--------|
| Types d'entités | enum `EntityType` figé | `TypeId` + `EntityRegistry` (l'enum ne subsiste que pour la rétrocompatibilité) |
| CGAL | exposé dans `Types.h` | confiné à `src/geometry/*.cpp` et `detail/`, contrôlé par `scripts/check_arch.sh` |
| Plugins | aucun | modules dynamiques `dlopen` + SDK installable + module cadastral chargé par découverte |

Ce qui reste ouvert est porté par domaine, pas par couche :
`CADASTRE_PLUGIN_STATUS.md` et `CADASTRAL_AUDIT_2026.md` pour le cadastre,
`ROADMAP_MARKET.md` pour l'ordre de priorité, `ARCHITECTURE_ROADMAP.md` pour la
migration technique.

---

## Index des documents

### 📖 Guides de démarrage

| Document | Objectif | Consulter quand |
|----------|----------|-----------------|
| **`QUICK_START.md`** | **En 5 minutes** | Tu veux lancer BCAD maintenant |
| `GETTING_STARTED.md` | Configuration | Tu découvres BCAD |
| `FIRST_CONTRIBUTION.md` | Tutoriel PR | Première contribution |

### 🏗️ Architecture

| Document | Objectif | Consulter quand |
|----------|----------|-----------------|
| `ARCHITECTURE.md` | Vue d'ensemble | Débuter |
| **`VISUAL_ARCHITECTURE.md`** | **Diagrammes ASCII** | Tu apprends mieux visuellement |
| **`WHY_THIS_DESIGN.md`** | **Justification** | Pourquoi ces choix ? |
| `ARCHITECTURE_PRINCIPLES.md` | 15 principes | Avant modification |
| `ARCHITECTURE_DECISIONS.md` | 15 ADR | Contexte décision |
| `ARCHITECTURE_ROADMAP.md` | Plan de migration | Voir où on va |
| `ROADMAP_MARKET.md` | Feuille de route marché | Prioriser selon l'usage visé |
| **`WALKTHROUGH.md`** | **Visite du code** | Comprendre le flux main→géométrie |

### 🔧 Détails techniques

| Document | Objectif | Consulter quand |
|----------|----------|-----------------|
| `GEOMETRY_ARCHITECTURE.md` | Géométrie, CGAL | Modifier géométrie |
| `ENTITY_MODEL.md` | Modèle d'entité | Ajouter Entity |
| `DOCUMENT_MODEL.md` | Modèle de document | Modifier Document |
| `COMMAND_SYSTEM.md` | Commandes | Ajouter Command |
| `EVENT_SYSTEM.md` | Bus d'événements | Ajouter Event |
| `RENDERING_ARCHITECTURE.md` | Rendu | Modifier rendu |
| `SPATIAL_INDEX.md` | Index spatial | Modifier index |
| `PERSISTENCE_ARCHITECTURE.md` | Persistence | Modifier persistence |
| `IO_ARCHITECTURE.md` | IO, DXF | Ajouter un format |
| `COORDINATE_SYSTEMS.md` | WCS/UCS | Systèmes de coordonnées |
| `PROPERTY_SYSTEM.md` | Propriétés | Ajouter des propriétés |
| `TYPEID_STABILITY.md` | Stabilité des `TypeId` | Renommer un type existant |
| `COMMAND_PATTERN.md` | Recette d'une commande | Écrire une commande Core ou plugin |
| `EVENTBUS_DELIVERY.md` | Garanties de livraison / filtrage | Publier ou consommer un événement |
| `SWITCH_REMOVAL_PATTERNS.md` | Sortie de `switch(EntityType)` | Croiser l'enum dans du code neuf |
| `UI_CONVENTIONS.md` | Contrats d'interface (français, docks, snaps) | Modifier l'UI ou un workbench |

### 🏛️ Métiers et état réel

| Document | Objectif | Consulter quand |
|----------|----------|-----------------|
| `CADASTRE_SPEC.md` | Exigences du module cadastral | Étendre le cadastre |
| `CADASTRE_PLUGIN_STATUS.md` | **Ce qui marche vraiment** dans le module | Savoir ce qui reste à faire |
| `CADASTRAL_AUDIT_2026.md` | Audit fonctionnel cadastral | Confronter promesses et dépôt |
| `CONSOLIDATION_STATUS.md` | Feuille d'avancement par tâche | Reprendre un lot inachevé |

### 🗄️ Historique de conception (ne pas prendre pour l'état actuel)

Ces documents gardent la trace des arbitrages ; ils décrivent un état dépassé et
sont conservés pour cela. En cas de conflit, le code puis les ADR font foi.

| Document | Rôle |
|----------|------|
| `ARCHITECTURE_BENCHMARK.md` | Comparaison publique avec AutoCAD, FreeCAD, QCAD/LibreCAD, BRL-CAD, OCCT |
| `ARCHITECTURE_REVIEW.md` | Synthèse d'audit de l'époque |
| `CGAL_MIGRATION.md`, `MIGRATION_PLAN.md` | Plans de migration CGAL et globale ; `ARCHITECTURE_ROADMAP.md` en est la checklist |

### 🔌 Extension

| Document | Objectif | Consulter quand |
|----------|----------|-----------------|
| `EXTENDING_BCAD.md` | Extension | Étendre BCAD |
| `PLUGIN_ARCHITECTURE.md` | Plugins | Créer plugin |
| `SDK_ARCHITECTURE.md` | SDK public | Créer plugin |
| `PLUGIN_DOMAINS.md` | Domaines métiers | Choisir le prochain plugin |
| `ROADMAP_MARKET.md` | Feuille de route marché | Arbitrer ce qu'on construit et dans quel ordre |

### 📚 Références

| Document | Objectif | Consulter quand |
|----------|----------|-----------------|
| `GLOSSARY.md` | Définitions | Terme inconnu |
| **`DECISION_TREE.md`** | **"Où aller ?"** | Tu ne sais pas par où commencer |
| **`TROUBLESHOOTING.md`** | **Dépannage** | Un problème bloque |
| `API_ABI_POLICY.md` | API/ABI | Modifier API |
| `BUILD_AND_PACKAGING.md` | CMake | Compiler |
| `TESTING_ARCHITECTURE.md` | Tests | Ajouter tests |
| `CONTRIBUTOR_GUIDE.md` | Standards C++ | Contribuer |
| `WORKBENCH.md` | Workbench | Organiser outils |
| `LICENSING.md` | Licences | Questions GPL |
| `THIRD_PARTY_LICENSES.md` | Dépendances | CGAL, Boost, etc. |

---

## Matrice Tâche → Documentation

| Tâche | Documents |
|-------|-----------|
| Ajouter une Entity | `ENTITY_MODEL.md`, `GEOMETRY_ARCHITECTURE.md` |
| Ajouter une Command | `COMMAND_SYSTEM.md` |
| Ajouter un Plugin | `EXTENDING_BCAD.md`, `PLUGIN_ARCHITECTURE.md` |
| Planifier un nouveau plugin métier | `PLUGIN_DOMAINS.md`, `WORKBENCH.md` |
| Publier une propriété d'un plugin dans le panneau | `PLUGIN_ARCHITECTURE.md`, `WORKBENCH.md` |
| Déclarer une règle de vérification métier | `PLUGIN_ARCHITECTURE.md` (§ `IValidator`), `WORKBENCH.md` |
| Modifier la géométrie | `GEOMETRY_ARCHITECTURE.md` |
| Modifier le Renderer | `RENDERING_ARCHITECTURE.md` |
| Modifier la persistence | `PERSISTENCE_ARCHITECTURE.md` |
| Ajouter des tests | `TESTING_ARCHITECTURE.md` |
| Modifier l'architecture | `ARCHITECTURE.md`, `ARCHITECTURE_PRINCIPLES.md`, `ARCHITECTURE_DECISIONS.md` |

---

## Thèmes transversaux

| Thème | Documents |
|-------|-----------|
| CGAL | `GEOMETRY_ARCHITECTURE.md`, `THIRD_PARTY_LICENSES.md`, `CGAL_MIGRATION.md` |
| Plugins | `PLUGIN_ARCHITECTURE.md`, `PLUGIN_DOMAINS.md`, `SDK_ARCHITECTURE.md`, `EXTENDING_BCAD.md`, `WORKBENCH.md` |
| API/ABI | `API_ABI_POLICY.md`, `SDK_ARCHITECTURE.md`, `TYPEID_STABILITY.md` |
| Validation métier | `PLUGIN_ARCHITECTURE.md`, `CADASTRE_PLUGIN_STATUS.md`, `CADASTRAL_AUDIT_2026.md` |
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

Ces schémas ne sont générés par aucun contrôle : ils dérivent dès qu'une couche
bouge. `VISUAL_ARCHITECTURE.md`, lui, est tenu à jour avec le code ; en cas de
désaccord entre les deux, croire `VISUAL_ARCHITECTURE.md` et `check_arch.sh`.

Mis à jour : 2026-09-25.
