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
| 017 | Espace papier comme objet du document, vocabulaire déclaré par le module | Proposé (spike mesuré, `layout_spike_test`) |

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

**Décision :** `IEntitySerializer` par type dans `SerializerRegistry`. Le chargeur natif n'a pas
d'autre porte : il demande au registre le serializer du `type_id` de la ligne, et range les
propriétés du `PropertyMap` dans une table générale `entity_properties(entity_id, key, value_json)`
où le type de la valeur est une donnée de la ligne. L'hôte ne déclare le nom d'aucune clé.

**Conséquences :** Plugins sauvegardent sans modifier le Core. Un type sans serializer enregistré
(module absent du poste) n'est pas abandonné : `geom::UnknownEntity` conserve type, paramètres et
propriétés pour les réécrire tels quels — un ouvrir/enregistrer ne détruit pas le travail d'un poste
équipé. `scripts/check_arch.sh` interdit à `src/io/` un en-tête ou un identifiant de domaine, hors
lignes marquées `NOLINT(arch-legacy-v1)` (la lecture et la migration du format v1, qui doit bien
nommer ce qu'elle convertit).

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

**Décision :** SQLite (DDL) + JSON, avec le JSON limité à sa seule utilité : le **type** et la valeur
d'une propriété (`value_json` dans `entity_properties`). La géométrie et les attributs d'un type
restent la chaîne compacte produite par son serializer, dans `entities.params`, sous un `type_id`
qui est une chaîne. La version du fichier est dans `PRAGMA user_version` : seule la version
courante est écrite, les versions antérieures sont lues par un chemin de compatibilité et une
migration transactionnelle (`Database::migrateSchema`), une version future est refusée sans toucher
au fichier.

**Conséquences :** SQLite libre/robuste, fichier inspectable à l'`sqlite3` de n'importe quel poste.
`schemaVersion()` **par serializer n'est pas implémenté** : le versionnement est celui du fichier, le
format d'un type reste la affaire de son serializer (qui doit donc tolérer ce que ses versions
antérieures ont écrit). Écrire uniquement la version courante évite la dette de deux formats
maintenus en parallèle.

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
- Positif : cette interdiction n'est plus une convention. `scripts/check_arch.sh`
  la vérifie par machine dans `src/app/` (§10, §10bis) et dans `src/io/` avec
  `include/bcad/io/` (gardes 12a et 12b : dépendances vers un module, puis
  identifiants de domaine). La seule exemption admise est marquée sur la ligne
  même (`NOLINT(arch-legacy-v1)`) et ne couvre que les deux littéraux dont la
  lecture de compatibilité `.bcad` v1 a besoin.
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

## 017 : L'espace papier est un objet du document, et son vocabulaire est déclaré

**Statut :** Proposé — adossé à un spike mesuré, pas à une discussion
**Date :** 2026-09-25

### Contexte

Le parcours attendu d'un plan cadastral — dossier, profil national, calques, layout
A3 paysage, vue à 1:500, cartouche alimenté par les attributs, flèche, barre,
légende, grille, nomenclature, validation, PDF vectoriel, DXF/GeoJSON/CSV, relecture
sans perte — bute de l'étape 1 à l'étape 9 sur un seul facteur commun :
**`src/layout` est un peintre sans objet.** `Sheet`, `layout::Viewport`, `Cartouche`
et `ParcelTable` ne vivent que le temps d'un `PdfExportOptions`, construit à la
volée par l'appelant et jeté à la fin du tracé. Rien n'est nommé, rien n'est
éditable, rien ne survit à la fermeture du fichier.

Ce n'est pas une intuition : `layout_spike_test`
(`tests/unit/layout/LayoutSpikeTest.cpp`) mesure sept obstacles sur le code réel.

| # | Obstacle mesuré | Preuve |
|---|---|---|
| 1 | La composition **sait** respecter une échelle explicite : 40×30 m à 1:500 donnent 80×60 mm | `Composition.h:103` |
| 2 | Mais le seul chemin de l'hôte l'écrase : 1:500 demandé ressort **1:200** | `PdfExport.cpp:411`, appelé par `MainWindowDocument.cpp:205` et `SplitParcelCommand.cpp:208` |
| 3 | Une vue à 1:500 d'un îlot de 300×200 m fait 600×400 mm sur 400×277 mm imprimables, et rien ne le refuse : `Viewport::fitsIn` existe et **n'a aucun appelant dans `src/`** | `Viewport.h:33` |
| 4 | Une seule vue par feuille, et centrée d'office : deux vues distinctes ont leurs emprises **l'une dans l'autre** ; la position papier n'est pas une donnée de la vue | `PdfExport.h:29` |
| 5 | Le mobilier n'est pas déclaré, il est **deviné du contenu** : un cartouche de dix champs d'attributs ne réserve **0 mm** de bande et n'y peint que les 150 pixels des deux montants du cadre — zéro cartouche — là où le seul champ `commune` réserve 25 mm et 263 pixels | `Cartouche.h:47`, `PdfExport.cpp:383` |
| 6 | Le vocabulaire est une **structure fermée** : `PROFIL_NATIONAL` et `INDICE_CADASTRAL` entrés dans `fromKeyValuePairs` ne ressortent pas — perte silencieuse | `Cartouche.h:99` |
| 7 | L'échelle et la grille sont du vocabulaire **français dans l'API publique installée** : 11 valeurs commentées « cadastrales FR (BOFiP DGFiP) », `gridStepMm` sans appelant, aucune légende ni grille de feuille peinte | `Scale.h:10-31` |

