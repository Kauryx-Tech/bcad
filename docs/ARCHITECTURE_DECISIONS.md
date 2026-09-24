# Décisions architecturales BCAD (ADR)

> [!IMPORTANT]
>
> ## Statut : décisions appliquées, avec écarts explicitement documentés
>
> Les ADR décrivent les invariants architecturaux. Les ADR-001 à 011 sont vérifiés par
> `scripts/check_arch.sh` et les tests d'architecture. Les écarts restants sont des extensions
> prévues (migrations avancées, backend de rendu multiple et 3D), suivies dans
> `ARCHITECTURE_ROADMAP.md`.

> Consigne les décisions importantes pour éviter qu'un futur développeur ne "simplifie" en supprimant des abstractions délibérées.

## Index

| # | Titre | Statut |
|---|-------|--------|
| 001 | Core indépendant du Renderer | Accepté |
| 002 | CGAL non exposé dans l'API | Accepté |
| 003 | Entité extensible par Registry | Accepté |
| 004 | Persistence par Serializer Registry | Accepté |
| 005 | Plugin System dynamique | Accepté |
| 006 | SDK public versionné | Accepté |
| 007 | Architecture 2D/3D simultanée | Accepté |
| 008 | Spatial Index indépendant du Renderer | Accepté |
| 009 | Command + Transaction primitives | Accepté |
| 010 | Event Bus typé | Accepté |
| 011 | Politique API/ABI v1 | Accepté |
| 012 | Licence GPL-3.0 | Accepté |
| 013 | Pas d'abstractions virtuelles inutiles | Accepté |
| 014 | Couches Core/Services/App/Plugins | Accepté |
| 015 | Persistence SQLite + JSON | Accepté |

---

## 001 : Core indépendant du Renderer

**Statut :** Accepté

**Problème :** `core::Document` possède `render::Quadtree`. Core ne compile pas sans render.

**Décision :** Core sans header de `render/`. Document utilise `ISpatialIndex`.

**Conséquences :** Core testable sans rendu. Indirection.

---

## 002 : CGAL non exposé dans l'API

**Problème :** `Types.h` expose `CGAL::Point_2`.

**Décision :** Types BCAD neutres (`Point2 { double x, y; }`). CGAL dans `geometry/detail/`.

**Conséquences :** API stable. CGAL remplaçable.

---

## 003 : Entité extensible par Registry

**Problème :** `enum EntityType` dans switch dispersés.

**Décision :** Enum supprimé. `TypeId` + `EntityRegistry`.

**Conséquences :** Plugins ajoutent des entités.

---

## 004 : Persistence par Serializer Registry

**Problème :** Format `.bcad` utilise switch(EntityType).

**Décision :** `IEntitySerializer` par type dans `SerializerRegistry`.

**Conséquences :** Plugins sauvegardent sans modifier le Core.

---

## 005 : Plugin System dynamique

**Problème :** Extensions sans modifier le Core.

**Décision :** `dlopen`/`LoadLibrary`. `bcad_plugin_init(PluginRegistry&)`. `PluginManager`.

**Conséquences :** Extensions sans recompiler. Pas d'ABI stable v1.

---

## 006 : SDK public versionné

**Problème :** Plugin dev a besoin des internals.

**Décision :** `install()` + `BCADConfig.cmake`. Publics dans `include/bcad/`.

**Conséquences :** `find_package(BCAD)` fonctionne.

---

## 007 : Architecture 2D/3D simultanée

**Problème :** BCAD 2D doit évoluer vers 2D/3D.

**Décision :** Types neutres : `Point2`/`Point3`, `Vector2`/`Vector3`, `Transform2`/`Transform3`.

**Conséquences :** Migration 3D progressive.

---

## 008 : Spatial Index indépendant du Renderer

**Problème :** `Quadtree` est dans `render/` mais utilisé par `Document`.

**Décision :** `ISpatialIndex` dans module `index/` (Core). Renderer consomme via interface.

**Conséquences :** Culling/picking sans renderer.

---

## 009 : Command + Transaction primitives

**Problème :** `QUndoCommand` (Qt) lie le Core à Qt.

**Décision :** `Command` + `Transaction` purs C++. `QCommandAdapter` fait le pont.

**Conséquences :** Core indépendant Qt. Transactions atomiques.

---

## 010 : Event Bus typé

**Problème :** `std::function` ad-hoc. Plugins ne peuvent pas s'abonner.

**Décision :** `EventBus` typé via `subscribe<EventT>(handler)`.

**Conséquences :** Découplage fort, type-safe.

---

## 011 : Politique API/ABI v1

**Problème :** ABI stable parfaite coûteuse.

**Décision :** API source stable. ABI binaire PAS garanti v1. Wrapper C en v2.

**Conséquences :** Plugins recompilés à chaque version.

---

## 012 : Licence GPL-3.0

**Problème :** CGAL utilise GPL.

**Décision :** BCAD sous GPL-3.0-or-later. Plugins propriétaires autorisés (liaison dynamique).

**Conséquences :** Compatible CGAL.

---

## 013 : Pas d'abstractions virtuelles inutiles

**Problème :** Tout virtualiser = over-engineering.

**Décision :** Value types privilégiés. `Point2 { double x, y; }` plutôt que `IPoint2`.

**Conséquences :** Code plus simple, perf.

---

## 014 : Couches Core/Services/App/Plugins

**Problème :** Sans structure, monolithe.

**Décision :** Core (std), Services (dépend Core), App (dépend services), Plugins (dépend SDK). Dépendances vers Core.

**Conséquences :** Clarté, testabilité, extensibilité.

---

## 015 : Persistence SQLite + JSON

**Problème :** BCAD utilise SQLite.

**Décision :** SQLite (DDL) + JSON (entité). `schemaVersion()` par serializer.

**Conséquences :** SQLite libre/robuste. JSON lisible.

---

## Ajouter un ADR

```markdown
## 016 : Titre

**Statut :** Proposé
**Date :** YYYY-MM-DD

### Contexte
Quel problème ?

### Décision
Qu'avons-nous choisi ?

### Conséquences
- Positif : ...
- Négatif : ...

### Alternatives
- Option A : ...
```

Un ADR est **immutable** une fois accepté.

---

## Voir aussi

- `ARCHITECTURE.md` — Architecture cible
- `ARCHITECTURE_PRINCIPLES.md` — Principes