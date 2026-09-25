# Spécification — Module métier Cadastre

> Statut : **document d'exigences**, pas d'état d'avancement. Le module
> cadastral est en place (`src/plugins/cadastre/`, chargé par découverte) ;
> ce qui fonctionne vraiment se lit dans `CADASTRE_PLUGIN_STATUS.md` et
> l'audit `CADASTRAL_AUDIT_2026.md`.
> ADR concernées : ADR-005 (plugins), ADR-009 (commandes), ADR-002 (CGAL masqué),
> ADR-016 (aucune règle métier dans le core)

## 1. Objectif

Gérer des parcelles cadastrales dans BCAD : création, validation géométrique, opérations métier (division/fusion), persistance, affichage.

## 2. Entité `cadastre.parcel`

| Champ | Type | Obligatoire | Description |
|-------|------|-------------|-------------|
| `vertices` | `Point2[]` | oui | ≥3 sommets, polygone fermé |
| `section` | `string` | oui | Lettre(s) cadastrale(s), ex: "A", "AB" |
| `numero` | `string` | oui | Numéro parcellaire, ex: "42" |
| `contenance` | `string` | oui | Surface déclarée, ex: "500m²" |
| `commune` | `string` | non | Code/nom commune |
| `proprietaire` | `string` | non | Nom propriétaire |
| `nature` | `enum` | non | Nature culture (terre, pré, bois...) |

**TypeId** : `cadastre.parcel` (inconnu du Core, enregistré par le plugin)

