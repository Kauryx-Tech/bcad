# Glossaire BCAD

> Définitions centralisées des termes techniques utilisés dans la documentation architecturale.

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

### Point2 / Point3
Value type représentant un point 2D ou 3D.
```cpp
struct Point2 { double x, y; };
struct Point3 { double x, y, z; };
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
Modèle central de BCAD. Possède les entités, calques, sélection, index spatial, transactions, événements. Thread-safe via shared_mutex.

### Entity
Objet géométrique dans le Document. Identifié par un `EntityId` (uint64_t) et un `TypeId`.

### EntityId
Identifiant unique d'une entité dans un Document. `uint64_t`, généré séquentiellement.
```cpp
using EntityId = uint64_t;
constexpr EntityId kInvalidEntityId = 0;
```

### TypeId
Identifiant de type d'entité. Composé de :
```cpp
struct TypeId {
    std::string name;        // "wall", "line"
    std::string namespace_;  // "architecture", "core"
    Guid guid;               // identifiant stable
};
```
### EntityRegistry
Registre dynamique des types d'entités. Permet l'ajout de nouvelles entités par les plugins.

### SerializerRegistry
Registre dynamique des sérialiseurs. Permet la persistence de nouvelles entités par les plugins.

### Layer / LayerManager
Calque graphique. Propriétés : nom, couleur, épaisseur, visibilité, verrouillage.

### ISpatialIndex
Interface abstraite pour l'indexation spatiale. Backends : Quadtree (2D), Octree (3D), R-tree, BVH.

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
Gère la découverte, le chargement, l'initialisation, et le shutdown des plugins.

### PluginRegistry
Registre passé à bcad_plugin_init(). Permet d'enregistrer types, commandes, serializers.

### bcad_plugin_init
Symbole exporté par chaque plugin :
```cpp
extern "C" void bcad_plugin_init(PluginRegistry& reg);
```

---

## Rendu

### IRenderBackend
Interface abstraite pour le moteur de rendu. Backends : OpenGLBackend, VulkanBackend (futur).

### SceneExtractor
Extrait les entités visibles d'un Document pour le rendu. Utilise ISpatialIndex pour le culling.

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

### WorkbenchRegistry
Registre des workbenches disponibles.

---

## Voir aussi

- `ARCHITECTURE_PRINCIPLES.md` — principes architecturaux
- `ARCHITECTURE_DECISIONS.md` — décisions architecturales
- `ARCHITECTURE_BENCHMARK.md` — comparaison CAO
