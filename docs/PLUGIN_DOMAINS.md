# Domaines métiers extensibles de BCAD

> [!IMPORTANT]
>
> ## Statut : REFÉRENCE DE FEUILLE DE ROUTE — non implémenté
>
> Ce document cartographie les domaines métiers que BCAD doit pouvoir
> accueillir comme **plugins**, et fixe la **frontière entre briques
> communes (Core) et sémantique métier (plugin)**. Il ne décrit pas le code
> existant : hormis le cadastre (plugin `src/plugins/cadastre/`), aucun de
> ces domaines n'est implémenté. Il conditionne les chantiers décrits dans
> `WORKBENCH.md` (mécanisme d'extension UI) et `PLUGIN_ARCHITECTURE.md`
> (contrat ABI).

## 1. Objet et périmètre

`PLUGIN_ARCHITECTURE.md` répond au **comment** (dlopen, registres, ABI).
`WORKBENCH.md` répond au **comment UI** (menus, rubans, outils contributed).
Ce document répond au **quoi** : quels domaines, dans quel ordre, et surtout
**qu'est-ce qui est commun à tous** afin d'éviter de recoder le même moteur
dans chaque plugin.

Contrainte structurante héritée de `CAHIER_DES_CHARGES.md` : BCAD est **2D
par choix**. Les domaines crédibles sont donc ceux dont le métier tient en
**plan, tracé, surface, attributs**. Les domaines exigeant un moteur 3D ou
des formats d'échange 3D (IFC, STEP, CAO mécanique de pièce) sont hors
périmètre et ne doivent pas être listés comme candidats.

## 2. Frontière Core / plugin

La ligne de partage est ce qui distingue une **primitive géométrique
générique** d'une **règle métier**.

### 2.1 Briques communes (Core, connues de tous les domaines)

| Brique | État | Commentaire |
|---|---|---|
| Primitives et transformations 2D | FAIT | `bcad_geometry` |
| Opérations booléennes (union/intersection/différence) | FAIT | `geom::booleanOp` |
| Aire, périmètre, bbox, polygone simple | FAIT | `geom::polygonArea` |
| Calques, sélection, snaps, grille | FAIT | `bcad_layers`, `Viewport` |
| Commande/transaction, undo/redo | FAIT | ADR-009 |
| Persistance natif + DXF | FAIT | `bcad_io` |
| Mise en page, cartouche, PDF vectoriel | FAIT | `bcad_layout` |
| Cotations (linéaire, alignée, angulaire, rayon, diamètre) | FAIT | dans le Core ; à terme candidate à l'extraction en plugin (cf. §5) |
| **Triangulation (Delaunay / Delaunay contrainte)** | FAIT, NON CÂBLÉ | présent dans le moteur, aucune commande GUI |
| **SCIA / saisie COGO (azimut + distance)** | À REFAIRE | l'app l'avait (`include/bcad/app/ParcelTool.h` + `bcad/cadastre/Cogo.h`), supprimé par erreur lors de la migration ; **à rétablir en primitive générique**, pas sous un nom métier |
| **Operateurs polygonales de découpage** (`splitPolygon` par ligne, `subdividePolygon` en N, `bufferSegment`) | À PROMOUVOIR | aujourd'hui enfermés dans `src/plugins/cadastre/ParcelOps.cpp` alors qu'ils sont 100 % génériques (cf. §4) |
| **Formatage d'unités / surfaces** (m², ha, a) | À PROMOUVOIR | aujourd'hui `formatContenance()` dans le plugin cadastre |
| **Contrat de clés de propriétés communes** (`area`, `label`, `reference`, `description`…) | À DÉFINIR | sans quoi chaque plugin invente son préfixe et le panneau propriétés devient incohérent d'un domaine à l'autre |

### 2.2 Sémantique métier (plugin, jamais dans le Core)

- La **définition** de ce qu'est son entité (une parcelle ≠ un lot ≠ une
  pièce ≠ un tronçon), avec ses TypeIds et son propre format sérialisé.
- Ses **règles de validation** (réglementaires, tolérances, conventions
  professionnelles — ex. « plan cadastral national »).
