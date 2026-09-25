# Feuille de route marché — BCAD pour l'Afrique de l'Ouest

> [!IMPORTANT]
>
> ## Statut : ORIENTATION VALIDÉE — exécution engagée
>
> Direction actée par le mainteneur le 2026-09-24 : construire au fur et à
> mesure, pour l'Afrique de l'Ouest, léger et adapté aux besoins locaux.
>
> Ce document arbitre **ce que BCAD construit, dans quel ordre et pourquoi**,
> à partir de l'inventaire des métiers (`PLUGIN_DOMAINS.md`) et des contraintes
> de la plateforme cible (`ADR-016`). Il n'est pas une description du code :
> l'état réel des fonctionnalités reste dans `CAHIER_DES_CHARGES.md`.

## 1. Réalités du terrain qui contraignent le produit

| Réalité | Exigence produit |
|---|---|
| Parc de postes ancien (mémoire limitée, GPU intégré, OS vieillissant ou Linux) | cible matérielle basse ; mode de rendu sans OpenGL à conserver ; binaire compact ; démarrage immédiat |
| Connexion internet instable, rare ou payante au volume | fonctionnement 100 % hors-ligne : aide, symboles, gabarits et notices embarqués, aucune dépendance réseau |
| Français, unités métriques (m², hectares), contexte administratif local | le livrable est un **document** (cartouche légal, tableau de surfaces, cadre de signature), pas un fichier de dessin |
| AutoCAD/DXF déjà présent dans les administrations et bureaux d'études | compatibilité DXF irréprochable (calques, XDATA, cotations) ; pont DWG par convertisseur externe |
| Formation CAO limitée, apprentissage « sur le tas » | une seule façon de faire chaque chose, raccourcis conformes AutoCAD, gabarits prêts à l'emploi, prise en main en moins d'une semaine |

## 2. Définition du produit

> **BCAD n'est pas un clone d'AutoCAD : c'est la machine à produire des
> dossiers techniques valables, hors-ligne, sur un poste modeste, aux normes
> du Togo et des pays voisins.**

Conséquence sur l'ordre des priorités. La **pyramide de valeur**, du plus
structurant au plus optionnel :

```
        [ 3D 2.5D / vues ]      aide à la décision, cubatures, coupes
       [ données attributaires ]  états, nomenclatures, métrés
      [ gabarits & normes locales ]  profils pays, cartouches, calques
    [ document livré ]  PDF vectoriel, mise en page, impression
  [ moteur géométrique ]  déjà en place, suffisant
```

Le moteur est la base, pas l'argument de vente. Ce qui différencie un outil
implantable localement : les trois étages du milieu.

## 3. Séquence de construction

Chaque étape doit livrer **un outil utilisable**, jamais un chantier ouvert.

