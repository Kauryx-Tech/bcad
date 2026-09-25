# Glossaire BCAD

> [!IMPORTANT]
>
> ## Statut : la plupart des termes décrivent l'architecture **en place**
>
> Registries (`EntityRegistry`, `CommandRegistry`, `SerializerRegistry`,
> `WorkbenchRegistry`, `ValidatorRegistry`), `EventBus`, `Command`,
> `ISpatialIndex`, `PropertyMap`, `Plugin`/SDK et `Workbench` sont implémentés
> et testés. Les entrées encore **cibles** sont marquées « cible » dans leur
> définition ; c'est le cas notamment de `EntityId` (au sens `uint64_t`),
> `Point3`, `bcad::document` et `PropertyKey`.
> Le détail de ce qui marche dans le produit, domaine par domaine, est dans
> `CADASTRE_PLUGIN_STATUS.md` et `CADASTRAL_AUDIT_2026.md`, pas ici.

> Définitions centralisées des termes utilisés dans la documentation
> architecturale. Une définition ne fait pas foi contre le code : la source
> d'autorité est le header cité, puis les ADR (voir `AGENTS.md`, « Hiérarchie
> des sources »).

## Termes généraux

### ADR (Architecture Decision Record)
Document qui consigne une décision architecturale importante, son contexte, ses alternatives, et ses conséquences. Voir `ARCHITECTURE_DECISIONS.md`.

### ABI (Application Binary Interface)
Interface binaire entre composants compilés. Contrôle la compatibilité entre plugins compilés et l'application. Voir `API_ABI_POLICY.md`.

### API (Application Programming Interface)
Interface source (headers C++) entre composants. Contrôle la compatibilité à la compilation.

### BCAD Core
Noyau métier de BCAD. Bibliothèque C++20 pure, indépendante de Qt, OpenGL, et du renderer. Voir `ARCHITECTURE.md`.

### BCAD SDK
Surface publique destinée aux développeurs de plugins. Voir `SDK_ARCHITECTURE.md`.

### Plugin
Bibliothèque dynamique (`.so`/`.dll`) qui étend BCAD sans modifier le Core. Voir `PLUGIN_ARCHITECTURE.md`.

---

## Géométrie

> Aujourd'hui, les types géométriques sont des alias CGAL (`using Point2 = Kernel::Point_2`,
> `include/bcad/geometry/Types.h`). Les entrées ci-dessous décrivent les value types **cibles**,
> sauf mention contraire.

### Point2 / Point3
Value type représentant un point 2D ou 3D (cible).
```cpp
// Aujourd'hui : alias CGAL (via Types.h)
using Point2 = Kernel::Point_2;
// Cible : value types POD
struct Point2 { double x, y; };
struct Point3 { double x, y, z; }; // Point3 n'existe pas encore
```

### Vector2 / Vector3
Value type représentant un vecteur 2D ou 3D.
```cpp
struct Vector2 { double x, y; };
struct Vector3 { double x, y, z; };
```

### Transform2 / Transform3
Opaque type représentant une transformation (translation, rotation, scale, mirror). Non virtual, value-like.

### BoundingBox2 / BoundingBox3
Rectangle délimitant une entité.
```cpp
struct BoundingBox2 { double minX, minY, maxX, maxY; };
struct BoundingBox3 { double minX, minY, minZ, maxX, maxY, maxZ; };
```

### Kernel CGAL
`CGAL::Exact_predicates_inexact_constructions_kernel`. Prédicats exacts (robustes), constructions en double précision (rapides).

### Tessellation
Conversion d'une entité géométrique exacte en maillage de triangles/lignes pour le rendu.

### LOD (Level of Detail)
Adaptation de la tessellation selon le zoom. Plus dézoomé = moins de segments.

### BooleanOp
Opération booléenne sur polygones : Union, Intersection, Difference, SymmetricDifference.

---

## Document et Entity

