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
| 016 | Plateforme cible et principes de conception | Accepté |

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
La surface publique est définie **par le répertoire** : ce qui est sous
`include/bcad/` est installé, exporté et contractuel ; ce qui est sous `src/`
est privé, y compris `src/app/` (l'interface Qt de l'hôte). Placer un en-tête
d'hôte sous `include/bcad/` créerait une API publique non linkable — le binaire
`bcad` n'est pas un objectif `EXPORT`é.

**Conséquences :** `find_package(BCAD)` fonctionne. Un plugin ne dépend que des
contrats d'extension (`bcad/plugin/*`, `bcad/core/*`, `bcad/registry/*`,
`bcad/serialization/*`), jamais de `MainWindow.h` ni `Viewport.h`. La preuve
`scripts/prove_sdk.sh` installe dans un répertoire tampon vidé et refuse toute
réapparition d'un en-tête d'hôte dans l'installation.

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
La règle portée est « aucun type Qt dans une API publique », pas « Qt interdit
dans le core seulement » : `check_arch.sh` (§4) scanne `include/bcad/` entier.
L'interface Qt de l'hôte n'a donc rien à y faire et vit dans `src/app/` (ADR-006).

**Conséquences :** Core indépendant Qt. Transactions atomiques. Un `#include <Qt…>`
dans un en-tête public échoue en CI, quel que soit son emplacement.

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

## 016 : Plateforme cible et principes de conception

**Statut :** Accepté
**Date :** 2026-09-24

### Contexte

Aucun document ne fixe « sur quel matériel BCAD doit fonctionner, et avec
quelles contraintes d'usage ». La cible retenue est celle du terrain ouest-
africain francophone (`ROADMAP_MARKET.md` §1) : parcs de postes anciens à
mémoire limitée et GPU intégré, connexion internet rare ou payante au volume,
usage en français avec des unités métriques, livrable attendu = document
administratif imprimable, et formation CAO limitée (prise en main « sur le
tas »). Sans invariant écrit, chaque fonctionnalité nouvelle peut, sans
qu'on s'en aperçoive, rendre l'outil inutilisable sur le matériel visé.

### Décision

**Plateforme cible (critères mesurables) :**

| Critère | Cible |
|---|---|
| Mémoire vive minimale | 2 Go (32/64 bits), 4 Go recommandés |
| GPU | optionnel ; OpenGL 3.3 si disponible, sinon repli logiciel |
| Fichier de dessin typique | 50 000 entités manipables sans attente perceptible |
| Sortie imprimable | A4 → A0, PDF vectoriel, imprimante locale |
| Réseau | **non requis** à aucune étape, du premier tracé au PDF |
| Langue | français, unités métriques (`m`, `m²`, `ha`) |

**Principes de conception qui en découlent :**

1. **Hors-ligne d'abord** — aucune dépendance à un service, un compte, une
   carte en ligne ou une mise à jour à chaud. Aide, gabarits, symboles et
   notices sont embarqués.
2. **Léger mesurable** — binaire compact (dépouillé et optimisé, objectif
   ≤ 30 Mo), démarrage instantané, pas de runtime lourd ni de machine
   virtuelle ; les dépendances natives restent indispensables et déclarées.
3. **Le livrable est un document** — une fonctionnalité n'est achevée que
   lorsqu'elle contribue à un plan imprimable conforme (mise en page,
   cartouche, tableau d'attributs), pas seulement à un dessin interactif.
4. **Configurable, pas reprogrammé** — les règles locales (calques, styles,
   cartouches, lexique, tolérances) sont des **données** : templates JSON
   installables et plugins. Le core n'intègre **aucun littéral métier**
   (cf. ADR-003, ADR-005).
5. **Interopérabilité avant l'originalité** — DXF complet et fidèle,
   raccourcis et gestuelle conformes aux habitudes AutoCAD, formats
   d'échange géomatiques ; un utilisateur formé ailleurs doit s'y retrouver.
6. **Une seule façon de faire** — pas de modes redondants ni d'options
   cumulées ; la surface d'apprentissage est une contrainte de produit.

### Conséquences

- Positif : les arbitrages de fonctionnalités ont un critère objectif ;
  la 3D lourde, les services en ligne et les grosses dépendances sont
  écartés sur motif documenté plutôt que par goût.
- Positif : la légèreté et le hors-ligne deviennent un **argument de vente**
  différenciant face aux CAO généralistes, pas une contrainte subie.
- Positif : le point 4 fournit la règle d'extension unique (workbench +
  plugin + gabarit) qui garde `src/app/` indépendant de tout métier. Ce canal
  de gabarit existe : l'hôte annonce des répertoires de données, le module y
  résout ses fichiers (`PluginRegistry::resolveDataFile`, ABI v7) ; le module
  cadastral en lit les motifs d'identification. Les valeurs qui n'ont pas de
  consommateur (styles de calque, tolérance de levé, unités) y restent
  volontairement non lues — lire une donnée sans règle à piloter serait
  réintroduire un littéral métier.
- Négatif : certaines fonctionnalités attendues (rendu réaliste, nuages de
  points, collaboration temps réel, calcul mutualisé) sont hors cible et
  devront être assumées comme telles face à un client.
- Négatif : le mode de rendu logiciel (repli sans OpenGL) est une exigence
  supplémentaire du backend de rendu (`IRenderBackend`, ADR-001), pas encore
  implémentée.
- Négatif : les cibles ci-dessus doivent être **mesurées** à chaque version,
  sinon l'ADR devient décoratif.

### Alternatives

- **Cible « poste récent »** (GPU dédié, 8 Go, en ligne) : refuserait de fait
  le parc existant visé ; écarté.
- **Ne rien figer et arbitrer au cas par cas** : dérive garantie vers un
  outil gourmand, et débats sans critère ; écarté.
- **Appliquette web ou électrons libres** : impose une connexion, un runtime
  lourd et une surface mémoire incompatibles avec la cible ; écarté
  (le rendu WebGPU est un backend possible, pas une plateforme hôte).

---

## Ajouter un ADR

```markdown
## 017 : Titre

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