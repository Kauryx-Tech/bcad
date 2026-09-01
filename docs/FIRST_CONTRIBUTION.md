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
- `src/app/Viewport.cpp` — interactions
- `include/bcad/app/*.h` — interface

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

> Note : L'architecture cible utilise un registre dynamique. L'architecture actuelle utilise un `enum class EntityType`. Le code ci-dessous est pour l'architecture **actuelle**.

### 1. Choisir une entité

Exemples : `Rectangle` (4 points), `Ellipse` (centre + rayons), `Spline`.

### 2. Fichiers à examiner

- `include/bcad/geometry/Entity.h` — enum
- `include/bcad/geometry/Line.h` — exemple
- `src/geometry/*.cpp` — implémentations

### 3. Approche actuelle

Comme l'enum est figé :
1. Créer la classe dans `include/bcad/geometry/`
2. L'implémenter dans `src/geometry/`
3. L'ajouter au `switch` dans `Database.cpp`, `DxfReader.cpp`, `DxfWriter.cpp`

**Ou** attendre la migration vers l'EntityRegistry (Phase 4).

### 4. Erreurs fréquentes

- **Modifier l'enum** sans coordination — casse l'API
- **Oublier `Database.cpp`** — persistance échoue
- **Oublier `DxfReader.cpp`/`DxfWriter.cpp`**

---

## Niveau 5 — Ajouter un plugin

**Objectif** : Comprendre le système de plugins.

**Statut** : Le système de plugins n'est pas encore implémenté (Phase 11 de la roadmap).

**Durée estimée** : 2-3 jours.

### 1. État actuel

Les plugins **n'existent pas encore**. Voir :
- `docs/PLUGIN_ARCHITECTURE.md`
- `docs/SDK_ARCHITECTURE.md`
- `docs/ARCHITECTURE_ROADMAP.md` — Phase 11

### 2. Quand la phase sera active

1. Créer un projet CMake séparé
2. Inclure `<bcad/plugin/PluginRegistry.h>`
3. Implémenter `extern "C" void bcad_plugin_init(PluginRegistry&)`
4. Compiler en `.so`/`.dll`

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