### Document
Modèle en mémoire d'un dessin : entités, calques, et l'index spatial tenu
synchronisé avec les deux (`bcad::core::Document`, `include/bcad/core/Document.h`).
Il publie les événements typés sur l'`EventBus`, rend `buildTessellation()`
appelable depuis un thread d'arrière-plan, et sérialise via la
`SerializerRegistry`. Thread-safe par verrou partagé/exclusif interne.
Ce qu'il ne fait **pas** : la sélection (vécue par l'UI), les transactions
(pilotes par les `Command`), les formats de fichier (module `io` et exporters
de modules).

### Entity
Objet géométrique dans le Document. Interface pure : une entité doit livrer
`typeId()`, `boundingBox()`, `applyTransform()`, `clone()`, `tessellate()`,
`distanceTo()`, `serializeParams()`, `writeDxf()`, `geometryInfo()`, son
`PropertyMap` et ses points d'accrochage. Identifiée par `id()` et un `TypeId`.

### EntityId *(cible, non implémenté)*
Alias documenté `using EntityId = uint64_t`. Dans le code, `Entity::id()` rend
un `int` attribué séquentiellement par le Document — c'est ce `int` que circulent
les commandes, l'index, les sérialiseurs et les `Diagnostic`. Passer à
`uint64_t` est une modification d'ABI (ADR-011).

### TypeId
Identifiant de type d'entité : une **chaîne stable**, comparée et hachée telle
quelle (`include/bcad/geometry/TypeId.h`).
```cpp
struct TypeId { std::string value; };   // "bcad.Line", "cadastre.parcel"
```
L'espace de noms est une convention d'écriture dans la chaîne, pas un champ
distinct, et il n'y a pas de GUID : la stabilité de la valeur est la garantie,
documentée dans `TYPEID_STABILITY.md`. C'est la clé de `EntityRegistry` et de
`SerializerRegistry` (ADR-003, ADR-004).
### EntityRegistry
Registre dynamique des types d'entités. Permet l'ajout de nouvelles entités par les plugins.

### SerializerRegistry
Registre dynamique des sérialiseurs. Permet la persistence de nouvelles entités par les plugins.

### Layer / LayerManager
Calque graphique. Propriétés : nom, couleur, épaisseur, visibilité, verrouillage.

### ISpatialIndex
Interface d'indexation spatiale (`include/bcad/index/ISpatialIndex.h`) :
`insert`, `remove`, `update`, `query(region)`. Un seul backend implémenté,
`QuadtreeIndex` (2D) ; octree, R-tree et BVH sont des pistes, pas du code.
C'est par cette interface que le core reste indépendant du rendu (ADR-008) et
qu'un dessin se culle sans parcourir toutes les entités.

### PropertyMap
Map typée de propriétés dynamiques attachées à une entité. Types : Double, Int, String, Bool, Color, Enum.

---

## Commandes et Transactions

### Command
Unité atomique de travail avec execute() et undo(). Voir `COMMAND_SYSTEM.md`.

### Transaction
Groupe de Commands exécutables/annulables comme une seule unité.

### CommandRegistry
Registre dynamique des commandes. Permet l'enregistrement par les plugins.

---

## Événements

### EventBus
Bus d'événements typé. Abonnement via `subscribe<EventT>(handler)`. Voir `EVENT_SYSTEM.md`.

### Event
Struct C++ typé (POD-like). Exemples : EntityAddedEvent, EntityModifiedEvent, SelectionChangedEvent.

---

## Plugins

### PluginManager
Gère la découverte, le chargement, l'initialisation et le déchargement des
modules (`src/plugin/PluginManager.cpp`). `discoverPluginPaths()` énumère les
répertoires de modules, `loadAllDiscovered()` les charge tous : l'hôte n'a donc
à nommer aucun module (ADR-016).

### PluginRegistry
Objet passé à `bcad_plugin_init()`, porteur des six points d'enregistrement :
`registerEntityType`, `registerCommand`, `registerSerializer`,
`registerWorkbench`, `registerValidator`, `registerFileExporter`. Il donne aussi
au module l'accès à ses valeurs réglables (`resolveDataFile`).

