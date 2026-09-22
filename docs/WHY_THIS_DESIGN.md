# Pourquoi ces choix architecturaux ?

> Ce document explique le **pourquoi** derrière les décisions de BCAD.

## Sommaire

1. [Pourquoi C++20 ?](#1-pourquoi-c20-)
2. [Pourquoi Qt ?](#2-pourquoi-qt-)
3. [Pourquoi CGAL ?](#3-pourquoi-cgal-)
4. [Pourquoi un Core indépendant ?](#4-pourquoi-un-core-indépendant-)
5. [Pourquoi des Registries ?](#5-pourquoi-des-registries-)
6. [Pourquoi des plugins dynamiques ?](#6-pourquoi-des-plugins-dynamiques-)
7. [Pourquoi GPL-3.0 ?](#7-pourquoi-gpl-30-)
---

## 1. Pourquoi C++20 ?

**Décision :** C++20, pas C++17 ou C++23.

**Raisons :**
- `std::span`, `std::optional`, `std::variant` natifs
- Concepts pour templates
- `constexpr` amélioré

**Pas C++17 :** Pas assez moderne.

**Pas C++23 :** Compatibilité compilateurs limitée.

---

## 2. Pourquoi Qt ?

**Décision :** Qt6 (Widgets, OpenGLWidgets).

**Raisons :**
- Mature, stable


---

## 3. Pourquoi CGAL ?

**Décision :** CGAL pour la géométrie.

**Raisons :**
- Prédicats **exacts** (critique en CAO)
- Bibliothèques spécialisées
- Standard CAO académique

**Coût :** Licence GPL.

**Alternatives :**
- **Eigen** : algèbre linéaire seulement
- **Boost.Geometry** : moins complet
- **OpenCascade** : trop lourd

---

## 4. Pourquoi un Core indépendant ?

**Décision :** Le Core ne dépend ni de Qt, ni d'OpenGL.
---

## 5. Pourquoi des Registries ?

**Décision :** `EntityRegistry`, `CommandRegistry`, `SerializerRegistry`.

**Raisons :**
- **Open/Closed** : ouvert à l'extension, fermé à la modification
- Les plugins ajoutent sans toucher au Core
- Plus de `switch(type)` dispersés

**Alternative considérée :** Enum `EntityType`. Refusé car non extensible.

---

## 6. Pourquoi des plugins dynamiques ?

**Décision :** `dlopen`/`LoadLibrary` + `bcad_plugin_init`.

**Raisons :**
1. **Découplage** : le Core ignore les plugins
2. **Distribution indépendante** : l'utilisateur installe uniquement ce dont il a besoin
3. **Pas de recompilation** : ajout sans rebuild
4. **Multi-domaine** : architecture, civil, mécanique peuvent coexister

**Coût :** Pas d'ABI binaire stable v1.

---

## 7. Pourquoi GPL-3.0 ?

**Décision :** GPL-3.0-or-later pour BCAD.

**Raisons :**
1. CGAL est sous GPL → cohérence
2. Copyleft fort protège la communauté
3. Plugins peuvent être propriétaires (liaison dynamique)

**Alternative :** LGPL. Refusé car limiterait la protection copyleft.

---

## 8. Pourquoi value types plutôt qu'interfaces ?

**Décision :** `Point2 { double x, y; }` plutôt que `IPoint2`.

**Raisons :**
- **Performance** : pas de virtual call
- **Simplicité** : pas de header abstrait
- **Compatibilité ABI** : un POD est toujours compatible
- **Sémantique** : un point EST une paire de doubles

---

## 9. Pourquoi un système de transactions ?

**Décision :** `Transaction` regroupe plusieurs `Command`.

**Raisons :**
- **Atomicité** : tout ou rien
- **Undo/redo groupé**
- **Cohérence** : pas d'état intermédiaire

---

## 10. Pourquoi des Events typés ?

**Décision :** `EventBus::subscribe<EntityAddedEvent>(handler)`.

**Raisons :**
- **Type-safety** : le compilateur vérifie
- **Performance** : dispatch O(1)
- **Découplage**

---

## 11. Pourquoi SQLite pour les fichiers .bcad ?

**Décision :** SQLite comme format natif.

**Raisons :**
1. Pas de dépendance externe
2. Performance (index, requêtes)
3. Atomicité
4. Maturité
5. Inspectable avec un outil SQLite

---

## 12. Pourquoi préparer la 3D maintenant ?

**Décision :** `Point3`, `Vector3`, `Transform3` au programme — **cible**. Aujourd'hui seuls
`Point2`/`Vector2` (alias CGAL, `include/bcad/geometry/Types.h`) et `Transform2D` existent ;
la 3D n'est pas encore engagée.

**Raisons :**
1. Éviter la dette technique
2. Migration progressive
3. Pas d'overhead significatif

---

## 13. Pourquoi exposer les violations ADR ?

**Décision :** Documenter les violations au lieu de les cacher.

**Raisons :**
1. Transparence
2. Plan clair
3. Honnêteté

---

## Voir aussi

- [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md)
- [ARCHITECTURE_DECISIONS.md](ARCHITECTURE_DECISIONS.md)
- [ARCHITECTURE_BENCHMARK.md](ARCHITECTURE_BENCHMARK.md)

**Raisons :**
1. **Testabilité** : testable sans fenêtre
2. **Substitution** : autre UI possible
3. **Headless** : serveurs BCAD possibles
4. **Plugins** : pas de Qt embarqué

**Coût :** Indirection via interfaces.
- Excellente intégration OpenGL
- Multiplateforme
- Signaux/slots

**Alternatives :**
- **wxWidgets** : moins de fonctionnalités
- **GTK** : pas idéal Windows/macOS
- **Dear ImGui** : trop basique
- **Electron** : trop lourd pour CAO