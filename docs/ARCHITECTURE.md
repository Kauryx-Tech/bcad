# Architecture BCAD

> [!IMPORTANT]
>
> ## Statut : implémentée (noyau) — le SDK plugin est la marge de maturité
>
> Le **noyau** (document, commands, properties, index spatial, events, les
> registries, les serializers) est **implémenté** dans `include/bcad/` et `src/`,
> avec sa preuve installable `examples/sdk_proof` (plugins, ADR-005). La
> « cible » d'ARCHITECTURE décrit la forme complète (workbench, 2D/3D complet) ;
> voir `ARCHITECTURE_REVIEW.md` pour l'état réel point par point.

> **Source de vérité architecturale.** Ce document décrit l'architecture cible de BCAD en tant que plateforme CAO extensible 2D/3D. Voir `ARCHITECTURE_ROADMAP.md` pour le plan de migration.

## 1. Vue d'ensemble

BCAD est une plateforme CAO modulaire, écrite en C++20, construite autour d'un **Core** indépendant et étendue par un **SDK** et un **système de plugins**.

```
                    BCAD APPLICATION
                           │
             ┌─────────────┴─────────────┐
             │                           │
          BCAD CORE                  BCAD SDK
             │                           │
      ┌──────┼────────┐          Extension APIs
      │      │        │                 │
  Geometry Document Services             │
      │      │        │                 │
      └──────┼────────┘                 │
             │                           │
      ┌──────┼─────────────┐             │
      │      │             │             │
 Rendering Persistence     IO             │
   API       API          API             │
      │                                  │
 OpenGL/Vulkan                    Plugin System
                                         │
                         ┌───────────────┼───────────────┐
                         │               │               │
                    Architecture       Civil       Mechanical
```

## 2. Règle d'or

**Le Core ne dépend ni de Qt, ni d'OpenGL, ni du renderer, ni d'un format de fichier.** Le Core est une bibliothèque C++20 pure, qui modélise le document et la géométrie.

## 3. Modules du Core

| Module | Responsabilité | Dépendances |
|--------|----------------|-------------|
| `geometry` | Types géométriques, transformations, booléens, triangulation | Aucune (std uniquement) |
| `layers` | Gestion des calques, propriétés de calque | `geometry` |
| `document` | Modèle de document, entités, lifecycle, transactions, événements | `geometry`, `layers` |
| `commands` | Système de commandes, transactions, undo/redo | `document` |
| `properties` | Système de propriétés génériques | `document` |
| `index` | Abstraction d'index spatial | `document` |
| `events` | Bus d'événements découplé | aucune |

## 4. Graphe cible

```
application (Qt)
        │
        ▼
┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│   rendering  │   │  persistence │   │     io       │
│   backend    │   │              │   │ (DXF, etc.)  │
└──────┬───────┘   └──────┬───────┘   └──────┬───────┘
       └────────────────────┴────────────────────┘
                           │
                    ┌──────────────┐
                    │     sdk      │
                    └──────┬───────┘
                           │
                    ┌──────────────┐
                    │     core      │
                    └──────┬───────┘
                           ▲
                    ┌──────┴───────┐
                    │  plugin system │
                    └───────────────┘
```

**Propriétés :** pas de cycles. Le Core ne pointe vers aucun service périphérique. Les plugins dépendent du SDK, jamais du Core interne.

## 5. Graphe actuel vs graphe cible

| Aspect | Aujourd'hui | Cible |
|--------|------------|-------|
| Core ↔ Render | Découplé (Quadtree déplacé dans `index/`, EventBus header-only) | Indépendant |
| Core ↔ Qt | Qt confiné à `app` (aucun header Qt dans Core) | Aucun header Qt ni render |
| EntityType | Enum déprécié, `TypeId` + `EntityRegistry` en place | Registry dynamique (fait) |
| Plugins | `PluginManager` (fnptr, médiation chapeau), `bcad_plugin` hôte, preuve `sdk_external_test`. Six points d'enregistrement (types, commandes, serializers, workbenches, validateurs, exporteurs) + un canal de données réglables (`resolveDataFile`) | Système complet + SDK (fait, marge mature) |
| CGAL | Privé (confiné à `src/geometry/*.cpp` + `detail/`) | Privé (fait) |
| CMake | SDK installé (`BCAD::bcad_core`, `bcad_geometry`, `bcad_plugin`), `sdk_proof` le consomme | SDK exporté versionné (fait) |

## 6. Documents liés

- `ARCHITECTURE_PRINCIPLES.md` — Principes non négociables
- `ARCHITECTURE_BENCHMARK.md` — Comparaison CAO
- `GEOMETRY_ARCHITECTURE.md` — Architecture géométrique
- `ENTITY_MODEL.md` — Modèle d'entité extensible
- `DOCUMENT_MODEL.md` — Modèle de document
- `PROPERTY_SYSTEM.md` — Système de propriétés
- `COMMAND_SYSTEM.md` — Commandes et transactions
- `EVENT_SYSTEM.md` — Bus d'événements
- `RENDERING_ARCHITECTURE.md` — Architecture de rendu
- `SPATIAL_INDEX.md` — Index spatial
- `COORDINATE_SYSTEMS.md` — Systèmes de coordonnées
- `PERSISTENCE_ARCHITECTURE.md` — Persistence
- `IO_ARCHITECTURE.md` — Entrées/sorties
- `SDK_ARCHITECTURE.md` — SDK public
- `PLUGIN_ARCHITECTURE.md` — Système de plugins
- `API_ABI_POLICY.md` — Politique API/ABI
- `BUILD_AND_PACKAGING.md` — Build et packaging
- `TESTING_ARCHITECTURE.md` — Tests
- `LICENSING.md` — Analyse des licences
- `THIRD_PARTY_LICENSES.md` — Dépendances tierces
- `ARCHITECTURE_ROADMAP.md` — Plan de migration
- `ARCHITECTURE_DECISIONS.md` — ADR (Architecture Decision Records)