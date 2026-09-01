# Principes architecturaux BCAD

> **Principes non négociables.** Toute contribution, PR ou décision architecturale doit être compatible avec ces principes. Les violations doivent être justifiées explicitement.

## 1. Principes fondamentaux

### 1.1 Inversion de dépendance (Dependency Inversion)

Les modules de haut niveau ne doivent pas dépendre de modules de bas niveau. Les deux doivent dépendre d'abstractions.

- Le Core ne dépend pas des services : c'est le SDK qui dépend du Core.
- Les services dépendent d'interfaces abstraites définies dans le Core.

### 1.2 Séparation des préoccupations (SoC)

Chaque module a une responsabilité unique et claire :
- `geometry` : types et opérations géométriques
- `document` : modèle de document
- `commands` : transactions
- `events` : bus d'événements
- `properties` : propriétés d'entité

### 1.3 Core indépendant de l'UI

Aucune classe du Core n'inclut un header Qt, GTK, ou autre bibliothèque d'interface utilisateur. Le Core est une bibliothèque C++ pure.

### 1.4 Core indépendant du renderer

Le Core n'inclut aucun header de `rendering/`. Le rendu est un service périphérique qui consomme le Core via des interfaces.

### 1.5 Core indépendant du format de fichier

Le format `.bcad` est un détail de la couche `persistence/`. Le Document n'a aucune connaissance directe du format.

### 1.6 Geometry API indépendante du backend

Les types géométriques BCAD (`Point2`, `Vector2`, `Transform2`, etc.) sont définis dans le Core. CGAL est confiné à `geometry/detail/` et peut être remplacé sans casser l'API publique.

### 1.7 API publique minimale

Moins d'API publique = plus de liberté de refactoring. Le SDK expose uniquement ce qui est nécessaire aux plugins.

### 1.8 Implémentation privée

Les détails d'implémentation sont dans des namespaces ou dossiers `detail/` ou `private/`. Tout ce qui n'est pas dans l'API publique peut être modifié.

### 1.9 Extensibilité par registry

L'ajout d'un nouveau type d'entité, d'une commande, ou d'un format de fichier se fait via un registry, pas par modification du Core.

### 1.10 Plugins découplés du Core interne

Un plugin ne doit jamais inclure un header interne du Core (`core/internal/`, `geometry/detail/`). Il passe uniquement par le SDK.

### 1.11 2D/3D-ready

Toute l'architecture doit supporter 2D et 3D simultanément :
- Types neutres dimension (`Point`, `Vector`) ou types dimensionnés explicites (`Point2`, `Point3`)
- Index spatial abstrait (`ISpatialIndex`)
- Camera abstraite (`ICamera`)
- Tessellator abstrait (`ITessellator`)

### 1.12 Backward compatibility (lorsque raisonnable)

Les versions mineures préservent la compatibilité binaire descendante. Les versions majeures peuvent casser. Les versions de patch ne modifient pas l'API.

### 1.13 Testabilité

Chaque module du Core doit être testable unitairement sans nécessiter Qt, OpenGL, ou un fichier. Les services sont testés via leurs interfaces.

### 1.14 Versionnement des API

Toute API publique est versionnée :
- SDK API version (majeure.mineure)
- Plugin ABI version (majeure.mineure)
- BCAD version (majeure.mineure.patch)

### 1.15 Absence de dépendances circulaires

Le graphe de dépendances est un DAG. Aucune dépendance circulaire n'est tolérée, ni au niveau CMake, ni au niveau header.

---

## 2. Anti-patterns à éviter

### 2.1 Abstraction excessive

Ne pas créer d'interfaces virtuelles inutiles. Préférer des value types quand la sémantique le permet (ex: `Point2` plutôt que `IPoint2`).

### 2.2 Interfaces virtuelles inutiles

Si une classe n'a qu'une seule implémentation et n'est jamais substituée, ne pas la rendre virtuelle.

### 2.3 Patterns "enterprise" non nécessaires

Éviter les singletons globaux, factories abstraites multiples, visiteurs élaborés, si une solution plus simple suffit.

### 2.4 Dépendances circulaires

Tout cycle, même légitime, est suspect. Refactoriser en introduisant une interface.

### 2.5 Singleton partout

Les singletons sont justifiés uniquement pour des ressources vraiment globales (logger, allocateur, registre immuable).

### 2.6 Sur-engineering

Ne pas anticiper des besoins futurs non confirmés. YAGNI (You Aren't Gonna Need It).

### 2.7 Duplication de données

Une entité a une seule source de vérité. Pas de copies synchronisées manuellement.

### 2.8 API publique exposant les détails internes

Les détails d'implémentation (structures internes, fonctions helper) ne doivent pas être dans les headers publics.

---

## 3. Critères d'acceptation d'une PR

Une PR est acceptable si :
- [ ] Elle respecte le graphe de dépendances
- [ ] Aucun header public n'expose une dépendance tierce inutile
- [ ] Les tests existants passent
- [ ] De nouveaux tests sont ajoutés pour les nouveaux comportements
- [ ] La documentation est mise à jour
- [ ] Le code respecte `.clang-format` et `.editorconfig`
- [ ] Pas de warning GCC/Clang/MSVC supplémentaire

---

## 4. Critères de revue architecturale

Une décision architecturale est acceptable si :
- [ ] Elle est compatible avec les 15 principes ci-dessus
- [ ] Elle est documentée (dans `docs/`)
- [ ] Elle est testable
- [ ] Elle ne ferme pas la porte à des extensions futures probables
- [ ] Elle ne crée pas de dette technique cachée