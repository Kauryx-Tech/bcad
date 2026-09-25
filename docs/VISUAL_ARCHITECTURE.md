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
   │  modules : BCAD_PLUGIN_PATH · applicationDir/../lib/bcad/plugins
   │            · arbre de build · $XDG_DATA_HOME/bcad/plugins
   │  données : BCAD_PLUGIN_DATA · applicationDir/../share/bcad/plugins
   │            · arbre de build · $XDG_DATA_HOME/bcad/plugins
   ▼
PluginManager::loadAllDiscovered()        (aucun nom de module cité)
   │  dlopen ──► bcad_plugin_api_version()  == PLUGIN_API_VERSION ? sinon refus
   │          ──► bcad_plugin_init(PluginRegistry&)
   │                  │ le registre porte d'abord les répertoires de données :
   │                  │ le module y lit ses gabarits avant de s'enregistrer
   │                  │ false → rollback complet des enregistrements du module
   │                  ▼
   │              registres globaux portés par libbcad_plugin
   ▼
menu/panneaux construits à partir de ce qui est déclaré
```

### 5.2 Les six points d'extension

```
PluginRegistry
 ├─ registerEntityType(TypeId, factory)   → EntityRegistry       (ADR-003)
 ├─ registerCommand(name, factory)        → CommandRegistry      (ADR-009)
 ├─ registerSerializer(unique_ptr<IEntitySerializer>) → SerializerRegistry (ADR-004)
 ├─ registerWorkbench(unique_ptr<IWorkbench>)         → WorkbenchRegistry  (ADR-016)
 ├─ registerValidator(unique_ptr<IValidator>)         → ValidatorRegistry  (ADR-016)
 └─ registerFileExporter(unique_ptr<IFileExporter>)   → FileExporterRegistry (ADR-016)
```

`IValidator` est le seul chemin par lequel une règle de vérification d'un
domaine est déclenchée sans que l'hôte la connaisse : il rend des
`validation::Diagnostic{severity, message, entityIds}`, affichés dans le dock
`Vérifications`, et le double-clic sélectionne les entités en cause.

Le registre n'est pas qu'un guichet d'enregistrement : il donne aussi au module
l'accès à ses **valeurs réglables** — `addDataDirectory` /
`resolveDataFile("<module>/…")`, chaînon entre les répertoires annoncés par
l'hôte et les gabarits JSON lus par le module (voir §2 et `PLUGIN_ARCHITECTURE.md` §9).

### 5.3 Cycle de vie, côté destructeur

```
unloadPlugin ──► retrait des TypeId/noms déclarés par CE module
             ──► destruction des workbenches, validateurs, serializers,
                  exporteurs
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
                 l'hôte ne sait que rassembler des paramètres selon une
                 stratégie générique (WorkbenchParams) ; il ne pousse la
                 commande dans QUndoStack que si l'action déclare
                 `modifiesDocument`
```

C'est cette table, pas un `if (module == "...")`, qui remplit le ruban.

### 6.3 Une classe, plusieurs unités de traduction

`MainWindow` est une seule classe dont les corps sont répartis sur cinq
fichiers de `src/app/`, par responsabilité :

| Fichier | Contenu |
|---------|---------|
| `MainWindow.cpp` | constructeur, docks, ligne de commande, thème |
| `MainWindowTools.cpp` | les 20 outils interactifs et leur table `kTools` |
| `MainWindowMenus.cpp` | menus de l'hôte et panneaux du ruban |
| `MainWindowPlugins.cpp` | workbenches, validateurs, exporteurs des modules |
| `MainWindowDocument.cpp` | document, fichiers, autosauvegarde, impression |

Trois règles tiennent ce découpage :

- **L'en-tête `Q_OBJECT` reste unique** (`src/app/MainWindow.h`) : le moc ne
  voit que lui, et `CMAKE_AUTOMOC` n'a pas à connaître la répartition.
- **Un symbole partagé passe par un en-tête interne** (`src/app/ActionIcons.h`,
  non installé, hors SDK). Jamais une table : `const ToolSpec kTools[]` exposée
  dans un en-tête reprendrait une copie par unité de traduction, sans erreur de
  compilation, et la synchronisation menu/ruban serait perdue. La table reste
  donc dans l'espace de nommage anonyme de `MainWindowTools.cpp`, et
  `onToolChanged` — son seul autre lecteur — vit dans le même fichier.
- **L'ordre affiché vient de la séquence d'appels du constructeur**, pas de
  l'ordre des fichiers : `buildToolActions` → `buildMenusAndRibbon` →
  `buildDockWidgets`.

`scripts/check_arch.sh` (11bis) refuse tout `src/app/MainWindow*.cpp` de plus de
350 lignes. La limite ne porte pas sur `Viewport.cpp` (1318 lignes) : son
découpage est un chantier à part.

### 6.4 Ce qui est public et ce qui ne l'est pas

La structure de l'arborescence est une déclaration, pas une suggestion :

```text
include/bcad/   API publique : installée, exportée par le SDK, contractuelle
src/            implémentation privée de l'hôte, y compris src/app/ (Qt)
src/plugins/    extensions qui ne dépendent que de l'API publique
```

Un en-tête sous `include/bcad/` est donc une promesse faite à tous les
consommateurs du SDK : il doit être linkable, stable, et sans dépendance qu'on
ne veut pas porter (d'où ADR-002 pour CGAL et ADR-009 pour Qt, vérifiés sur
`include/bcad/` **entier**, sans exception pour un sous-répertoire).

`include/bcad/app/` contrevenait à cette règle : l'interface Qt de l'hôte y
était installée comme API publique alors que le binaire `bcad` n'est pas exporté
(`install(TARGETS …)`, `CMakeLists.txt:46-49`), qu'aucun consommateur du SDK ne
peut la lier, et que ses `#include <Qt…>` passaient sous la garde ADR-009
puisque celle-ci ne scannait que `include/bcad/core/`. Les dix en-têtes sont
désormais dans `src/app/`, à côté de leurs `.cpp`, inclus par nom
(`#include "Viewport.h"`) comme `ActionIcons.h` l'était déjà. Un plugin n'a
donc plus accès qu'aux contrats d'extension (`bcad/plugin/*`, `bcad/core/*`,
`bcad/registry/*`, `bcad/serialization/*`) — c'est ce qu'on veut dire quand on
annonce l'hôte extensible.

Conséquences vérifiées : `scripts/prove_sdk.sh` installe dans un répertoire
tampon **vidé au préalable** et échoue si `include/bcad/app` réapparaît
(`cmake --install` n'efface jamais ce qui n'est plus produit, donc une
installation antérieure aurait masqué la régression) ; `tests/smoke_test.cpp`
atteint le parseur de coordonnées par un chemin d'inclusion explicite vers
`src/app`, ce qui reste un accès de test à du privé, pas une API.

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