À quoi s'ajoute le fait le plus gênant : les libellés du cartouche — `Commune`,
`Section`, `Contenance`, `Propriétaire`, `Code commune` (commenté « code INSEE »
dans `Cartouche.h:22`) — sont écrits en dur dans le peintre de l'hôte, à
`src/layout/PdfExport.cpp:72-93`. Un profil togolais ne peut pas nommer ses champs
sans patcher l'hôte. L'ADR-016 §4 l'interdit, et la garde `check_arch.sh` ne
regardait pas `src/layout/` : le trou est structurel, pas une faute ponctuelle.

### Décision

1. **Un layout est un objet nommé du document**, pas une option d'export ni une
   entité géométrique : `Document` expose une collection de feuilles, chacune
   tenue par un nom, un format, une orientation, des marges, des vues et des
   meubles. Il est hors du modèle : ni `extents()`, ni index spatial, ni
   tessellation, ni picking de dessin ne le voient.
2. **Une vue porte sa position.** Source (rectangle monde), échelle (valeur
   explicite, jamais déduite à l'impression) et emplacement papier sont trois
   données distinctes. Ajuster l'échelle pour tenir devient une **action** de
   l'opérateur, pas un effet de bord de `drawSheet`.
3. **Le vocabulaire est une donnée déclarée, jamais un champ C++.** Un meuble est
   un couple « nature + zone + liste de champs `libellé → valeur` ». L'hôte peint
   des libellés qu'il ne comprend pas ; le module et son profil national les
   nomment. En conséquence : les 22 champs de `Cartouche`, les libellés du peintre,
   les 11 échelles FR et les colonnes de `ParcelRow` **quittent l'API publique**
   pour devenir des déclarations de module.
4. **Ce qui est déclaré par un module inconnu se recompose sans se perdre**, selon
   la règle déjà posée pour les entités (ADR-004, `UnknownEntity`) : une nature de
   meuble sans peintre est conservée octet pour octet et restituée au retour du
   module.
5. **La validité d'une feuille est une validation, pas une exception.** « La vue
   déborde de la feuille », « échelle hors de la liste du profil » passent par
   `IValidator` (ADR-010, déjà en service), donc un module peut publier ses propres
   règles de mise en page.
6. **Un seul saut de format.** `.bcad` v3 porte à la fois les attributs du dossier
   (métadonnées au niveau document, qui manquent depuis l'étape 1) et les feuilles.
   Les règles de v2 s'appliquent inchangées : migration transactionnelle, version
   future refusée sans toucher à l'octet, version courante seule écrite.

### Conséquences

- Positif : les étapes 1 à 9 du parcours cessent d'être des paramètres d'un appel
  d'export et deviennent un objet que l'on ouvre, édite, annule et referme — ce qui
  rend l'étape 13 (« rouvrir sans perte ») vraie pour la mise en page, pas seulement
  pour le dessin.
- Positif : `src/layout` redevient ce que son nom indique — de la géométrie en
  millimètres et un peintre — sans un seul nom de métier dans son code ni son API.
  La garde `check_arch.sh` doit alors être étendue à `src/layout/` avec
  `include/bcad/layout/`, comme elle l'est déjà à `src/io/` (gardes 12a/12b).
  Elle est le **point d'arrivée** de la décision, pas son départ : posée
  aujourd'hui, elle échouerait sur les libellés de `PdfExport.cpp:72-93` et
  bloquerait la branche avant que le vocabulaire ait pu bouger.
- Positif : un second domaine (réseaux, topographie, lotissement) déclare son
  cartouche, ses échelles et sa nomenclature sans ouvrir une ligne de `src/`.
- Négatif : c'est le changement le plus étendu depuis l'ABI des plugins. Il touche
  l'API publique installée (`Cartouche`, `PdfExportOptions`, `Document`), donc
  `PLUGIN_API_VERSION` passe, et le module cadastral est réécrit sur ce point.
- Négatif : `Cartouche`, `kStandardScales` et `ParcelTable` sont **cassés en
  compatibilité**, pas seulement étendus. Rien dans `examples/` ni dans le SDK
  prouvé ne les utilise ; le seul consommateur est le module cadastre du dépôt.
- Négatif : l'édition d'une feuille (voir le papier dans la surface de dessin,
  déplacer une vue à la souris) n'est **pas** résolue par cette ADR. Elle est
  rendue possible, pas livrée. Sans elle, le layout se règle par des boîtes de
  saisie et l'aperçu d'impression.
- Négatif : `layout_spike_test` caractérise des obstacles ; quand un obstacle
  disparaît, l'assertion casse. Le test est à convertir en test de contrat au fur
  et à mesure, pas à supprimer.

### Alternatives

- **Garder la feuille éphémère et ne faire que l'UI** (boutons sur le chemin
  d'impression existant) : livrable en deux semaines, mais l'étape 13 reste fausse
  — l'opérateur qui règle une mise en page la perd en fermant le fichier. Écarté
  pour cette raison, qui est exactement le critère « le livrable est un document »
  de l'ADR-016 §3.
- **Faire du layout un type d'entité du registre** (ADR-003/004, gratuit en
  sérialisation et en annulation) : écarté, parce qu'une feuille n'est pas du
  contenu dessiné — elle polluerait `extents()`, l'index spatial, le picking et la
  tessellation, et chaque consommateur devrait penser à la filtrer. C'est le genre
  de filtre oublié que cet ADR cherche à rendre impossible.
- **Persister tout de suite `Cartouche` tel quel dans une table `layouts` à
  colonnes** (le chemin le plus court, et le plus tentant) : écarté, c'est figer
  dans un format versionné — donc pour toujours — le vocabulaire cadastral
  français, en répétant exactement la faute que la table `cadastre_parcels` avait
  commise et que le geste 2 vient de défaire.
- **Ajouter une exception à ADR-016 pour `src/layout`** : écarté, l'interdiction
  n'a de valeur que si le répertoire qui peint le document y est soumis.

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