### Gabarit (données réglables d'un module)
Fichier JSON posé dans `share/bcad/plugins/<module>/`, que le module atteint par
`PluginRegistry::resolveDataFile("<module>/…")` et non par un chemin qu'il
devinerait. Un profil illisible ou absent laisse le module à ses valeurs par
défaut. C'est le canal des profils par pays (`ROADMAP_MARKET.md` étape 3).

### bcad_plugin_init
Symbole exporté par chaque plugin (ADR-005) :
```cpp
extern "C" bool bcad_plugin_init(PluginRegistry& reg);
```
Remplit `reg.info()` puis enregistre ses extensions ; retourne `false` pour
faire échouer le chargement — l'hôte annule alors ce qui a déjà été déclaré.

### ABI des modules (`PLUGIN_API_VERSION`)
Entier comparé **à l'égalité** au chargement (`include/bcad/plugin/PluginRegistry.h`).
Un module plus ancien ou plus récent est refusé : le dépôt ne garantit pas
l'ABI d'une version à l'autre (ADR-011), les modules sont recompilés.

---

## Validation

### IValidator
Interface d'une règle de vérification déclarée par un module
(`include/bcad/plugin/Validator.h`) : `id()`, `label()`, `applicableTypes()`,
`validate(entities)`. Le seul point d'entrée par lequel une règle métier est
déclenchée depuis l'application sans que celle-ci connaisse le métier.

### ValidatorRegistry
Registre global des `IValidator`, singleton porté par `libbcad_plugin.so`
(partagé hôte et modules), alimenté par `PluginRegistry::registerValidator()`
et vidé avant le `dlclose` du module.

### Diagnostic
Résultat d'une règle : `{ Severity severity; std::string message;
std::vector<int> entityIds; }` (`include/bcad/validation/Diagnostics.h`).
`entityIds` désigne les entités en cause ; l'UI les sélectionne au
double-clic.

### Severity
`Info`, `Warning`, `Error`. Un `Error` est ce qui empêche un document d'être
publiable ; la nuance appartient au module qui déclare la règle, pas à l'hôte.

---

## Rendu

### GlRenderer
`include/bcad/render/GlRenderer.h` — OpenGL 3.3 core, vit dans le thread qui
possède le contexte ; ne consomme que des batches de polylignes tessellées.
**Il n'y a pas d'interface `IRenderBackend`** : un second backend (Vulkan)
demande une ADR, pas une abstraction anticipée (ADR-013).

### Camera2D
Transformation monde → écran, pan/zoom ; source du `pixelsPerUnit` dont dépend
le niveau de détail.

### TessellationWorker
`src/app/TessellationWorker.cpp` — `QThread` dédié qui appelle
`Document::buildTessellation(region, tolerance)` hors du thread GL et rend le
résultat par signal.

### SceneExtractor *(cible, non implémenté)*
Le rôle est tenu aujourd'hui par la requête de l'index spatial dans
`Document::buildTessellation` ; la classe du même nom n'existe pas.

---

## Systèmes de coordonnées

### WCS (World Coordinate System)
Repère global du document. 2D : (X, Y), Z=0. 3D : (X, Y, Z).

### UCS (User Coordinate System)
Repère défini par l'utilisateur dans l'UI.

---

## Workbench

### Workbench
Groupe d'outils, commandes, et ressources pour un domaine métier.
**Implémenté** (lot A de `WORKBENCH.md`) sous sa forme déclarative : un plugin
décrit ses panneaux et actions, l'hôte en fait des menus et des rubans.

### WorkbenchRegistry
Registre des workbenches disponibles (`include/bcad/plugin/Workbench.h`),
singleton **porté par `libbcad_plugin.so`** — la bibliothèque liée par l'hôte et
par les modules, raison pour laquelle les deux voient le même conteneur sans
`-rdynamic`. Alimenté par `PluginRegistry::registerWorkbench()`, vidé avant le
`dlclose`.

---

## Voir aussi

- `ARCHITECTURE_PRINCIPLES.md` — principes architecturaux
- `ARCHITECTURE_DECISIONS.md` — décisions architecturales
- `ARCHITECTURE_BENCHMARK.md` — comparaison CAO