| # | Étape | Contenu | Débloque | Effort |
|---|---|---|---|---|
| 0 | Verrouiller la migration cadastral | plugin dynamique, purge du code mort core, tests `parcelops`/`cadastre_io` | base saine | **FAIT** |
| 1 | **Workbench (lot A)** | `registerWorkbench()` ; menus et rubans générés depuis le plugin ; cadastre migré ; `check_arch.sh` interdit les littéraux métier dans `src/app/` | condition d'existence des étapes 6 et 7 : un plugin = zéro ligne de core | **FAIT** |
| 2 | **Étage « document »** | hachures, texte multi-lignes et styles de texte, cotations exportées en objets DXF `DIMENSION`, styles/flèches de cotation, **cartouche à champs pilotés par les attributs**, fiabilité d'impression A4→A0 | dossier administratif imprimable — valeur immédiate, sans nouveau plugin | moyen |
| 3 | **Gabarits Afrique** ⭐ | profils par pays : Togo (existant), Bénin, Côte d'Ivoire, Burkina Faso, Sénégal, Niger, Mali ; calques normalisés, styles de texte et de tracé, cartouches, lexique cadastral francophone, unités ; choix du pays à la création du document | **c'est le produit** : 100 % données JSON, quasi aucun C++, inexistant chez les concurrents | **faible** — **canal ouvert** : un module lit ses gabarits (`resolveDataFile`, `$BCAD_PLUGIN_DATA`), et le profil Togo pilote déjà les motifs d'identification. Les calques/styles/cartouches attendent que l'hôte sache les consommer (aucun point d'extension de styles) |
| 4 | **États et nomenclatures** | outil générique : sélection d'entités → tableau attributaire (surface, nature, propriétaire ; regards, câbles, portes) + totaux et métré → export PDF/CSV/tableur | la capacité réclamée par six familles de métiers (`PLUGIN_DOMAINS.md` §5) ; s'appuie sur `PropertyMap` | moyen |
| 5 | **2.5D à coût marginal** ⭐ | câbler la triangulation de Delaunay **déjà implémentée dans le moteur** : points cotés → MNT, courbes de niveau, pentes ; profils en long et en travers ; profondeur de pose des réseaux ; cubatures déblais/remblais | topographie, voirie, lotisseurs, carrières, irrigation | moyen (brique existante) |
| 6 | Plugin **réseaux** | nœuds et tronçons, chaînage/points kilométriques, carnet de regards, schéma unifilaire, détection de conflicts par profondeur | offices de l'eau et d'électricité, communes | élevé |
| 7 | Plugin **architecture simple** | murs, ouvertures, pièces par extrusion 2D + hauteur, coupes et façades générées, surfaces de plan | architectes, ateliers, plans de sécurité incendie (ERP) | élevé |
| 8 | **Éducation & robustesse** | mode exercice/TP, tutoriels embarqués hors-ligne, export SVG/PNG pour support de cours, paquet Windows autonome et AppImage Linux, optimisation de taille | adoption par les écoles techniques = pérennité de la base d'utilisateurs | faible→moyen |

⭐ = meilleurs rapports valeur/effort : **1 (obligatoire), 3 (presque gratuit
et unique), 5 (code déjà écrit)**.

## 4. Exclusion explicite du périmètre

Décisions prises pour préserver la légèreté et la capacité de livraison
incrémentale ; à réviser seulement sur argument marché, pas technique.

| Exclu | Motif | Révision |
|---|---|---|
| Noyau 3D solide (B-Rep), type Open CASCADE / CGAL 3D | second moteur, coût maximal, marché déjà couvert par FreeCAD en gratuit | si un client paying le demande pour de la pièce fabriquée |
| Solveur de contraintes et historique paramétrique | ajout d'architecture majeur, hors périmètre depuis l'origine | jamais pour la v1 |
| Scripting Python/embarqué dans le produit | complexité d'ABI et de distribution, besoin non démontré | après l'étape 6, si des règles de gestion l'exigent |
| Lecture DWG native | format propriétaire | convertisseur externe (ODA/LibreDWG) |
| Matériaux PBR, rendu temps réel, gisement de fonctionnalités « vue 3D » | hors cible (ADR-016) | non |
| Course au nombre de commandes AutoCAD | chaque fonction doit être complète (undo, propriétés, DXF, PDF, impression) plutôt que nombreuse | permanent |

## 5. Risques et garde-fous

1. **Fidélité DXF** — porte d'entrée des bureaux déjà équipés, et talon
   absolu : un aller-retour qui perd calques ou cotations disqualifie l'outil.
   *Garde-fou* : test d'acceptation sur un jeu de plans de référence du domaine
   cadastral, rejoué à chaque évolution de `bcad_io`.
2. **Surface d'extension UI non fermée** — tant que les menus d'un plugin sont
   écrits dans `MainWindow`, chaque plugin creuse la dette et l'étape 6/7
   devient intenable. *Garde-fou* : étapes 1 et durcissement de
   `scripts/check_arch.sh`.
3. **Cible matérielle jamais déclarée** — sans ADR-016, la dérive vers un
   outil gourmand est invisible jusqu'au premier poste client. *Garde-fou* :
   `ADR-016`, mesuré à chaque version.
4. **Dispersion des efforts** — seize métiers candidats, une équipe. *Garde-fou* :
   aucun nouveau plugin métier hors de la séquence §3 sans arbitrage explicite.

## 6. Indicateurs de réussite (par version)

- Démarrage et tracé fluide sur le poste de référence ADR-016.
- Un géomètre produit seul : plan cadastral + tableau de surfaces + cartouche
  conforme, exporté en PDF, **sans connexion**.
- Un aller-retour DXF avec un plan réel du domaine sans perte mesurable.
- Zéro littéral de nom métier (`"cadastre.`) dans `src/app/`.
- Nombre de lignes de core modifiées par nouveau plugin métier : **0**.

## 7. Références

- `ADR-016` — plateforme cible et principes de conception
- `PLUGIN_DOMAINS.md` — inventaire des métiers, briques communes, couches
- `WORKBENCH.md` — mécanisme d'extension UI (étape 1)
- `CADASTRE_SPEC.md`, `CADASTRE_PLUGIN_STATUS.md` — domaine réalisé
- `CAHIER_DES_CHARGES.md` — état et fonctionnalités du code
