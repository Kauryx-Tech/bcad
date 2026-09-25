# Architecture des plugins BCAD

> [!IMPORTANT]
>
> ## Statut : ARCHITECTURE IMPLEMENTEE (ABI ADR-005)
>
> Le systeme de plugins existe : `PluginManager` (dlopen), symbole
> `bcad_plugin_init(PluginRegistry&)` (ADR-005), PluginRegistry concret
> (metadonnees + enregistrement des extensions), preuve avec un plugin externe
> minimal (`examples/sdk_proof`, test `sdk_external_test`). Ce document décrit
> la cible ; l'ecart implementation est signale dans chaque section ou
> necessaire (voir aussi `MIGRATION_PLAN.md`, Phase 10).

> Système de plugins dynamique. Chargement, découverte, cycle de vie.

## 1. Principe

Un plugin BCAD est une **bibliothèque dynamique** (`.so`/`.dll`/`.dylib`) qui :
1. Est compilée séparément du Core
2. Trouve BCAD via `find_package(BCAD CONFIG REQUIRED)`
3. Exporte un symbole `bcad_plugin_init(PluginRegistry&)`
4. Enregistre ses entités, commandes, serializers via le `PluginRegistry` (§4, médiatisé par l'hôte)

## 2. Architecture

```
┌──────────────────────────────────────┐
│          BCAD Application             │
│  ┌──────────────────────────────┐    │
│  │       PluginManager           │    │
│  │  - discovery                  │    │
│  │  - loading                    │    │
│  │  - lifecycle                  │    │
│  └──────────────────────────────┘    │
└──────────────────┬───────────────────┘
                   │ dlopen
       ┌───────────┼───────────┐
       ▼           ▼           ▼
   architecture  civil    mechanical
     .so          .so      .so
```

## 3. Interface plugin

L'ABI plugin est la cible ADR-005 : une bibliothèque dynamique qui exporte le
symbole `bcad_plugin_init(PluginRegistry&)`. Il n'y a **pas** d'interface de
plugin orientée objet (`IPlugin` a été supprimé, ADR-013) : le contrat est le
registre passé au point d'entrée.

```cpp
// include/bcad/plugin/PluginRegistry.h
namespace bcad::plugin {

struct PluginInfo {
    std::string name;            // "architecture"
    std::string version;        // "1.0.0"
    std::string description;    // "..."
    std::string author;          // "..."
    int apiVersion = PLUGIN_API_VERSION; // gate ABI (optionnel, precoce via bcad_plugin_api_version)
};

using PluginInitFunc = bool (*)(PluginRegistry& reg);

}
```
## 4. PluginRegistry

Le registre est **concret** (pas d'interface virtuelle, ADR-013) et
**médiatisé par l'hôte** : les enregistrements sont implémentés dans
libbcad_plugin (DSO hôte), ce qui garantit une seule instance des registres
globaux quel que soit le DSO du plugin.

```cpp
namespace bcad::plugin {

class BCAD_PLUGIN_API PluginRegistry {
public:
    PluginInfo& info();   // metadonnees du plugin

    bool registerEntityType(geom::TypeId typeId, const EntityFactory& factory);
    bool registerCommand(std::string_view commandName, const CommandFactory& factory);
    bool registerSerializer(std::unique_ptr<serialization::IEntitySerializer> serializer);
    bool registerWorkbench(std::unique_ptr<IWorkbench> workbench);
    bool registerValidator(std::unique_ptr<IValidator> validator);
    bool registerFileExporter(std::unique_ptr<IFileExporter> exporter);

    // Ce que CE plugin a enregistré : l'hôte les retire avant dlclose.
    std::vector<std::string> registeredEntityTypeIds() const;
    std::vector<std::string> registeredCommandNames() const;
    std::vector<std::string> registeredSerializerTypeIds() const;
    std::vector<std::string> registeredWorkbenchIds() const;
    std::vector<std::string> registeredValidatorIds() const;
    std::vector<std::string> registeredFileExporterIds() const;

    // Données réglables du module (gabarits), voir ci-dessous.
    void addDataDirectory(const std::string& directory);
    std::string resolveDataFile(const std::string& relativePath) const;

private:
    PluginInfo info_;
    std::vector<std::string> serializerTypeIds_, entityTypeIds_, commandNames_;
    std::vector<std::string> workbenchIds_, validatorIds_, fileExporterIds_;
    std::vector<std::string> dataDirs_;
};

}
```

`registerWorkbench` est le quatrième point d'extension : le plugin y déclare ses
panneaux et ses actions, l'hôte en fait des menus et des panneaux de ruban **sans
connaître aucun métier** (voir [WORKBENCH.md](WORKBENCH.md)). Comme les
serializers, l'instance est construite dans le DSO du plugin et **détenue par
l'hôte** (`WorkbenchRegistry`), qui la détruit au déchargement avant `dlclose`.

`registerValidator` est le cinquième (ADR-016 : le core ne contient **aucune**
règle) : le plugin déclare des `IValidator` — un `id`, un `label`, les `TypeId`
auxquels il s'applique, et `validate(entities) -> std::vector<Diagnostic>`.
L'hôte ne sait que les exécuter et afficher les diagnostics
(`WorkbenchParams::RunValidators`, dock « Vérifications ») ; la rédaction des
messages appartient au plugin. Même cycle de vie que les workbenches : registre
porté par l'hôte (`ValidatorRegistry`), retrait avant `dlclose`.
`registeredValidatorIds()` est le traceur de ce qui a été déclaré, utilisé pour
le rollback si `bcad_plugin_init` échoue.

`registerFileExporter` est le sixième : un `IFileExporter` porte un `id`, un
`label` (libellé de menu, dans la langue du déclarant), une `extension` et
`writeDocument(document, path, &error)`. Le menu `Fichier → Exporter` dresse la
liste des exporteurs **enregistrés** — y compris ceux d'un module métier — et
l'hôte ne nomme aucun format (ADR-016). Les formats natifs du noyau tombent dans
le même registre (`io::initializeNativeFileExporters()`), pour qu'un exporteur de
plugin ne soit pas traité comme un citoyen de second rang. Même cycle de vie que
les workbenches : registre porté par l'hôte (`FileExporterRegistry`), traceur
`registeredFileExporterIds()`, retrait avant `dlclose`.

### Données réglables d'un module (gabarits)

Un métier ne tient pas dans son code : le motif d'une section cadastrale, le
format d'un numéro, la tolérance d'un levé sont des valeurs qu'un utilisateur
averti doit pouvoir changer sans recompiler le module. Le plugin ne les lit donc
pas en dur et ne devine pas où elles sont posées — l'hôte les lui annonce :

- l'hôte appelle `PluginManager::addDataDirectory()` pour chaque répertoire où il
  trouve des données de modules ;
- avant `bcad_plugin_init`, ces répertoires sont versés dans le
  `PluginRegistry`, et `$BCAD_PLUGIN_DATA` est inséré **en tête** (priorité à la
  configuration explicite) ;
- le plugin résout un chemin relatif **préfixé par son propre module** :
  `resolveDataFile("cadastre/templates/cadastre_togo.json")`. Préfixer évite
  qu'un module lise le gabarit d'un autre par collision de nom.

Ce n'est **pas** un septième point d'extension : rien n'est enregistré, aucun
registre n'est impliqué. C'est un canal de lecture, et un module qui ne trouve
aucun fichier garde ses valeurs par défaut — l'absence de gabarit n'est pas une
erreur. Le module cadastral l'utilise pour les motifs d'identification de la
règle `cadastre.identification` (voir `CADASTRE_PLUGIN_STATUS.md`).

Un `PluginRegistry` qui porte des répertoires est un changement de layout, donc
`PLUGIN_API_VERSION` v7 (voir §13).

### Comment fonctionne la médiation (une seule instance des registres)

- `libbcad_plugin.so` est volontairement **mince** : il ne lie **pas** les
  bibliothèques statiques qu'il médiatise. Dans son diagramme de symboles,
  `EntityRegistry::*`, `CommandRegistry::instance()` et
  `SerializerRegistry::registerSerializer` sont **non définis (U)**.
- Les registres (singletons) sont portés par **l'executable hôte**, qui lie
  `bcad_registry`, `bcad_commands` et `bcad_serialization`. Ces trois
  bibliothèques sont compilées en **visibilité par défaut** (pas `hidden`),
  condition nécessaire pour que l'éditeur de liens accepte qu'un DSO les
  référence et les auto-exporte vers l'executable.
- Au chargement, les références non définies de libbcad_plugin sont résolues
  vers l'instance de l'executable : hôte et plugins aboutissent donc au **même**
  registre, sans duplication des statics entre DSO.

**Exigence d'hôte :** toute application qui charge des plugins doit lier
`BCAD::bcad_plugin` **et** `BCAD::bcad_registry`, `BCAD::bcad_commands`,
`BCAD::bcad_serialization`, ainsi que les bibliothèques de types portées par
l'exécutable (voir `examples/sdk_proof/loader`). Le loader de la preuve embarque
l'ensemble des objets de `bcad_geometry`/`bcad_core`/`bcad_properties` via
`-Wl,--whole-archive` et exporte les symboles bcad ciblés avec
`--export-dynamic-symbol` (voir ci-dessous). Sans cela, le chargement échoue ou
les enregistrements aboutissent dans une instance distincte.

### Types partagés hôte ↔ plugin (dynamic_cast inter-DSO)

Par défaut, chaque DSO embarque ses propres copies des vtables/typeinfo, ce qui
rend le `dynamic_cast` entre DSO non fiable (chaque unité d'édition de liens a
son `typeid`). Pour partager LES types entre l'hôte et le plugin :

- Le plugin (**Phase 10, preuve**) **ne lie aucune bibliothèque de types** : ses
  entités/vtables/typeinfo sont celles **de l'hôte**.
- L'édition de liens de l'hôte doit donc **définir et exporter** tous les
  symboles de types bcad que le plugin référence. C'est le rôle de
  `-Wl,--whole-archive` (tirer **tous** les objets des libs statiques, y compris
  `Property.cpp.o` qui définit les vtables) associé à
  `--export-dynamic-symbol` ciblé sur les seuls symboles `bcad` :
  `_ZN4bcad*` (fonctions), `_ZNK4bcad*`, `_ZTVN4bcad*` (vtables),
  `_ZTIN4bcad*`/`_ZTSN4bcad*`/`_ZGVN4bcad*` (RTTI).
- **Ne pas utiliser `-rdynamic`** : il entraîne un crash de teardown
  déterministe (collision des weak symbols vtables entre DSO). Export ciblé
  uniquement des symboles bcad (testé : binutils ≥ 2.46).
- Résultat : `typeid(*entite) == typeid(PointEntity)` et le `dynamic_cast`
  inter-DSO réussissent dans le loader de la preuve.

### Closures des factories : pointeurs de fonction, pas std::function

Les callbacks d'enregistrement (`EntityFactory`, `CommandFactory`) sont des
**pointeurs de fonction bruts** dans l'ABI (et non des `std::function`) : un
`std::function` traversant un DSO embarque son `_M_manager`/`_M_invoker` émis
dans le **plugin** (site de construction). Stocké dans un registre hôte, il
serait détruit à la sortie du processus **après** le `dlclose` → SEGV.
- L'hôte (`libbcad_plugin`, jamais déchargé) **re-emballe** le pointeur dans un
  `std::function` créé côté hôte : le registre global ne détient aucune closure
  dont le code vit dans le plugin.
- Les plugins passent donc des **fonctions libres** (`&makeEntity`) ou des
  lambdas **sans capture** (convertibles en pointeur de fonction). Un lambda
  avec capture n'est pas accepté (contrainte d'ABI volontaire).

### Instances plugin dans les registres hôtes (cycle de vie des serializers)

`registerSerializer` est le seul cas où les registres hôtes stockent une
**instance** (et non une closure) créée par le plugin : l'objet
`IEntitySerializer` (vtable, dtor, `operator delete`) vit dans le DSO du
plugin. L'hôte (libbcad_plugin) le porte dans `SerializerRegistry` mais sa
destruction **après** un `dlclose` exécuterait du plugin code → SEGV.

Résolu par un cycle de vie maîtrisé :
- `PluginRegistry` mémorise les TypeIds serializer enregistrés par le plugin ;
- au déchargement, `unloadPlugin` les **retire du registre avant `dlclose`**
  (`SerializerRegistry::remove`, destruction pendant que le DSO est chargé) ;
- corollaire : `registerSerializer` refuse un TypeId déjà traité, et un
  déchargement ne laisse jamais de code plugin dans les registres hôtes.

## 5. Symbole d'entrée

```cpp
// Plugin.cpp
#include <bcad/plugin/PluginRegistry.h>
#include <memory>

extern "C" int bcad_plugin_api_version() {   // optionnel : gate ABI precoce
    return bcad::plugin::PLUGIN_API_VERSION;
}

extern "C" bool bcad_plugin_init(bcad::plugin::PluginRegistry& reg) {
    reg.info().name = "architecture";
    reg.info().version = "1.0.0";
    reg.info().description = "Architecture domain entities";

    // Factories = pointeurs de fonction (ABI) : fonctions libres ou lambdas
    // SANS capture (conversion implicite). L'hote les re-emballe (cf. §4).
    reg.registerEntityType(bcad::geom::TypeId{"arch.wall"}, [](std::string_view params) {
        return std::make_unique<WallEntity>(/* ... */);
    });

    reg.registerCommand("CreateWall", [](const std::vector<std::string>& args) {
        return std::make_unique<CreateWallCommand>(args);
    });

    reg.registerSerializer(std::make_unique<WallSerializer>());

    return true;   // false fait echouer le chargement
}

extern "C" void bcad_plugin_shutdown() {}
```

## 6. PluginManager

### 6.1 Interface

L'interface publique du gestionnaire est **réduite au cycle de vie** (ADR-013) :
chargement, déchargement, liste, et **découverte** (énumération des répertoires
candidats — l'hôte ne nomme aucun plugin, ADR-016).

**Médiation :** l'executable hôte porte les registres
(`BCAD::bcad_registry`, `BCAD::bcad_commands`, `BCAD::bcad_serialization`) ;
`libbcad_plugin.so` est mince et résout ses références vers l'hôte (§4).

```cpp
namespace bcad::plugin {

class BCAD_PLUGIN_API PluginManager {
public:
    virtual ~PluginManager() = default;

    // Chargement (dlopen) : vérifie bcad_plugin_init, appelle le plugin, publie
    // un PluginHandle portant reg.info(). nullptr si échec.
    virtual PluginHandle* loadPlugin(const std::string& path) = 0;

    // Déchargement : bcad_plugin_shutdown (si présent) puis dlclose.
    virtual bool unloadPlugin(PluginHandle* handle) = 0;

    // Liste
    virtual std::vector<PluginHandle*> getLoadedPlugins() const = 0;

    // Répertoires de découverte, dans l'ordre d'exploration.
    virtual void addSearchDirectory(const std::string& directory) = 0;

    // Répertoires de DONNÉES des modules (gabarits). Versés au
    // PluginRegistry avant bcad_plugin_init ; un module y résout
    // "<module>/…". Séparé des répertoires de modules : un DSO et un
    // fichier de réglages ne vivent pas au même endroit.
    virtual void addDataDirectory(const std::string& directory) = 0;

    // Candidates : $BCAD_PLUGIN_PATH (fichier OU dossier) puis les répertoires
    // ajoutés. Dédupliqués, filtrés (un DSO de la plateforme BCAD n'est pas un
    // plugin), triés par chemin pour un ordre déterministe.
    virtual std::vector<std::string> discoverPluginPaths() const = 0;

    // Charge tous les candidats ; retourne les handles publiés.
    virtual std::vector<PluginHandle*> loadAllDiscovered() = 0;
};

plugin::PluginManager& pluginManager();   // singleton hôte

}
```

L'hôte Qt appelle `addSearchDirectory()` pour les emplacements usuels
(`lib/bcad/plugins` de l'installation, arbre de build, `$XDG_DATA_HOME/bcad/plugins`)
puis `loadAllDiscovered()`, et affiche « N plugin(s) chargé(s) » : il ne
contient le nom d'aucun module.

### 6.2 Cycle de vie

```
1. Chargement (dlopen)
2. Validation (symbole bcad_plugin_init présent, API stricte)
3. Vérification de version (bcad_plugin_api_version optionnel, gate ABI précoce)
4. Appel à bcad_plugin_init(PluginRegistry&), hors mutex (ré-entrance possible)
   - le registre reçoit d'abord les répertoires de données (§4, gabarits), dont
     `$BCAD_PLUGIN_DATA` en tête ; le module lit ses valeurs réglables pendant ce
     temps, avant d'enregistrer quoi que ce soit
   - si l'init échoue : rollback, dans l'ordre inverse — exporteurs,
     validateurs, workbenches, serializers, commandes, types d'entités — puis
     dlclose
5. Copie de reg.info() et des identifiants déclarés dans le PluginHandle
6. Plugin actif
7. Shutdown (bcad_plugin_shutdown, hors mutex)
8. Retrait des instances plugin des registres hôtes (exporteurs, validateurs,
   workbenches, serializers) PENDANT que le DSO est encore chargé
9. Unload (dlclose)
```

Étapes 8 et 9 ne sont faites que par `unloadPlugin()` explicite :
`~PluginManager` ne dlclose plus rien (cf. §13.7).

## 7. Manifeste (non implémenté)

**Aucun `plugin.json` n'est lu par le dépôt** (vérifié : zéro référence dans
`src/` et `include/`). La découverte se fait en énumérant les répertoires et en
`dlopen`-ant chaque candidat (§6.1) ; les métadonnées viennent de
`PluginRegistry::info()` une fois le module chargé, jamais d'un fichier à côté.

Le format ci-dessous est donc un dessin, pas un contrat :

```json
{
  "name": "architecture",
  "version": "1.0.0",
  "author": "BCAD Team",
  "description": "Architecture domain entities",
  "library": "libbcad-architecture-plugin",
  "requires": {
    "BCAD": "1.0.0"
  },
  "entry": "bcad_plugin_init"
}
```

**Avantage visé :** décrire un plugin sans le charger. Non obtenu aujourd'hui.

## 8. Mécanismes de chargement par OS

| OS | Mécanisme | API |
|----|-----------|-----|
| Linux | `dlopen` / `dlsym` / `dlclose` | `<dlfcn.h>` |
| Windows | `LoadLibrary` / `GetProcAddress` / `FreeLibrary` | `<windows.h>` |
| macOS | `dlopen` / `dlsym` / `dlclose` | `<dlfcn.h>` |

Implémentation réelle : les appels `dlopen`/`dlsym`/`dlclose` sont faits en
ligne dans `PluginManagerImpl` (`src/plugin/PluginManager.cpp`). Il n'existe pas
de classe `NativeLoader` — l'abstraction n'a pas été écrite, le code POSIX est
seul en service et les en-têtes Windows ne sont pas exercés par les CI locales.

## 9. Répertoires de plugins

Ce que l'hôte Qt explore réellement, dans cet ordre (`MainWindow`, et
`$BCAD_PLUGIN_PATH` en tête si défini) :

```
$BCAD_PLUGIN_PATH                         fichier OU répertoire
<applicationDir>/../lib/bcad/plugins      installation (make install)
<applicationDir>/../plugins               arbre de build
<applicationDir>/../../plugins            arbre de build : l'app est posée dans
                                          build/src/app, le module dans build/plugins
$XDG_DATA_HOME/bcad/plugins               données utilisateur, sinon
                                          ~/.local/share/bcad/plugins
```

Les chemins système hors l'arborescence de l'application — `/usr/lib/bcad/plugins`,
`/usr/local/lib/bcad/plugins`, `%APPDATA%\BCAD\plugins`,
`~/Library/Application Support/BCAD/plugins` — ne sont **pas** scannés : rien ne
les ajoute aux répertoires de recherche. Un packager qui voudrait ces emplacements
doit les passer à `addSearchDirectory()`, ou poser `$BCAD_PLUGIN_PATH`.

Un candidat est retenu si son nom ressemble à un module : extension `.so`/`.dll`/
`.dylib`, ou — dans l'arbre de build, où CMake pose le fichier **sans suffixe** —
nom commençant par `bcad_`. Les DSO de la plateforme BCAD (`libbcad_plugin`,
`bcad_plugin`) sont explicitement exclus : ce sont des modules de médiation hôtes,
pas des plugins.

### Répertoires de données des modules

Les gabarits réglables (§4) vivent ailleurs que les DSO, dans une arborescence
`share/` qui **reproduit la forme relative** de celle des modules, pour qu'un
même chemin de résolution marche en arbre de build et en installation :

```
$BCAD_PLUGIN_DATA                         en tête, prioritaire sur ce qui suit
<applicationDir>/../share/bcad/plugins    installation : bin/ côtoie share/
<applicationDir>/../../share/bcad/plugins arbre de build : build/src/app -> build/share
$XDG_DATA_HOME/bcad/plugins               données utilisateur, sinon
                                          ~/.local/share/bcad/plugins
```

Le module cadastral y trouve `cadastre/templates/*.json` ; CMake copie le
répertoire `templates/` du module vers `build/share/bcad/plugins/cadastre/` à
chaque build, et l'installation les pose au même endroit sous le préfixe.

## 10. Dépendances entre plugins (non implémenté)

`PluginInfo` ne porte que `name`, `version`, `description`, `author` et
`apiVersion` : **aucun champ `requiresPlugins`**, et le PluginManager ne trie
rien. Charger A avant B est aujourd'hui un effet de l'ordre de découverte
(déterministe, trié par chemin — cf. `discovery_test`), pas d'une résolution de
dépendances.

## 11. Règles

1. Plugin = bibliothèque dynamique
2. Symbole d'entrée : `bcad_plugin_init`
3. PluginInfo déclare les métadonnées
4. Le PluginManager gère le cycle de vie
5. Les plugins utilisent uniquement le SDK public
6. Les dépendances entre plugins sont déclaratives
7. Le déchargement est sûr (pas de ressources partagées non gérées)

## 12. Modules partagés entre hôte et plugin

Le partage de types repose sur une **délimitation nette** entre les modules
que l'hôte porte/exporte et le reste :

| Modules hôte (partagés) | Rôle | Exporté par l'hôte |
|--------------------------|------|--------------------|
| `bcad_geometry` | types (Point2, TypeId, Entity, entités) | vtables/typeinfo via `--export-dynamic-symbol` |
| `bcad_properties` | `PropertyMap` / vtables associées | idem |
| `bcad_core` | Document & entités d'infra | idem |
| `bcad_registry` | EntityRegistry (singleton hôte) | registre |
| `bcad_commands` | CommandRegistry (singleton hôte) | registre |
| `bcad_serialization` | SerializerRegistry (singleton hôte) | registre |

Modules **exclus** de l'interface plugin : `render/` (OpenGL, ADR-001), `io/`
(DXF/SQLite, services applicatifs), `app/` (Qt, ADR-009), `plugin/` (hôte).
Un plugin ne lie jamais ces modules.

**Conséquence pour l'hôte :** tout exécutable qui charge des plugins doit
1) embarquer **tous** les objets statiques des modules partagés
(`-Wl,--whole-archive`, sinon `Property.cpp.o` etc. sont retirés et le
`dlopen` échoue sur `undefined symbol: _ZTVN4bcad...`), et
2) **exporter** les symboles bcad ciblés (`ZM..` → fonctions, `ZTV` → vtables,
`ZTI/ZTS/ZGV` → RTTI). Le plugin, lui, **ne lie aucune** bibliothèque de types.

