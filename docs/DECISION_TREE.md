# Arbre de décision BCAD

> **Je suis X, je veux Y, où dois-je commencer ?**

Ce document t'aide à naviguer rapidement vers la bonne section selon ton profil et ton besoin.

## Sommaire

- [Par profil utilisateur](#par-profil-utilisateur)
- [Par type de tâche](#par-type-de-tâche)
- [Par symptôme / problème](#par-symptôme--problème)
- [Par concept à comprendre](#par-concept-à-comprendre)
- [Index rapide](#index-rapide-des-documents)

---

## Par profil utilisateur

### Je suis nouveau sur le projet

**Documents à lire dans cet ordre :**
1. [QUICK_START.md](QUICK_START.md) — Lancer BCAD en 5 min
2. [GETTING_STARTED.md](GETTING_STARTED.md) — Configuration détaillée
3. [VISUAL_ARCHITECTURE.md](VISUAL_ARCHITECTURE.md) — Vue d'ensemble visuelle
4. [GLOSSARY.md](GLOSSARY.md) — Termes techniques

### Je suis développeur C++ expérimenté

**Documents à lire :**
1. [ARCHITECTURE.md](ARCHITECTURE.md) — Vue d'ensemble
2. [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md) — 15 principes
3. [ARCHITECTURE_DECISIONS.md](ARCHITECTURE_DECISIONS.md) — 15 ADR
4. [WALKTHROUGH.md](WALKTHROUGH.md) — Visite guidée du code

### Je veux contribuer au Core

```
            Je veux contribuer au Core
                       │
                       ▼
            [FIRST_CONTRIBUTION.md](FIRST_CONTRIBUTION.md)
                       │
                       ▼
              ┌────────────────┐
              │ Quel type ?    │
              └────┬───────────┘
                   │
       ┌───────────┼───────────┐
       ▼           ▼           ▼
   Bug fix    Feature    Refactor
       │           │           │
       ▼           ▼           ▼
  CONTRIBUTOR_  EXTENDING_  ARCHITECTURE_
    GUIDE.md    BCAD.md      PRINCIPLES.md
```

**Documents :**
- [FIRST_CONTRIBUTION.md](FIRST_CONTRIBUTION.md) — Guide pas-à-pas
- [CONTRIBUTOR_GUIDE.md](CONTRIBUTOR_GUIDE.md) — Standards C++
- [TESTING_ARCHITECTURE.md](TESTING_ARCHITECTURE.md) — Tests

### Je veux créer un plugin

**Documents :**
1. [EXTENDING_BCAD.md](EXTENDING_BCAD.md) — Vue d'ensemble extension
2. [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md) — Système de plugins
3. [SDK_ARCHITECTURE.md](SDK_ARCHITECTURE.md) — SDK public
4. [examples/04_building_block/](../examples/04_building_block/) — Exemple

### Je suis un agent IA

**Documents :**
1. [../AGENTS.md](../AGENTS.md) — Guide agents IA (racine)
2. [ARCHITECTURE.md](ARCHITECTURE.md)
3. [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md)
4. [ARCHITECTURE_DECISIONS.md](ARCHITECTURE_DECISIONS.md)

---

## Par type de tâche

### Je veux ajouter une entité géométrique

**Documents actuels :**
- [ENTITY_MODEL.md](ENTITY_MODEL.md) — Modèle d'entité
- [GEOMETRY_ARCHITECTURE.md](GEOMETRY_ARCHITECTURE.md) — Géométrie

### Je veux ajouter une commande

**Documents :**
- [COMMAND_SYSTEM.md](COMMAND_SYSTEM.md) — Système de commandes
- [ENTITY_MODEL.md](ENTITY_MODEL.md) — Modèle d'entité

### Je veux modifier le rendu

**Documents :**
- [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) — Pipeline de rendu
- [VISUAL_ARCHITECTURE.md](VISUAL_ARCHITECTURE.md#5-architecture-du-rendu) — Schéma rendu

### Je veux ajouter un format de fichier

**Documents :**


---

## Par concept à comprendre

### Comprendre le Core

```
        Comprendre le Core
              │
              ▼
   [ARCHITECTURE.md](ARCHITECTURE.md) §3
              │
              ▼
   ┌──────────────────────────┐
   │ Modules du Core :         │
   │  • geometry              │
   │  • document              │
   │  • commands              │
   │  • events                │
   │  • layers                │
   │  • properties            │
   │  • index                 │
   └──────────────────────────┘
```

### Comprendre les plugins

```
        Comprendre les plugins
                │
                ▼
   [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md)
                │
       ┌────────┼────────┐
       ▼        ▼        ▼
   Concept  Lifecycle  Exemple
   [EXTENDING_BCAD.md](EXTENDING_BCAD.md)
```

### Comprendre le rendu

```
        Comprendre le rendu
               │
               ▼
   [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md)
               │
       ┌───────┼───────┐
       ▼       ▼       ▼
   Scene   Tessell.  Backend
   extract
```

### Comprendre l'undo/redo

**Documents :**
- [COMMAND_SYSTEM.md](COMMAND_SYSTEM.md) — Transactions
- [DOCUMENT_MODEL.md](DOCUMENT_MODEL.md) — Transactions côté Document

### Comprendre les événements

**Documents :**
- [EVENT_SYSTEM.md](EVENT_SYSTEM.md) — Bus d'événements

---

## Index rapide des documents

| Besoin | Document |
|--------|----------|
| Démarrer rapidement | QUICK_START.md |
| Configurer l'environnement | GETTING_STARTED.md |
| Vue d'ensemble | ARCHITECTURE.md |
| Diagrammes visuels | VISUAL_ARCHITECTURE.md |
| Décisions passées | ARCHITECTURE_DECISIONS.md |
| Principes | ARCHITECTURE_PRINCIPLES.md |
| Roadmap | ARCHITECTURE_ROADMAP.md |
| Contribution Core | CONTRIBUTOR_GUIDE.md |
| Première contribution | FIRST_CONTRIBUTION.md |
| Créer un plugin | EXTENDING_BCAD.md |
| Système de plugins | PLUGIN_ARCHITECTURE.md |
| SDK public | SDK_ARCHITECTURE.md |
| Géométrie | GEOMETRY_ARCHITECTURE.md |
| Entités | ENTITY_MODEL.md |
| Document | DOCUMENT_MODEL.md |
| Commandes | COMMAND_SYSTEM.md |
| Événements | EVENT_SYSTEM.md |
| Rendu | RENDERING_ARCHITECTURE.md |
| Propriétés | PROPERTY_SYSTEM.md |
| Index spatial | SPATIAL_INDEX.md |
| Coordonnées | COORDINATE_SYSTEMS.md |
| Persistence | PERSISTENCE_ARCHITECTURE.md |
| IO/DXF | IO_ARCHITECTURE.md |
| Workbench | WORKBENCH.md |
| Build | BUILD_AND_PACKAGING.md |
| Tests | TESTING_ARCHITECTURE.md |
| API/ABI | API_ABI_POLICY.md |
| Licences | LICENSING.md |
| Dépendances | THIRD_PARTY_LICENSES.md |
| Termes techniques | GLOSSARY.md |
| Problèmes | TROUBLESHOOTING.md |
| Visite code | WALKTHROUGH.md |

---

## Voir aussi

- [README.md](README.md) — Index de la documentation
- [AGENTS.md](../AGENTS.md) — Guide agents IA
- [IO_ARCHITECTURE.md](IO_ARCHITECTURE.md) — Entrées/sorties
- [PERSISTENCE_ARCHITECTURE.md](PERSISTENCE_ARCHITECTURE.md) — Persistence

### Je veux préparer la 3D

**Documents :**
- [COORDINATE_SYSTEMS.md](COORDINATE_SYSTEMS.md) — WCS/UCS
- [SPATIAL_INDEX.md](SPATIAL_INDEX.md) — Index spatial
- [GEOMETRY_ARCHITECTURE.md](GEOMETRY_ARCHITECTURE.md) — Types 2D/3D

### Je veux modifier le système de coordonnées

**Documents :**
- [COORDINATE_SYSTEMS.md](COORDINATE_SYSTEMS.md) — Systèmes de coordonnées

---

## Par symptôme / problème

### Erreur de compilation

**Documents :**
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — Erreurs courantes
- [BUILD_AND_PACKAGING.md](BUILD_AND_PACKAGING.md) — Configuration build

### Performance dégradée

**Documents :**
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — Section performance
- [SPATIAL_INDEX.md](SPATIAL_INDEX.md) — Optimisation via index spatial

### Comportement UI inattendu

**Documents :**
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) — Section affichage

### Fichier .bcad corrompu

**Documents :**
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
- [PERSISTENCE_ARCHITECTURE.md](PERSISTENCE_ARCHITECTURE.md) — Format SQLite