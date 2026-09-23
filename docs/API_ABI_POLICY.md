# Politique API/ABI BCAD

> Stratégie réaliste pour l'API et l'ABI. Pas de promesse d'ABI stable parfaite en v1.

## 1. Distinction API / ABI

| Terme | Signification |
|-------|---------------|
| **API** | Interface source (headers C++). Casser = recompilation nécessaire. |
| **ABI** | Interface binaire (layout mémoire, vtables, mangling). Casser = re-link nécessaire. |

## 2. Politique API (source)

### 2.1 Compatibilité source

- **Versions mineures (1.x.y)** : pas de cassure source. Ajouts uniquement.
- **Versions majeures (X.y.z)** : cassure possible.
- **Versions de patch (1.0.x)** : pas de cassure, corrections uniquement.

### 2.2 Ce qui peut casser en v majeure

- Suppression de classes publiques
- Renommage de méthodes
- Changement de signature de méthodes
- Restructuration de namespaces
- Suppression d'enums

### 2.3 Ce qui ne casse jamais

- Binaire (pas de garantie ABI en v1)
- Comportement (le contrat d'une méthode est respecté)

## 3. Politique ABI (binaire)

### 3.1 État actuel

BCAD n'offre **aucune** garantie ABI. La bibliothèque n'est pas exportée séparément ; elle est liée statiquement à l'application.

### 3.2 Cible v1

Pas de garantie ABI. Les plugins doivent être recompilés pour chaque version de BCAD.

### 3.3 Cible v2 (futur)

ABI stable via :
- Wrapper C pur pour les interfaces stables
- `extern "C"` pour les fonctions d'entrée
- Versioning explicite des interfaces
- Utilisation de `abi::cxx11` ou PIMPL pour masquer les layouts

## 4. Problèmes potentiels

### 4.1 STL dans API publique

**Problème :** les types STL (`std::string`, `std::vector`, `std::map`) ne sont pas ABI-stable entre versions de compilateur/lib standard.

**Solution :** utiliser uniquement dans le SDK, pas dans les interfaces binaires. Préférer des types valeur simples (POD) ou des value types BCAD.

### 4.2 Exceptions

**Problème :** les exceptions traversant les frontières de bibliothèque peuvent poser des problèmes ABI (notamment entre compilateurs).

**Solution :**
- Les exceptions du SDK ne traversent jamais les frontières C
- Les exceptions C++ sont catchées dans le SDK et converties en codes d'erreur
- Le SDK documente les exceptions levées

### 4.3 RTTI

**Problème :** RTTI est désactivable (`-fno-rtti`). Si le Core l'utilise et le plugin non, ça casse.

**Solution :** ne pas dépendre de `dynamic_cast` dans le SDK. Utiliser des capabilities ou des `is*` explicites.

### 4.4 Vtables

**Problème :** les vtables changent avec les ajouts de méthodes virtuelles, ce qui peut casser l'ABI.

**Solution :**
- Préférer les interfaces sans méthodes virtuelles (template method pattern)
- Pour les interfaces critiques, utiliser PIMPL

### 4.5 Compatibilité compilateur

**Problème :** GCC, Clang, MSVC ont des ABI C++ différents (mangling, layout, vtables).

**Solution v1 :** un plugin compilé avec GCC ne fonctionne pas avec BCAD compilé avec Clang, et vice versa. Documenter.

**Solution v2 :** wrapper C pur, qui est ABI-stable entre compilateurs.

### 4.6 Types Qt

**Problème :** Qt expose des types dans certains headers publics.

**Solution :** aucun type Qt dans le SDK. Le SDK est pur C++.

### 4.7 Types CGAL

**Problème :** CGAL est template-heavy et exposé dans `Types.h`.

**Solution :** masquer CGAL derrière des value types BCAD (voir `GEOMETRY_ARCHITECTURE.md`).

### 4.8 Types OpenGL

**Problème :** types OpenGL (`GLuint`, etc.) exposés dans `IRenderBackend` ?

**Solution :** ne pas exposer dans le SDK. Les types OpenGL sont confinés à `OpenGLBackend`.

## 5. Versioning

> *État réel :* le projet est versionné `project(bcad VERSION 1.0.0)`, le SDK
> installable exporte `BCADConfigVersion.cmake` (compatibilité
> `SameMajorVersion`, v. ADR-006) et `libbcad_plugin` porte
> `VERSION`/`SOVERSION` (major => rupture ABI).

### 5.1 Versions

```cpp
namespace bcad {
constexpr int kBCADVersionMajor = 1;
constexpr int kBCADVersionMinor = 0;
constexpr int kBCADVersionPatch = 0;
constexpr const char* kBCADVersionString = "1.0.0";

constexpr int kSDKVersionMajor = 1;
constexpr int kSDKVersionMinor = 0;
constexpr int kSDKVersionPatch = 0;
}
```

La **majeure** du SDK et le `SOVERSION` de `libbcad_plugin` changent à toute
**cassure d'ABI**. L'ABI **plugin** a son propre compteur (voir 5.2).

### 5.2 Compatibilité binaire et plugin

La compatibilité ABI du plugin est déclarée dans `PluginInfo` et contrôlée en
**égalité stricte** au chargement par le PluginManager :

```cpp
constexpr int PLUGIN_API_VERSION = 2;  // incrémenté à chaque cassure d'ABI plugin
// v1 -> v2 : factory callbacks std::function -> pointeurs de fonction bruts

struct PluginInfo {
    // ...
    int apiVersion = PLUGIN_API_VERSION;  // version de l'ABI plugin
};
```

Le plugin peut aussi exporter `bcad_plugin_api_version()` (gate précoce
recommandée). Le PluginManager refuse de charger un plugin dont
`apiVersion != bcad::plugin::PLUGIN_API_VERSION` (un plugin ABI-ancien est
rejeté proprement, avant `bcad_plugin_init`).

**Conséquence :** un plugin est toujours compilé contre la même version que
l'hôte, avec la même chaîne d'outils (GCC/Clang/MSVC et libstdc++). Aucune
promesse d'ABI inter-versions en v1 (ADR-011), donc **plugins recompilés à
chaque version** de BCAD.

### 5.3 Macro de version

```cpp
// project(bcad VERSION ...) définit BCAD_VERSION_MAJOR/MINOR/PATCH/STRING
// côté CMake ; BCADConfigVersion.cmake (SameMajorVersion) filtre les
// versions acceptees par find_package(BCAD ...).
```

## 6. Règles

1. **API source stable** entre versions mineures
2. **Pas de garantie ABI** en v1
3. **Wrapper C** introduit en v2 pour ABI stable
4. **STL/exception/RTTI/vtables** : prudence dans le SDK
5. **Types tiers** (Qt, CGAL, OpenGL) jamais dans le SDK
6. **Versioning** explicite et vérifié au chargement
7. **Compatibilité compilateur** documentée
8. **Recompilation** du plugin requise à chaque version BCAD (v1)