## 13. Contrat plugin (règles pour l'auteur d'un plugin)

1. **Ne pas définir de types en conflit.** Toujours utiliser les entités /
   classes des headers SDK (ce sont celles de l'hôte, partagées). Ne pas
   redéfinir de classes à vtables « locales » doublonnant une classe SDK —
   des weak symbols en double mènent à des `dynamic_cast`/`typeid` instables.
2. **Ne pas lier de bibliothèque de types** (`bcad_geometry`, `bcad_core`,
   `bcad_properties`, `bcad_registry`, `bcad_commands`, `bcad_serialization`)
   : ce serait une seconde copie des vtables/typeinfo. Lier **uniquement**
   `BCAD::bcad_plugin`.
3. **Passer des fonctions libres** (`&makeEntity`) ou des lambdas **sans
   capture** aux méthodes `registerEntityType`/`registerCommand` (pointeurs de
   fonction). Un lambda avec capture n'est pas compilable (ABI volontaire).
4. **Déclarer `bcad_plugin_api_version()`** renvoyant `PLUGIN_API_VERSION` :
   le plugin ne se charge que si sa version d'ABI est **strictement égale**
   à celle de l'hôte (gate dans PluginManager). Recompiler le plugin pour
   chaque version de BCAD (ADR-011).
5. **Même chaîne d'outils** que l'hôte (compilateur, libstdc++, standard,
   RTTI **activé**, exceptions compatibles) : les ABI C++ (mangling, layout,
   vtables) diffèrent entre GCC / Clang / MSVC.
6. **Ressources avant `dlclose`** : détruire avant `unloadPlugin` toute entité
   ou commande créée depuis les factories (le code vit dans le DSO du plugin
   déchargeable). Les registres hôte ne gardent que des closures hôte (cf. §4).
   Les **serializers**, les **workbenches**, les **validateurs** et les
   **exporteurs de fichier**, eux, sont
   retirés automatiquement par l'hôte au déchargement (ce sont les seules
   instances plugin stockées dans les registres hôtes).