- Ses **commandes** métier (scission, fusion, recalcul, generation d'état).
- Ses **templates** de profil, calques et styles (`*.json` installables).
- Sa **boîte à outils UI**, déclarée via `registerWorkbench()` (cible
  `WORKBENCH.md`).

### 2.3 Critère de bascule

Une fonctionnalité migre du plugin vers le Core dès qu'**un deuxième domaine
en a besoin**. Inversement, une règle propre à un seul domaine ne monte pas
dans le Core (ADR-013 : pas d'abstraction prématurée).

Indicateur faible à surveiller : dès qu'un domaine demande
« **attributs + surface + nomenclature + export tableau** », c'est la preuve
que ces briques sont communes et doivent être génériques.

## 3. Cartographie des domaines candidats

### 3.1 Géomatique et topographie (les plus proches du cadastre)

| Domaine | Entités typiques | Réutilise | Nouveauté à créer |
|---|---|---|---|
| **Cadastre** *(fait)* | parcelle, limite, borne, servitude | primitives polygonales, COGO, attributs | règles du plan cadastral national |
| **Topographie / relevé de terrain** | station, visée, point coté, courbe de niveau, talus | COGO, snaps, bornes | points cotés, interpolation, câblage du TIN |
| **Urbanisme / lotissement** | zone, emprise, recul, parcelle-mère | split / subdivide, surface | contrôle de conformité (protections, COS) |
| **Aménagement paysager** | végétal, surface verte, bassin | polygones, blocs, légende | inventaire et comptage végétal |

### 3.2 Réseaux et voirie (tracés et nomenclature)

| Domaine | Entités typiques | Réutilise | Nouveauté à créer |
|---|---|---|---|
| **VRD / voirie** | tronçon, profil type, point kilométrique, îlot, bordure | polylignes, offsets, surface | chaînage linéaire, numérotage PK |
| **Réseaux humides** (eau, assainissement) | conduite, regard, vanne | linéaire, nœuds, attributs | **topologie de graphe** (inexistante), calcul d'écoulement et connectivité |
| **Réseaux secs** (électricité, télécom, gaz) | poste, câble, chambre, branchement | linéaire, nœuds, attributs | nomenclature matériaux, plan de recollement |
| **Énergie / éclairage public** | candélabre, circuit, puissance | ponctuel + graphe | bilan électrique, schéma unifilaire |

### 3.3 Construction et bâtiment

| Domaine | Entités typiques | Réutilise | Nouveauté à créer |
|---|---|---|---|
| **Architecture** | mur, cloison, porte, fenêtre, pièce | COGO, snaps, `polygonArea` (pièce) | équerrage, assemblages (exige le lot C de `WORKBENCH.md`) |
| **Structure / génie civil** | poutre, poteau, semelle, ferraillage | linéaire, blocs, cotation | symbôles de ferraillage, tableau d'aciers |
| **Aménagement intérieur** | mobilier, équipement, circulation | blocs, surface | bibliothèque de symboles, calepinage |

### 3.4 Exploitation et documents

| Domaine | Entités typiques | Réutilise | Nouveauté à créer |
|---|---|---|---|
| **Cotation et annotation** *(transverse)* | cotation, texte, repère, nuage, tableau | existe déjà dans le Core | extraction en plugin exemplaire |
| **Cartographie / SIG** | raster géoréférencé, grille, projection | mise en page, cartouche | géoréférencement (Lambert, UTM), tuiles raster |
| **Plans de sûreté et accessibilité** (ERP) | issue, itinéraire, zone dégagée | linéaire, surface, symboles | vérifications réglementaires par calcul |
| **Archivage / GED** | document lié, révision, tampon | attributs, PDF | horodatage de révisions, index documentaire |

## 4. Problème identifié : le commun est aujourd'hui enfermé dans le cadastre

Le cœur de `src/plugins/cadastre/ParcelOps.cpp` est **générique** et ne
devrait pas vivre dans un plugin :

- `splitParcel(parcel, cutLine)` — « couper un polygone par une ligne » n'a
  rien de cadastral.
- `subdivideParcel(parcel, n, direction)` — « partager une surface en N
  parts » est la base du lotissement, de la voirie et du calepinage.
- `mergeParcels(a, b)` — simple wrapper sur l'union booléenne.
- `bufferLine(a, b, width)` — rectangle autours d'un segment, utilitaire
  pour tout tracé à largeur (murs, bordures, caniveaux).
- `formatContenance(areaM2)` — formatage m² / hectares, transversal.

Un second domaine (architecture, urbanisme, VRD) recopierait ces fonctions.
Le chantier de promotion dans `bcad_geometry` (noms génériques :
`splitPolygon`, `subdividePolygon`, `bufferSegment`) et dans un module
d'unités est donc **préalable ou parallèle** à l'arrivée du deuxième plugin
(voir l'ordre de passage §5, lot B).

## 5. Ordre de passage recommandé

| # | Chantier | Objectif | Coût |
|---|---|---|---|
| **A** | **Workbench** (lot A de `WORKBENCH.md`) | Mécanisme : `registerWorkbench()` + menu/rubans générés ; le cadastre migre dessus et sert de preuve | Moyen |
| **A'** | Durcir `scripts/check_arch.sh` | Interdire les littéraux `"cadastre.` dans `src/app/` (la vérification #10 actuelle, qui cherche `cadastre::ParcelEntity`, est aveugle à ce code) | Faible |
| **B** | **Promotion des briques communes** dans `bcad_geometry` + module d'unités, rétablissement du COGO | Le plugin cadastre devient *consommateur* de primitives du Core | Moyen |
| **C** | **Cotation extraite en plugin** | Premier vrai client du workbench : fonctionnalité existante à sortir du Core sans régression, exige les outils de canevas interactifs | Moyen+ |
| **D** | **Topographie / relevé** | Cousin du cadastre (~80 % de briques communes) : valide la cohabitation de deux plugins métiers | Élevé |
| **E** | **Architecture** | Le plus attendu, mais exigeant en équerrage et assemblages (lot C de `WORKBENCH.md`) | Élevé |
| **F** | **Réseaux (VRD, eau)** | Valorisation forte à l'export (plans de recollement) mais suppose la brique topologie de graphe, inexistante | Très élevé |

Le lot A est **orthogonal** au contenu : c'est le contenant (le mécanisme),
indépendamment du domaine. Le lot B est le vrai sujet soulevé par la
question « les métiers n'ont-ils pas des besoins communs ? » — oui, et il
faut les sortir des plugins avant que le deuxième ne les recopie.

## 6. Écueils à éviter

1. **Recopier `ParcelOps` dans chaque plugin** — cf. §4.
2. **Coder les menus d'un plugin dans `MainWindow`** — c'est le déficit
   actuel du cadastre (`src/app/MainWindow.cpp`, menu `&Cadastre`, panneau
   ruban, tests de TypeId en littéraux) ; le workbench doit le rendre
   impossible, pas seulement déconseillé.
3. **Inventer des clés de propriétés par domaine** — imposer le contrat
   commun (§2.1) pour que panneau propriétés et tableaux d'affichage restent
   cohérents.
4. **Faire monter une règle mono-domaine dans le Core** — violation
   d'ADR-003/ADR-005 et d'ADR-013.
5. **Accepter un domaine hors périmètre** (3D, IFC, solveur de contraintes)
   sans décision d'architecture — voir `CAHIER_DES_CHARGES.md` §4.

## 7. Références

- `WORKBENCH.md` — mécanisme d'extension UI cible (non implémenté)
- `PLUGIN_ARCHITECTURE.md` — contrat ABI, registres médiatisés, §13 règles auteur
- `CADASTRE_SPEC.md`, `CADASTRE_PLUGIN_STATUS.md` — domaine réalisé
- `CADASTRAL_AUDIT_2026.md` — audit fonctionnel
- `SDK_ARCHITECTURE.md`, `EXTENDING_BCAD.md` — surface publique et recettes
- `AGENTS.md` — règles architecturales non négociables
