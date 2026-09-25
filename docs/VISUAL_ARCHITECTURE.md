# Architecture visuelle BCAD

> Schémas ASCII de l'architecture **telle qu'elle est dans le code**. Chaque
> bloc nomme les classes réellement définies quelque part dans `include/` ou
> `src/` ; les flèches sont les dépendances de compilation, pas des souhaits.
> Les règles rappelées ici sont contrôlées par `scripts/check_arch.sh`.

## Table des matières

1. [Vue d'ensemble](#1-vue-densemble)
2. [Flux de données](#2-flux-de-données)
3. [Graphe de dépendances](#3-graphe-de-dépendances)
4. [Cycle de vie d'une entité](#4-cycle-de-vie-dune-entité)
5. [Système de plugins](#5-système-de-plugins)
6. [Interface utilisateur](#6-interface-utilisateur)
7. [Architecture du rendu](#7-architecture-du-rendu)

---

## 1. Vue d'ensemble

```
┌──────────────────────────────────────────────────────────────────┐
│  bcad (Qt6)                         src/app/                     │
│  ┌────────────┐ ┌───────────┐ ┌────────────┐ ┌────────────────┐  │
│  │ Menus +    │ │ Viewport  │ │ Docks      │ │ Ligne de       │  │
│  │ ruban      │ │ (QOpenGL  │ │ Calques,   │ │ commande       │  │
│  │ (MainWindow│ │  Widget)  │ │ Propriétés,│ │ + barre d'état │  │
│  │  RibbonBar)│ │           │ │ Vérific... │ │                │  │
│  └─────┬──────┘ └─────┬─────┘ └─────┬──────┘ └───────┬────────┘  │
│        └──────────────┴─────────────┴────────────────┘           │
└───────────────────────────────┬──────────────────────────────────┘
                                │ uniquement des types du SDK
┌───────────────────────────────▼──────────────────────────────────┐
│  Core + services            src/core, geometry, layers, index,   │
│  (aucun #include <Qt...>,     events, properties, registry,      │
│   aucun nom de métier)        serialization, commands, io, render│
└───────────────────────────────▲──────────────────────────────────┘
                                │ médiation par registres partagés
┌───────────────────────────────┴──────────────────────────────────┐
│  Modules dynamiques (dlopen)                     src/plugins/    │
│  cadastral aujourd'hui ; un domaine = un module, jamais dans le  │
│  core (ADR-016)                                                  │
└──────────────────────────────────────────────────────────────────┘
```

Trois bibliothèques portent la frontière d'ABI : `bcad_geometry` (et le
reste du core) en statique, `bcad_plugin` en partagé — c'est elle qui
exporte les registres globaux, donc le module et l'hôte voient le **même**
conteneur.

---

## 2. Flux de données

### 2.1 Du clic à l'entité persistée

```
clic / saisie clavier
      │
      ▼
Viewport ── outil actif (ToolMode) ── snaps (SnapEngine)
      │  points d'ancrage calculés en coordonnées monde
      ▼
Command / Transaction ── pushée dans le QUndoStack (QtCommandAdapter)
      │  commande pure C++ (ADR-009), aucun type Qt
      ▼
Document::addEntity ──► index_.insert ──► EventBus.publish(EntityAdded)
      │                                        │
      ▼                                        ▼
sauvegarde .bcad (SQLite)                 docks + canevas rafraîchis
   via SerializerRegistry
```

### 2.2 Undo / redo

```
QUndoStack (Qt, thread GUI)
   │  push(cmd.clone())
   ▼
QtCommandAdapter ──► bcad::commands::Command ──► Document
   undo() ──► cmd->undo(doc) ──► index_.remove + EntityRemoved
   redo() ──► cmd->execute(doc) ── réutilise la MÊME commande C++
```

Une commande de plugin arrive dans la même pile : `WorkbenchAction` est une
stratégie déclarée par le module, exécutée par l'hôte (`MainWindow.cpp`),
qui ne connaît que le type des paramètres à rassembler.

---

## 3. Graphe de dépendances

### 3.1 Réel

```
                    ┌─────────┐
                    │   app   │  (Qt)
                    └────┬────┘
        ┌────────────┬───┴────┬───────────┬────────────┐
        ▼            ▼        ▼           ▼            ▼
   ┌────────┐  ┌─────────┐ ┌────────┐ ┌──────────┐ ┌──────────┐
   │  core  │  │ render  │ │   io   │ │  plugin  │ │  layout  │
   └───┬────┘  └────┬────┘ └───┬────┘ └────┬─────┘ └────┬─────┘
       │            │          │           │            │
       └────────────┴──────────┴─────┬─────┴────────────┘
                                     ▼
                       geometry · layers · index · events
                          properties · registry
                       serialization · commands
```

- `core → render` : **aucune**. `Document` ne manipule que
  `index::ISpatialIndex` et des données (ADR-001).
- `geometry` → CGAL : confiné à `src/geometry/*.cpp` et
  `src/geometry/detail/` ; `include/bcad/geometry/` n'expose aucun type CGAL
  (ADR-002).
- `commands`, `events`, `properties` → Qt : **aucun** (ADR-009).
- `app`/`core` → un nom de métier ou de module : **aucun** (ADR-016,
  `check_arch.sh` §10bis).

Les anciennes violations documentées ici (dépendance `core → render`,
`EntityType` enum comme type d'extension) sont corrigées ; l'enum ne subsiste
que par la règle de rétrocompatibilité de `TYPEID_STABILITY.md`.

### 3.2 Qui vérifie

```
scripts/check_arch.sh ──► includes <Qt...> dans include/bcad/
                       ──► types CGAL exposés
                       ──► dépendances core → render
                       ──► littéraux métier / noms de modules dans src/app
                       ──► points d'entrée bcad_plugin_init des modules
```

Le script sort en code 1 au premier type de violation comptabilisée : une
documentation qui contredit le script a tort.

---

## 4. Cycle de vie d'une entité

```
1. CRÉATION
   outil (Viewport) ou commande de module
        │  factory : EntityRegistry::create(TypeId, params)
        ▼
2. IDENTITÉ
   id() attribué par Document · typeId() porté par la classe
   calque + couleur "ByLayer" éventuelle
        ▼
3. INTÉGRATION
   Document::addEntity ──► boundingBox() ──► ISpatialIndex::insert
                        ──► EventBus EntityAdded
        ▼
4. AFFICHAGE
   tessellation au LOD courant (voir §7) — l'entité ne sait que
   produire des polylignes : tessellate(maxDeviation)
        ▼
5. MODIFICATION
   applyTransform(t) / propriétés (PropertyMap)
        │  index_.update + EntityModified
        ▼
6. PERSISTANCE
   SerializerRegistry (clé = TypeId) ──► colonne du .bcad SQLite
   DXF : writeDxf()   GeoJSON/CSV : exporter du module
        ▼
7. SUPPRESSION
   Commande::undo ou removeEntity(id) ──► index_.remove ──► EntityRemoved
```

---

## 5. Système de plugins

### 5.1 Découverte et chargement

```
MainWindow
   │  BCAD_PLUGIN_PATH · applicationDir/../lib/bcad/plugins
   │  · arbre de build · $XDG_DATA_HOME/bcad/plugins
   ▼
PluginManager::loadAllDiscovered()        (aucun nom de module cité)
   │  dlopen ──► bcad_plugin_api_version()  == PLUGIN_API_VERSION ? sinon refus
   │          ──► bcad_plugin_init(PluginRegistry&)
   │                  │ false → rollback complet des enregistrements du module
   │                  ▼
   │              registres globaux portés par libbcad_plugin
   ▼
menu/panneaux construits à partir de ce qui est déclaré
```

### 5.2 Les cinq points d'extension

```
PluginRegistry
 ├─ registerEntityType(TypeId, factory)   → EntityRegistry       (ADR-003)
 ├─ registerCommand(name, factory)        → CommandRegistry      (ADR-009)
 ├─ registerSerializer(unique_ptr<IEntitySerializer>) → SerializerRegistry (ADR-004)
 ├─ registerWorkbench(unique_ptr<IWorkbench>)         → WorkbenchRegistry  (ADR-016)
 └─ registerValidator(unique_ptr<IValidator>)         → ValidatorRegistry  (ADR-016)
```

`IValidator` est le seul chemin par lequel une règle de vérification d'un
domaine est déclenchée sans que l'hôte la connaisse : il rend des
`validation::Diagnostic{severity, message, entityIds}`, affichés dans le dock
`Vérifications`, et le double-clic sélectionne les entités en cause.

### 5.3 Cycle de vie, côté destructeur

```
unloadPlugin ──► retrait des TypeId/noms déclarés par CE module
             ──► destruction des workbenches, validateurs, serializers
             ──► dlclose                      ← après, plus aucun code du
                                                 module n'est atteignable
```

Un objet construit dans le DSO et stocké dans un registre global qui ne le
détruit pas **avant** `dlclose` fait un SEGV : sa vtable pointe de la mémoire
libérée. Pour la même raison, `~PluginManager` ne dlclose rien : à la sortie du
programme, l'ordre de destruction des registres globaux n'est pas défini.

---

## 6. Interface utilisateur

### 6.1 Structure de la fenêtre

```
┌──────────────────────────────────────────────────────────────────────┐
│ Fichier │ Édition │ Affichage │ Dessin │ Modifier │ Cotation │ Calque│
│         │ Outils  │ Aide      │ <un menu par module chargé>         │
├──────────────────────────────────────────────────────────────────────┤
│ Ruban : Accueil │ Modifier │ Affichage │ Annoter │ <onglet par module>│
│         (panneaux « Dessin », « Sélection », « Accrochage », ...)    │
├───────────┬──────────────────────────────────────────────┬───────────┤
│ Calques   │                                              │ Propriétés│
│ (recherche│            Viewport — canevas OpenGL 3.3     │ (colonnes │
│  par nom) │                                              │  génériques)
│           │                                              │ Vérifica- │
│           │                                              │  tions    │
├───────────┴──────────────────────────────────────────────┴───────────┤
│ Ligne de commande : nom de commande + arguments                      │
├──────────────────────────────────────────────────────────────────────┤
│ Barre d'état : X, Y · accrochages · messages d'erreur refuser/action  │
└──────────────────────────────────────────────────────────────────────┘
```

Les trois docks sont empilés/ongletés et l'utilisateur peut les fermer ; une
action qui produit un résultat rouvre son dock, sinon le résultat est calculé
et jeté.

### 6.2 D'où vient chaque bouton

```
core           → tools de dessin/modification, cotation, calques (libellés
                 génériques de CAO)
module métier  → IWorkbench::label() + panels() + actions()
                 → menuBar()->addMenu(label)  (MainWindow::buildPluginMenus)
                 → ribbon_->addPanel(label, panel.title, actions)
                 l'hôte ne sait que rassembler des paramètres typés
                 (WorkbenchParams) et pousser la commande dans QUndoStack
```

C'est cette table, pas un `if (module == "...")`, qui remplit le ruban.

---

## 7. Architecture du rendu

```
Document (thread GUI, verrou interne)
   │  Viewport::worldToleranceForZoom(camera.pixelsPerUnit())
   │      = tolérance monde du LOD (src/app/Viewport.cpp:197)
   ▼
TessellationWorker — QThread dédié (src/app/TessellationWorker.cpp)
   doc->buildTessellation(region, tolerance)      [src/core/Document.cpp:153+]
     1. requête de l'index spatial sur la région visible ← le quadtree borne
        le travail
     2. entity->tessellate(tolerance) par entité, filtrée par la visibilité
        du calque
   ▼
core::TessellationResult { batches de couleur, région, tolérance utilisée }
   │  signal queued-connection (traverse le thread grâce au Q_DECLARE_METATYPE)
   ▼
GlRenderer : protected QOpenGLFunctions_3_3_Core
   glBufferData des VBO puis glDrawElements — dans le thread qui possède
   le contexte, donc jamais depuis le worker
   │
   ▼
Camera2D (pan/zoom, monde → écran) + Grid
```

Le rendu ne demande donc jamais à une entité de se dessiner elle-même : il ne
consomme que des polylignes tessellées, ce qui est ce qui garde un type d'entité
publié par un module affichable sans code de rendu dédié.

---

## Voir aussi

| Document | Description |
|----------|-------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Documentation textuelle |
| [ARCHITECTURE_DECISIONS.md](ARCHITECTURE_DECISIONS.md) | ADR |
| [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md) | Principes |
| [PLUGIN_ARCHITECTURE.md](PLUGIN_ARCHITECTURE.md) | Détails plugins et SDK |
| [WORKBENCH.md](WORKBENCH.md) | UI déclarée par un module |
| [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) | Détails rendu |
| [CADASTRE_PLUGIN_STATUS.md](CADASTRE_PLUGIN_STATUS.md) | Ce qu'un module fournit vraiment |
| `docs/schemas/` | Fichiers Draw.io (non régénérés : peuvent diverger) |
