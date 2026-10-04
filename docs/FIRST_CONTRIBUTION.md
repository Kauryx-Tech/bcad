# Première contribution BCAD

> Guide pas-à-pas pour contribuer à BCAD, du plus simple au plus avancé.

## Niveau 1 — Modifier un test

**Objectif** : Comprendre le cycle build/test.

**Durée** : 15-30 minutes.

### 1. Examiner le fichier de test

Ouvre `tests/smoke_test.cpp`. Il y a une fonction par module : `test_geometry_module`, `test_layers_module`, `test_core_module`, etc.

```cpp
bool test_geometry_module() {
    return true;
}
```

### 2. Documentation à lire

- `docs/GETTING_STARTED.md` — pour configurer l'environnement
- `README.md` § Compilation — pour les commandes exactes

### 3. Modifier le test

Ajoute une ligne dans `int main()` :

```cpp
std::cout << "[smoke] running my test..." << std::endl;
if (!test_my_feature()) {
    std::cerr << "FAIL: my_feature" << std::endl;
    return 1;
}
```

### 4. Build et test

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### 5. Erreurs fréquentes

- **Oublier de compiler avant de tester** : toujours `cmake --build` avant `ctest`
- **Modifier un test pour qu'il passe au lieu de corriger le code**

---

## Niveau 2 — Corriger un bug simple

**Objectif** : Corriger un bug dans un module existant.

**Durée** : 1-2 heures.

### 1. Trouver un bug

- Cherche un bug dans les issues GitHub
- Ou utilise l'application et note un comportement anormal

### 2. Fichiers à examiner

Selon le module :
- `include/bcad/geometry/Entity.h`
- `src/geometry/*.cpp`
- `src/core/Document.cpp`

### 3. Documentation à lire

- `docs/GEOMETRY_ARCHITECTURE.md` — si bug géométrique
- `docs/DOCUMENT_MODEL.md` — si bug document

### 4. Étapes

1. Reproduire le bug
2. Identifier la fonction responsable
3. Écrire un test qui échoue
4. Corriger le code
5. Vérifier que le test passe

### 5. Erreurs fréquentes

- **Ne pas écrire de test pour le bug** — sans test, le bug peut revenir
- **Corriger le symptôme au lieu de la cause**

---

## Niveau 3 — Modifier une fonctionnalité existante

**Objectif** : Améliorer une fonctionnalité.

**Durée** : 2-4 heures.

### 1. Choisir une fonctionnalité

Exemples : raccourci clavier, calcul d'aire, snapping.

### 2. Fichiers à examiner

- `src/app/Commands.cpp` — commandes
- `src/app/Viewport*.cpp` — interactions, sur dix unités par responsabilité (la carte est en tête de `Viewport.cpp`) ;
  la table de répartition est dans l'en-tête de `src/app/Viewport.cpp`
- `src/app/*.h` — interface (privée : ces en-têtes ne sont pas installés)

### 3. Documentation à lire

- `docs/COMMAND_SYSTEM.md` — système de commandes
- `docs/DOCUMENT_MODEL.md` — modèle de document

### 4. Build

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/src/app/bcad  # tester manuellement
```

### 5. Erreurs fréquentes

- **Ne pas tester manuellement** — tests automatisés ne couvrent pas l'UI
- **Introduire une régression**

---

## Niveau 4 — Ajouter une nouvelle entité géométrique

**Objectif** : Comprendre le modèle d'entité.

**Durée** : 4-6 heures.

> Note : les entités sont enregistrées dynamiquement (`TypeId` + `EntityRegistry`,
> ADR-003). L'ancien `enum class EntityType` est déprécié pour les nouvelles
> entités : on utilise `typeId()`/`TypeId`.

### 1. Choisir une entité

Exemples : `Rectangle` (4 points), `Ellipse` (centre + rayons), `Spline`.

### 2. Fichiers à examiner

- `include/bcad/geometry/Entity.h` — classe de base
- `include/bcad/registry/EntityRegistry.h` — règle d'enregistrement
- `include/bcad/plugin/PluginRegistry.h` — médiation plugin
- `src/geometry/*.cpp` — implémentations

### 3. Approche

1. Hériter de `bcad::geom::Entity` dans `include/bcad/geometry/`
2. L'implémenter dans `src/geometry/`
3. L'enregistrer au chargement (plugin ou applicatif) sous un `TypeId` — pas
   de modification de `Database.cpp`/`DxfReader.cpp`/`DxfWriter.cpp`
   (dispatch par `EntityRegistry`, ADR-003)

> Le motif complet (avec commande et serializer) est décrit dans
> `docs/EXTENDING_BCAD.md` ; la preuve exécutable est `examples/sdk_proof`.

### 4. Erreurs fréquentes

- **Définir des types dans le namespace de l'hôte** — ABI plugin cassée
- **Ré-exposer CGAL dans `include/bcad/geometry/`** — violation ADR-002
- **Contourner `EntityRegistry`** en ajoutant un `switch` sur `type()`

---

## Niveau 5 — Ajouter un plugin

**Objectif** : Comprendre le système de plugins.

**Statut** : **implémenté** (ADR-005, `dlopen` + `bcad_plugin_init`) — preuve
`examples/sdk_proof`, test `sdk_external_test`.

**Durée estimée** : 2-3 jours.

### 1. État actuel

Les plugins tournent via `PluginManager` (chargement/déchargement) et un
`PluginRegistry` médiatisé par l'hôte. Voir :
- `docs/PLUGIN_ARCHITECTURE.md`
- `docs/SDK_ARCHITECTURE.md`
- `examples/sdk_proof` — preuve installable

### 2. Compiler un plugin

1. Créer un projet CMake séparé
2. `find_package(BCAD CONFIG REQUIRED)` + lier **`BCAD::bcad_plugin`**
3. Inclure `<bcad/plugin/PluginRegistry.h>`
4. Implémenter `extern "C" int bcad_plugin_api_version()` (`PLUGIN_API_VERSION`,
   gate strict) et `extern "C" bool bcad_plugin_init(PluginRegistry& reg)`
5. Compiler en `MODULE` : `libabc.so` (fonctions de type partagés : voir §10)

### 3. Documentation à lire

- `docs/EXTENDING_BCAD.md`
- `docs/PLUGIN_ARCHITECTURE.md`
- `docs/ARCHITECTURE_DECISIONS.md` ADR-005

---

## Critères d'acceptation

Ta PR doit :

- [ ] Compiler sans warnings
- [ ] Passer `ctest`
- [ ] Ne pas introduire de violation architecturale
- [ ] Avoir un test pour le nouveau comportement
- [ ] Suivre le style C++ du projet

## Conseils

1. **Commence petit**
2. **Lis avant d'écrire**
3. **Demande avant de refactorer**
4. **Teste manuellement**
5. **Documente** dans `docs/`

## Voir aussi

- `docs/GETTING_STARTED.md`
- `docs/CONTRIBUTOR_GUIDE.md`
- `docs/EXTENDING_BCAD.md`
- `docs/ARCHITECTURE_PRINCIPLES.md`
- `CONTRIBUTING.md`