7. **Ne pas compter sur un déchargement à la fin du processus.**
   `~PluginManager` ne dlclose **rien** : les registres qu'il devrait nettoyer
   sont des statiques de fonction dont l'ordre de destruction vis-à-vis du
   manager n'est pas défini, et les atteindre à ce moment-là appelait des
   méthodes d'objets détruits (SEGV à la fermeture de l'application, qui charge
   sans décharger). Un programme qui veut réellement décharger un module doit
   appeler `unloadPlugin()` pendant que ses registres sont vivants.
8. **Valeurs réglables : demandées, pas devinées.** Un module qui a besoin de
   constantes métier (motif d'identifiant, tolérance, profil) les lit par
   `resolveDataFile("<module>/…")` et garde un repli codé pour le cas où aucune
   donnée n'est installée. Il ne construit pas un chemin absolu à partir de
   `__FILE__`, du répertoire courant ou d'une variable d'environnement qu'il
   inventerait : c'est l'hôte qui annonce où sont les données (§9).

## 14. Versionnement de l'ABI plugin

- `PLUGIN_API_VERSION` (`include/bcad/plugin/PluginRegistry.h`) est **incrémenté
  à chaque cassure d'ABI** de l'interface plugin (v1 : factories
  `std::function` → v2 : pointeurs de fonction → v3 : extension UI
  `registerWorkbench` → v4 : extension de vérification `registerValidator` →
  v5 : extension d'export `registerFileExporter` → v6 : deux champs de plus sur
  `WorkbenchAction` → v7 : répertoires de données sur `PluginRegistry`).
  Contrôlé strictement au
  chargement (`pluginApiVersion != PLUGIN_API_VERSION` → refus).
- **Ajouter un champ n'est pas compatible.** Une structure ou une classe
  traversant la frontière change de layout, donc un plugin binaire compilé contre
  la version d'avant écrirait à côté. Les deux cas concrets : `WorkbenchAction` (v6) et
  `PluginRegistry` (v7).
- La bibliothèque hôte `libbcad_plugin` porte `VERSION ${PROJECT_VERSION}` et
  `SOVERSION ${PROJECT_VERSION_MAJOR}` (`src/plugin/CMakeLists.txt`) ; sa version
  **majeure** change à toute cassure ABI. (`BCAD_VERSION` n'existe pas :
  `project(bcad VERSION 1.0.0)` définit `PROJECT_VERSION_*`. Un `SOVERSION` vide
  faisait avaler à CMake le jeton `VERSION` suivant et installait le DSO sous le
  nom littéral `libbcad_plugin.so.VERSION`, donc aucune application installée ne
  démarrait.)
- SDK versionné via `BCADConfigVersion.cmake` (compatibilité
  `SameMajorVersion`, ADR-006) : `find_package(BCAD 1 REQUIRED)` accepte
  `1.x.y`.
- Politique de fond : ADR-011 — API source stable en version mineure, **aucun
  contrat ABI inter-versions** en v1 ; les plugins sont recompilés à chaque
  version de BCAD. Un wrapper C stable est visé en v2.