**Validation** (`isValid`) :
- ≥3 sommets distincts
- Aire > 1e-9 (non colinéaire)
- Polygone simple (pas d'auto-intersection)
- `section` et `numero` non vides

**Format sérialisé** : `x,y;x,y;...|section|numero|contenance|commune`

## 3. Tâches

### Phase A — Géométrie & validation (FAIT)

- [x] `ParcelEntity : PolylineEntity` fermée + champs métier
- [x] Validation géométrique + métier
- [x] Factory `makeParcel(string_view)` + enregistrement `cadastre.parcel`
- [x] Serializer CSV

### Phase B — Commande création (FAIT)

- [x] `cadastre.create_parcel` : `execute` ajoute au Document, `undo` retire

### Phase C — Opérations métier

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| C1 | `split_parcel` | Diviser une parcelle par une polyligne de coupe | B |
| C2 | `merge_parcels` | Fusionner 2+ parcelles adjacentes | B |
| C3 | `subdivide` | Lotissement : découpe régulière en N lots | C1 |
| C4 | Calcul contenance | Aire géométrique → m²/ha, comparaison déclarée | A |

### Phase D — Persistance

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| D1 | SQLite | Table `cadastre_parcels` + requêtes | A |
| D2 | DXF | Calque `CADASTRE` + étiquettes | A |
| D3 | Import DXF | Lecture parcelles existantes (LWPOLYLINE → ParcelEntity) | D2 |

### Phase E — Données & référentiel

| # | Tâche | Description |
|---|-------|-------------|
| E1 | Unicité section+numéro | Contrainte + message si doublon |
| E2 | Propriétaire / indivision | Champ + UI édition |
| E3 | Nature / zonage | Enum + couleur par nature |

### Phase F — UI

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| F1 | Outil dessin | Clic → polygone parcelle, validation en live | A |
| F2 | Panneau propriétés | Édition section/numéro/contenance/commune | A |
| F3 | Étiquettes | Numéro + surface affichés sur carte | A |
| F4 | Recherche | Par `section+numéro` → sélection des parcelles correspondantes | A |
| F5 | Détection recouvrement | Highlight zones en conflit | C1,C2 |

> F4 est **fait** : `cadastre.find_parcel`, dans le module, atteinte par l'action
> « Rechercher une parcelle... » du workbench. La recherche **sélectionne** les
> parcelles ; elle ne zoome pas (l'hôte ne connaît pas de « zoom sur la
> sélection », et un module n'a rien à dire sur la caméra). La version perdue à
> la migration en module dynamique (`fcb2e24`, `cadastre::findByRef`) est
> rétablie et couverte par `cadastre_search_test`. F1, F2 et la détection de F5
> existent ; F3 (étiquettes centre + surface) non — les étiquettes ne vivent que
> sur la feuille imprimée.

## 4. Chaîne complète : plan parcellaire → mise en page → impression

> Objectif : produire un plan parcellaire imprimable (comme un géomètre).

### État actuel du socle

| Étape | Statut | Notes |
|-------|--------|-------|
| Dessin géométrique | ✅ | Points, lignes, polylignes, textes ; `cadastre.parcel` via module dynamique |
| Calques | ✅ | LayerManager, panneau « Calques » avec recherche |
| Rendu OpenGL | ✅ | GlRenderer + tessellation hors thread GL |
| DXF lecture/écriture | ✅ | DxfReader/DxfWriter (géométrie, pas les attributs cadastraux) |
| SQLite | ✅ | `.bcad` natif, attributs cadastraux inclus |
| Mise en page | ✅ | Feuille A4–A0, marges, plan à l'échelle standard, cartouche, flèche nord, barre d'échelle, tableau parcellaire et bornes **composés et imprimés** (`layout::composeSheet` + `layout::drawSheet`) |
| Cotation/étiquetage | ⚠️ | Cotations linéaire, alignée, angulaire, rayon, diamètre interactives ; étiquettes de parcelle centrées **sur la feuille imprimée**, pas dans le canevas ; pas de cotations cadastrales automatiques |
| Impression / PDF | ✅ | `QPrinter` en PDF vectoriel, aperçu avant impression |

Les phases G/H/I ci-dessous gardent leur numérotation d'origine : une ligne
`#GN`/`HN`/`IN` déjà réalisée n'y est pas retirée, et l'état réel se lit dans
`CADASTRE_PLUGIN_STATUS.md` et `CADASTRAL_AUDIT_2026.md`.

### Phase G — Mise en page (Layout)

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| G1 | Modèle de feuille | Formats A4/A3/A2/A1/A0, orientation portrait/paysage, marges | — |
| G2 | Cartouche | Bloc titre : commune, section, échelle, date, géomètre, légende | G1 |
| G3 | Échelle & carroyage | Échelles cadastrales (1:500, 1:1000, 1:2000...) + grille Lambert | G1 |
| G4 | Viewport plan | Fenêtre du dessin dans la feuille, zoom/pan indépendant | G1 |
| G5 | Tableau parcellaire | Liste parcelles (section, numéro, contenance, propriétaire) auto | A |

### Phase H — Cotation & annotation

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| H1 | Cotations linéaires | Distances entre bornes (côtés parcelle) | A |
| H2 | Cotations angulaires | Angles aux sommets | A |
| H3 | Étiquettes parcelle | Numéro + contenance au centre, échelle-dépendant | F3 |
| H4 | Bornes | Symboles + numérotation | A |
| H5 | Flèche Nord | Orientation | G3 |

### Phase I — Impression & export

| # | Tâche | Description | Dépend |
|---|-------|-------------|--------|
| I1 | Aperçu impression | QPrintPreviewDialog | G1 |
| I2 | Export PDF | QPrinter → PDF vectoriel (via rendu) | G1 |
| I3 | Export DXF complet | Calques `CADASTRE`/`COTATION`/`CARTOUCHE` + cartouche, **composés par le module** (l'hôte n'écrit que géométrie + XDATA générique) | D2,G2 |
| I4 | Export image | PNG/JPG haute résolution | G1 |

### Ordre d'implémentation (chaîne minimale viable)

```
G1 (feuille) → G4 (viewport) → H3 (étiquettes) → G2 (cartouche) → I2 (PDF)
     ↓
H1 (cotations) → G3 (échelle) → I1 (aperçu)
```

## 5. Critères d'acceptation

- Chaque opération métier est une `Command` annulable (`execute`/`undo`/`clone`)
- Aucune modif du Core : tout passe par `PluginRegistry` (ADR-005)
- Tests : `cadastre_external_test` couvre entité + commande + serializer + loader
- `check_arch.sh` reste vert
- Plan imprimable : DXF ou PDF généré contient parcelles + étiquettes + cartouche à l'échelle

## 6. Prochaine étape

La chaîne minimale viable (G1 → G4 → H3 → G2 → I2 → I1) est **atteinte** : la
feuille est composée et imprimée, l'aperçu et l'export partagent le même peintre.
Ce qui reste réellement ouvert, dans l'ordre où le route `ROADMAP_MARKET.md` :

1. **Formats d'échange branchés** — **fait** : `IFileExporter` est le sixième
   point d'extension, `GeoPackage cadastral` est enregistré par le module et
   reachable depuis `Fichier → Exporter` ; GeoJSON et CSV sont écrits par
   `src/io/Exchange.cpp` à partir du `PropertyMap`, donc sans littéral cadastral
   dans l'hôte. Détail dans `CADASTRE_PLUGIN_STATUS.md`.
2. **Gabarits JSON lus par le module** — **fait pour l'identification** : le
   module lit `section_pattern` / `number_pattern` dans
   `templates/cadastre_togo.json` (canal `resolveDataFile`, ABI v7). Restent non
   lus et assumés : styles de calque et de texte (faute de point d'extension de
   styles), `survey_tolerance` (faute de règle à alimenter) et `units` (faute de
   consommateur). Voir `CADASTRAL_AUDIT_2026.md`.
3. **I3 export DXF complet** — la géométrie part, les attributs cadastraux
   aussi (XDATA générique `BCAD_PROPS`, voir `IO_ARCHITECTURE.md` §4.1). Les
   calques `CADASTRE`/`COTATION`/`CARTOUCHE` et le cartouche ne sont pas écrits.
   L'hôte ne le fera pas : l'ancienne version écrivait des étiquettes à une
   échelle 1:500 et une hauteur de 2 mm imposées par le noyau, ce qui violait
   ADR-016 ; ce code est retiré et la composition d'un document d'export
   cadastral appartient au module.
4. **F4 recherche par section+numéro** — **fait**, cf. §3.
5. **I4 export image** et **G3 carroyage Lambert** : non commencés.
